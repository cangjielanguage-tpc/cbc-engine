#include "field_layout.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
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
#include <string_view>

static constexpr auto MAX_ALIGN = alignof(max_align_t);
static constexpr std::string_view NW_LONG_LINK_VALUE_TYPE = "nw_long_link.protocol_manager:ValueType";

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

        if (term.GetKind() == TermKind::BOX) {
            term = term.Subterm(0);
        }

        std::optional<FieldLayout> layout = std::nullopt;

        if (auto it = cache.find(term); it != cache.end()) {
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

    /// The size of a field of given type.
    std::optional<uint32_t> GetFlatSize(Term term) override
    {
        using TK = TermKind;
        if (term.IsReference()) {
            return sizeof(void*);
        }
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
            case TK::F64: return 8;

            case TK::IADDR:
            case TK::UADDR:
            case TK::BSTRING:
            case TK::C_POINTER:
            case TK::NULLABLE:
            case TK::FUNCTIONAL:
            case TK::NON_NULLABLE: return sizeof(void*);

            case TK::PRIMITIVE_ENUM: {
                auto id   = PrimitiveEnumId(term.GetId());
                auto def  = Symlevel::Reader::Read(session, id.GetIdentifier());
                auto term = TermManager::Resolve(session, def.GetEnumType());
                return GetFlatSize(term);
            }

            case TK::TYPE:
            case TK::UNION_ENUM:
            case TK::OPTION:     return GetCbcFlatSize(term);

            case TK::TUPLE:
            case TK::AOT_TYPE: return GetAotFlatSize(term);

            case TK::FUNC_TYPE_VAR:
            case TK::CLASS_TYPE_VAR: return std::nullopt;

            case TK::NIL:
            case TK::NOTHING:
            case TK::UNDEFINED: return std::nullopt;

            default: {
                FATAL("Unexpected kind %d", term.GetKind());
            }
        }
    }

    /// The alignment of a field of given type.
    uint8_t GetFlatAlignment(Term term) override
    {
        if (term.IsReference()) {
            return sizeof(void*);
        }
        switch (term.GetKind()) {
            case TermKind::PRIMITIVE_ENUM: {
                auto id   = PrimitiveEnumId(term.GetId());
                auto def  = Symlevel::Reader::Read(session, id.GetIdentifier());
                auto term = TermManager::Resolve(session, def.GetEnumType());
                return GetFlatAlignment(term);
            }
            case TermKind::TUPLE:
            case TermKind::OPTION:
            case TermKind::UNION_ENUM:
            case TermKind::TYPE: {
                auto optlayout = GetLayout(term);
                if (optlayout.has_value()) {
                    auto layout = *optlayout;
                    return layout->desc.alignment;
                }
                return MAX_ALIGN;
            }
            case TermKind::AOT_TYPE: {
                auto ti = typeInfoManager.AcquireTypeInfo(session, term);
                if (!ti.has_value()) {
                    return MAX_ALIGN;
                }
                return RTSupport::MetaInfo::GetAlign(*ti);
            }
            case TermKind::C_POINTER:
            case TermKind::NULLABLE:
            case TermKind::FUNCTIONAL:
            case TermKind::NON_NULLABLE: return sizeof(void*);

            default: {
                // for primitives types
                // alignment is the same as the size.
                ASSERTION(
                    static_cast<uint16_t>(term.GetKind()) < FIRST_NON_PRIMITIVE,
                    "only primitive term kinds are expected here"
                );
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
            return;
        }
        switch (term.GetKind()) {
            case TermKind::AOT_TYPE: {
                auto typeInfo = typeInfoManager.AcquireTypeInfo(session, term);
                if (!typeInfo.has_value()) {
                    return;
                }

                typeInfo->VisitReferenceOffsets([&offsets, disp](uint32_t offset) { offsets.push_back(offset + disp); }
                );
                return;
            }

            case TermKind::TUPLE:
            case TermKind::OPTION:
            case TermKind::UNION_ENUM:
            case TermKind::TYPE: {
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
            default: {
                /* pass */
            }
        }
    }

private:
    std::optional<uint32_t> GetAotFlatSize(Term term)
    {
        if (term.IsReference())
            return sizeof(void*);
        auto ti = typeInfoManager.AcquireTypeInfo(session, term);
        if (!ti.has_value()) {
            return std::nullopt;
        }
        return RTSupport::MetaInfo::GetTypeSize(*ti);
    }

    std::optional<uint32_t> GetCbcFlatSize(Term term)
    {
        if (term.IsReference()) {
            return sizeof(void*);
        }
        auto optlayout = GetLayout(term);
        if (optlayout.has_value()) {
            auto layout = *optlayout;
            return layout->desc.size;
        }
        return std::nullopt;
    }

    struct SizeAlignmentAccumulator {
        FLManager* manager;
        std::optional<uint32_t> size;
        uint8_t alignment;

        void AddField(
            std::vector<FieldLayout::Entry>& entries,
            Term type,
            std::optional<Identifier<Symlevel::FieldDefinition>> fdef
        )
        {
            auto offset = AddField(manager->GetFlatSize(type), manager->GetFlatAlignment(type));
            entries.emplace_back(FieldLayout::Entry { .definition = fdef, .fieldType = type, .offset = offset });
        }

        std::optional<uint32_t> AddField(std::optional<uint32_t> fieldSize, uint8_t fieldAlignment)
        {
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
                size      = std::nullopt;
                alignment = MAX_ALIGN;
            }
            alignment = std::max(alignment, fieldAlignment);
            return offset;
        }
    };

    std::optional<FieldLayout> BuildLayout(Term term)
    {
        Log::fields.Log(Logging::Level::INFO, [&](Stream::Output& out_) {
            Stream::ResolvingOutput out(session, out_);
            out << "starting to build layout for " << term << Stream::endl;
        });

        auto kind = term.GetKind();

        std::optional<FieldLayout> layout {};

        if (kind == TermKind::TYPE) {
            auto def = Symlevel::Reader::Read(session, ExtractTypeDefIdentifier(term));
            layout = BuildLayoutCbc(term, def);
        } else if (kind == TermKind::TUPLE) {
            SizeAlignmentAccumulator acc { this, 0, 1 };
            FieldLayout::Content content;
            auto len = term.GetLength();
            for (int i = 0; i < len; i++) {
                acc.AddField(content.fields, term.Subterm(i), std::nullopt);
            }
            content.desc.alignment = acc.alignment;
            content.desc.size      = acc.size;
            layout                 = std::move(content);
        } else if (kind == TermKind::OPTION && !term.IsReference()) {
            ClassSubstitution substitute(session, term);
            SizeAlignmentAccumulator acc { this, 0, 1 };
            FieldLayout::Content content;
            acc.AddField(content.fields, Term::Predefined(TermKind::BOOLEAN), std::nullopt);

            auto def      = Symlevel::Reader::Read(session, ExtractTypeDefIdentifier(term));
            auto someType = TermManager::Resolve(session, def.GetEnumType());
            someType = substitute.Substitute(someType);
            acc.AddField(content.fields, someType, std::nullopt);

            content.desc.alignment = acc.alignment;
            content.desc.size      = acc.size;
            layout = std::move(content);
        } else if (kind == TermKind::OPTION && term.IsReference()) {
            ClassSubstitution substitute(session, term);
            SizeAlignmentAccumulator acc { this, 0, 1 };
            FieldLayout::Content content;
            acc.AddField(content.fields, Term::Predefined(TermKind::UNIT), std::nullopt);

            auto def      = Symlevel::Reader::Read(session, ExtractTypeDefIdentifier(term));
            auto someType = TermManager::Resolve(session, def.GetEnumType());
            someType      = substitute.Substitute(someType);
            acc.AddField(content.fields, someType, std::nullopt);

            content.desc.alignment = acc.alignment;
            content.desc.size      = acc.size;
            layout                 = std::move(content);
        } else if (kind == TermKind::PRIMITIVE_ENUM) {
            ClassSubstitution substitute(session, term);
            SizeAlignmentAccumulator acc { this, 0, 1 };
            FieldLayout::Content content;

            auto def      = Symlevel::Reader::Read(session, ExtractTypeDefIdentifier(term));
            auto someType = TermManager::Resolve(session, def.GetEnumType());
            someType      = substitute.Substitute(someType);
            acc.AddField(content.fields, someType, std::nullopt);

            content.desc.alignment = acc.alignment;
            content.desc.size      = acc.size;
            layout = std::move(content);
        } else if (kind == TermKind::UNION_ENUM) {
            ClassSubstitution substitute(session, term);
            auto def = Symlevel::Reader::Read(session, ExtractTypeDefIdentifier(term));

            // Compatibility workaround for nw_long_link's ValueType. CBC currently classifies this enum as a
            // packed union even though String contains a managed reference. The generated bytecode nevertheless
            // uses the naturally aligned constructor offsets (tag at 0, String at 8), so use a conservative
            // uniform record large enough for the String constructor. The reference slot at offset 8 is also
            // used for all constructors; PREP_TYPED and zero-initialized heap storage keep it null for the numeric
            // constructors.
            auto typeName = Symlevel::Reader::Read(session, def.GetName());
            if (typeName == NW_LONG_LINK_VALUE_TYPE) {
                std::optional<Term> stringCtor;
                for (auto ctor : def->unionFields.Values(session)) {
                    stringCtor = substitute.Substitute(TermManager::Resolve(session, ctor));
                    break;
                }
                if (!stringCtor.has_value()) {
                    return std::nullopt;
                }

                if (stringCtor->GetKind() != TermKind::TUPLE || stringCtor->GetLength() != 1) {
                    return std::nullopt;
                }

                auto stringType = stringCtor->Subterm(0);
                FieldLayout::Content content {
                    .fields = {
                        { .definition = std::nullopt,
                          .fieldType  = Term::Predefined(TermKind::I32),
                          .offset     = 0 },
                        { .definition = std::nullopt, .fieldType = stringType, .offset = 8 },
                    },
                    .desc = { .size = 24, .alignment = 8 },
                };
                layout = std::move(content);
            } else {
                // for some reason CJNative packs their enums tightly
                uint8_t alignment = 1;

                bool failed   = false;
                uint32_t size = 0;
                for (auto fieldTypeId : def->unionFields.Values(session)) {
                    auto fieldType = TermManager::Resolve(session, fieldTypeId);
                    fieldType      = substitute.Substitute(fieldType);
                    auto fieldSize = GetFlatSize(fieldType);
                    if (!fieldSize) {
                        failed = true;
                        break;
                    }
                    size = std::max(size, *fieldSize);
                }
                if (failed) {
                    layout = std::nullopt;
                } else {
                    layout = FieldLayout::Content { .desc = { size, alignment } };
                }
            }
        } else if (kind == TermKind::AOT_TYPE) {
            FATAL("Unreachable");
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

        SizeAlignmentAccumulator acc { this, layout.desc.size, layout.desc.alignment };

        for (auto fieldId : def.GetInstanceFields().Values(session)) {
            auto def       = Symlevel::Reader::Read(session, fieldId);
            auto fieldType = TermManager::Resolve(session, def.FieldType());
            fieldType      = substitute(fieldType);
            acc.AddField(layout.fields, fieldType, fieldId);
        }

        layout.desc.size      = acc.size;
        layout.desc.alignment = acc.alignment;

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
        } else if (def.GetFlags().Is(Symlevel::TypeKind::RECORD)) {
            return FieldLayout::Content();
        } else if (super.GetKind() == TermKind::TYPE) {
            auto opt = GetLayout(substitute(super));
            if (!opt.has_value()) {
                return std::nullopt;
            }
            return *opt.value(); // copy
        } else {
            switch (super.GetKind()) {
                case TermKind::NIL:
                case TermKind::FUNCTIONAL: break;
                default:                   ASSERTION(false, "only nil or type term kinds are expected for super");
            }

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
