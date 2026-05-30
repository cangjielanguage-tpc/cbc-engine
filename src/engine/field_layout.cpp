#include "field_layout.h"
#include "engine/engine.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/type_kind.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

static constexpr auto MAX_ALIGN = alignof(max_align_t);

namespace Engine {

FieldLayout::FieldLayout(Content&& content) : content(std::make_shared<Content>(content)) {}

std::shared_ptr<FieldLayout::Content const> FieldLayout::operator->() const { return content; }

FieldLayout::Content const& FieldLayout::operator*() const { return *content; }

/// Fully local implementation of the field layout manager.
struct FLManager : public FieldLayoutManager {
    Session& session;
    TypeInfoManager& typeInfoManager;
    std::unordered_map<Term, FieldLayout, Term::Hasher> cache;

    FLManager(Session& session, TypeInfoManager& typeInfoManager) : session(session), typeInfoManager(typeInfoManager)
    {}

    std::optional<FieldLayout> GetLayout(Term term) override
    {
        Log::fields.Log(Logging::Level::INFO, [&](Stream::Output& out_) {
            Stream::ResolvingOutput out(session, out_);
            out << "requested field layout for " << term << Stream::endl;
        });
        std::optional<FieldLayout> layout = std::nullopt;
        if (term.GetKind() != TermKind::TYPE) {
            return std::nullopt;
        } else if (auto it = cache.find(term); it != cache.end()) {
            // FIXME: recursion detection
            return it->second;
        } else {
            auto layout = BuildLayout(term);
            if (layout.has_value()) {
                cache.insert({ term, *layout });
            }
            return layout;
        }
    }

    /// The size of a field of given type and its alignment.
    std::optional<uint32_t> GetFlatSize(Term term) override
    {
        using TK = TermKind;
        switch (term.GetKind()) {
            case TK::VOID:
            case TK::UNIT: return 0;

            case TK::I8:
            case TK::U8:
            case TK::BOOLEAN: return 1;

            case TK::I16:
            case TK::U16:
            case TK::F16: return 2;

            case TK::I32:
            case TK::U32:
            case TK::F32:
            case TK::UCHAR32: return 4;

            case TK::I64:
            case TK::U64:
            case TK::F64:
            case TK::IADDR:
            case TK::UADDR:
            case TK::BSTRING:
            case TK::C_POINTER: return 8;

            case TK::AOT_TYPE:
            case TK::NULLABLE:
            case TK::NON_NULLABLE:
            case TK::CANGJIE_ARRAY: return sizeof(void*);

            case TK::TYPE: {
                auto ident = TypeTermId(term).GetIdentifier();
                auto kind  = Symlevel::TypeDefinition::Resolve(session, ident).GetFlags().GetTypeKind();
                if (kind != Symlevel::TypeKind::RECORD) {
                    return sizeof(void*);
                }
                auto optlayout = GetLayout(term);
                if (optlayout.has_value()) {
                    auto layout = *optlayout;
                    return layout->desc.size;
                }
                return std::nullopt;
            }

            case TK::AOT_REC: {
                auto ti = typeInfoManager.AcquireTypeInfo(session, term);
                if (!ti.has_value()) {
                    return std::nullopt;
                }
                return RTSupport::MetaInfo::GetTypeSize(*ti);
            }

            case TK::FUNC_TYPE_VAR:
            case TK::CLASS_TYPE_VAR: return std::nullopt;

            case TK::NIL:
            case TK::NOTHING:
            case TK::UNDEFINED:
            case TK::METHOD:
            case TK::GENERIC_METHOD:
            case TK::LAST:           return std::nullopt;

            default: {
                FATAL("Unexpected kind %d", term.GetKind());
            }
        }
    }

    /// The alignment of a field of given type and its alignment.
    uint8_t GetFlatAlignment(Term term) override
    {
        switch (term.GetKind()) {
            case TermKind::TYPE: {
                auto ident = TypeTermId(term).GetIdentifier();
                auto kind  = Symlevel::TypeDefinition::Resolve(session, ident).GetFlags().GetTypeKind();
                if (kind != Symlevel::TypeKind::RECORD) {
                    return sizeof(void*);
                }
                auto optlayout = GetLayout(term);
                if (optlayout.has_value()) {
                    auto layout = *optlayout;
                    return layout->desc.alignment;
                }
                return MAX_ALIGN;
            }
            case TermKind::AOT_REC: {
                auto ti = typeInfoManager.AcquireTypeInfo(session, term);
                if (!ti.has_value()) {
                    return MAX_ALIGN;
                }
                return RTSupport::MetaInfo::GetAlign(*ti);
            }

            default: {
                // for primitives and reference types
                // alignment is the same as the size.
                auto size = GetFlatSize(term);
                if (size.has_value()) {
                    return std::max(size.value(), 1u);
                }
                return MAX_ALIGN;
            }
        }
    }

    void FillRefOffsets(Term term, std::vector<uint32_t>& offsets, uint32_t disp) override
    {
        ASSERT(!term.IsGeneric());
        if (term.IsReference()) {
            offsets.push_back(disp);
        } else if (term.GetKind() == TermKind::AOT_REC) {
            auto typeInfo = typeInfoManager.AcquireTypeInfo(session, term);
            if (!typeInfo.has_value()) {
                return;
            }
            RTSupport::Execution::VisitReferences(typeInfo.value(), [&offsets, disp](uint32_t offset) {
                 offsets.push_back(offset + disp);
            });
        } else if (term.GetKind() == TermKind::TYPE) {
            ASSERT(!term.IsReference());
            // Absent offsets must be handled separately.
            // Here we will just ignore possible errors.
            auto optlayout = GetLayout(term);
            if (!optlayout.has_value()) {
                return;
            }
            for (auto& field : (**optlayout).fields) {
                if (!field.offset.has_value()) {
                    return;
                }
                auto offset = *field.offset + disp;
                FillRefOffsets(field.fieldType, offsets, offset);
            }
        }
    }

private:
    std::optional<FieldLayout> BuildLayout(Term term)
    {
        Log::fields.Log(Logging::Level::INFO, [&](Stream::Output& out_) {
            Stream::ResolvingOutput out(session, out_);
            out << "starting to build layout for " << term << Stream::endl;
        });
        auto type = TypeTermId(term);
        auto def  = Symlevel::Reader::Read(session, type.GetIdentifier());

        std::optional<FieldLayout> layout {};

        if (def.GetFlags().Is(Symlevel::TypeFlag::AOT)) {
            layout = BuildLayoutAot(term, def);
        } else {
            layout = BuildLayoutCbc(term, def);
        }

        Log::fields.Log(Logging::Level::DEBUG, [&](Stream::Output& out_) {
            Stream::ResolvingOutput out(session, out_);
            if (layout.has_value()) {
                out << term << " " << *layout << Stream::endl;
            } else {
                out << "failed to build layout for " << term << Stream::endl;
            }
        });
        return layout;
    }

    std::optional<FieldLayout> BuildLayoutAot(Term term, Symlevel::TypeDefinition& def)
    {
        ClassSubstitution substitute(session, term);
        auto optlayout = GetTypeBaseLayout(substitute, def);
        if (!optlayout.has_value()) {
            return std::nullopt;
        }
        FieldLayout::Content layout(std::move(*optlayout));

        if (term.IsGeneric()) {
            // We can not properly query offsets of generic aot type.
            layout.desc.size      = std::nullopt;
            layout.desc.alignment = MAX_ALIGN;

            size_t ordinal = layout.fields.size();
            for (auto fieldId : def.GetInstanceFields().Values(session)) {
                auto def       = Symlevel::Reader::Read(session, fieldId);
                auto fieldType = TermManager::Resolve(session, def.FieldType());
                fieldType      = substitute(fieldType);

                layout.fields.emplace_back(FieldLayout::Entry {
                    .definition = fieldId, .fieldType = fieldType, .offset = std::nullopt });
                ordinal++;
            }
            return layout;
        }

        // Concrete term path.
        auto typeInfo = typeInfoManager.AcquireTypeInfo(session, term);

        if (!typeInfo.has_value()) {
            return std::nullopt;
        }

        size_t ordinal = layout.fields.size();
        for (auto fieldId : def.GetInstanceFields().Values(session)) {
            auto def = Symlevel::Reader::Read(session, fieldId);
            // FIXME: substitution
            auto fieldType = TermManager::Resolve(session, def.FieldType());
            fieldType      = substitute(fieldType);

            auto offset = RTSupport::Execution::GetFieldOffset(*typeInfo, ordinal, false);
            layout.fields.emplace_back(FieldLayout::Entry {
                .definition = fieldId, .fieldType = fieldType, .offset = offset });
            ordinal++;
        }

        layout.desc.size      = RTSupport::MetaInfo::GetTypeSize(*typeInfo);
        layout.desc.alignment = RTSupport::MetaInfo::GetAlign(*typeInfo);

        return layout;
    }

    std::optional<FieldLayout> BuildLayoutCbc(Term term, Symlevel::TypeDefinition& def)
    {
        ClassSubstitution substitute(session, term);
        auto optlayout = GetTypeBaseLayout(substitute, def);
        if (!optlayout.has_value()) {
            return std::nullopt;
        }
        FieldLayout::Content layout(std::move(*optlayout));

        auto& alignment = layout.desc.alignment;
        auto& size      = layout.desc.size;

        for (auto fieldId : def.GetInstanceFields().Values(session)) {
            auto def            = Symlevel::Reader::Read(session, fieldId);
            auto fieldType      = TermManager::Resolve(session, def.FieldType());
            fieldType           = substitute(fieldType);
            auto fieldSize      = GetFlatSize(fieldType);
            auto fieldAlignment = GetFlatAlignment(fieldType);

            std::optional<uint32_t> offset = std::nullopt;

            if (size.has_value()) {
                auto offs = MathUtils::AlignUp(*size, fieldAlignment);
                size      = offs;
                offset    = offs;
                if (fieldSize.has_value()) {
                    size = *size + *fieldSize;
                }
            }
            if (!fieldSize.has_value()) {
                Log::fields.Log(Logging::Level::ERROR, [&](Stream::Output& out_) {
                    Stream::ResolvingOutput out(session, out_);
                    out << "failed to obtain field size " << def << Stream::endl;
                });
                size      = std::nullopt;
                alignment = MAX_ALIGN;
            }
            alignment = std::max(alignment, fieldAlignment);

            layout.fields.emplace_back(FieldLayout::Entry {
                .definition = fieldId, .fieldType = fieldType, .offset = offset });
        }

        layout.desc.size      = size;
        layout.desc.alignment = alignment;

        return FieldLayout(std::move(layout));
    }

    /// Layout of the super type for classes or empty layout for records.
    std::optional<FieldLayout::Content> GetTypeBaseLayout(ClassSubstitution& substitute, Symlevel::TypeDefinition& def)
    {
        auto super = TermManager::Resolve(session, def.GetSuperType());
        if (super.GetKind() == TermKind::UNDEFINED) {
            return std::nullopt;
        } else if (def.GetFlags().Is(Symlevel::TypeKind::RECORD)) {
            return FieldLayout::Content();
        } else if (super.GetKind() == TermKind::TYPE) {
            auto opt = GetLayout(substitute(super));
            if (!opt.has_value()) {
                return std::nullopt;
            }
            return *opt.value(); // copy
        } else {
            ASSERTION(super.GetKind() == TermKind::NIL, "only nil or type term kinds are expected for super");

            FieldLayout::Content base;
            base.desc.alignment = sizeof(void*);

            return base;
        }
    }
};

std::unique_ptr<FieldLayoutManager> FieldLayoutManager::New(Session& session)
{
    return std::make_unique<FLManager>(session, TypeInfoManager::Of(session));
}

std::unique_ptr<FieldLayoutManager> FieldLayoutManager::New(Session& session, TypeInfoManager& typeInfoManager)
{
    return std::make_unique<FLManager>(session, typeInfoManager);
}

FieldLayoutManager::~FieldLayoutManager() = default;

static Stream::Descripted stream(Stream::cerr, "[FL] ");
Logging::Logger Log::fields(&stream, Logging::Level::ERROR);
} // namespace Engine
