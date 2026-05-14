#include "field_layout.h"
#include "engine/engine.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "utils/assertion.h"
#include "utils/math.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Engine {

FieldLayout::FieldLayout(Content&& content) : content(std::make_shared<Content>(content)) {}

std::shared_ptr<FieldLayout::Content const> FieldLayout::operator->() const { return content; }

FieldLayout::Content const& FieldLayout::operator*() const { return *content; }

/// Fully local implementation of the field layout manager.
struct FLManager : public FieldLayoutManager {

    Session& session;
    std::unordered_map<Term, FieldLayout, Term::Hasher> cache;

    FLManager(Session& session) : session(session) {}

    std::optional<FieldLayout> GetLayout(Term term) override
    {
        if (term.GetKind() != TermKind::TYPE) {
            return std::nullopt;
        }
        // FIXME: recursion detection
        auto it = cache.find(term);
        if (it != cache.end()) {
            return it->second;
        }

        auto result = BuildLayout(term);
        if (result.has_value()) {
            cache.insert( {term, *result} );
        }
        return result;
    }

    /// The size of a field of given type and its alignment.
    std::optional<uint32_t> GetFlatSize(Term term) override
    {
        using TK = TermKind;
        switch (term.GetKind()) {
            case TK::VOID:
            case TK::UNIT: return 0;

            case TK::BOOLEAN:
            case TK::I8:
            case TK::U8:      return 1;

            case TK::I16:
            case TK::U16:
            case TK::F16: return 2;

            case TK::I32:
            case TK::U32:
            case TK::UCHAR32:
            case TK::F32:     return 4;

            case TK::I64:
            case TK::U64:
            case TK::IADDR:
            case TK::UADDR:
            case TK::BSTRING:
            case TK::F64:
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
                auto ti = TypeInfoManager::Of(session).AcquireTypeInfo(session, term);
                if (!ti.has_value()) {
                    return std::nullopt;
                }
                return RTSupport::MetaInfo::GetTypeSize(*ti);
            }

            case TK::TYPE_VAR: return std::nullopt;

            case TK::NIL:
            case TK::NOTHING:
            case TK::UNDEFINED:
            case TK::METHOD:
            case TK::GENERIC_METHOD:
            case TK::LAST:           return std::nullopt;
        }
    }

    /// The alignment of a field of given type and its alignment.
    uint8_t GetFlatAlignment(Term term) override
    {
        constexpr auto MAX_ALIGN = alignof(max_align_t);
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
                auto ti = TypeInfoManager::Of(session).AcquireTypeInfo(session, term);
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

    std::optional<FieldLayout> BuildLayout(Term term) {
        auto type = TypeTermId(term);
        auto def = Symlevel::Reader::Read(session, type.GetIdentifier());
        auto super = TermManager::Resolve(session, def.GetSuperType());

        FieldLayout::Content layout;

        if (super.GetKind() == TermKind::UNDEFINED) {
            return std::nullopt;
        } else if (super.GetKind() == TermKind::TYPE) {
            auto opt = GetLayout(super);
            if (!opt.has_value()) {
                return std::nullopt;
            }
            auto superLayout = opt.value();
            layout = *superLayout;
        } else {
            ASSERTION(term.GetKind() == TermKind::NIL, "only nil or type term kinds are expected for super");
        }

        auto& alignment = layout.desc.alignment;
        auto& size      = layout.desc.size;

        // FIXME: split table for instance and static fields in encoding.
        auto fields = def.GetInstanceFields();
        for (auto fieldId : fields.Values(session)) {
            auto def = Symlevel::Reader::Read(session, fieldId);
            // FIXME: substitution
            auto fieldType = TermManager::Resolve(session, def.FieldType());
            auto fieldSize      = GetFlatSize(fieldType);
            auto fieldAlignment = GetFlatAlignment(fieldType);

            std::optional<uint32_t> offset = std::nullopt;

            if (size.has_value()) {
                auto offset = MathUtils::AlignUp(*size, fieldAlignment);
                size        = offset;
            }
            if (size.has_value() && fieldSize.has_value()) {
                size = *size + *fieldSize;
            }
            alignment = std::max(alignment, fieldAlignment);

            layout.fields.emplace_back(FieldLayout::Entry {
                .definition = fieldId, .fieldType = fieldType, .offset = offset });
        }

        layout.desc.size      = size;
        layout.desc.alignment = alignment;

        return FieldLayout(std::move(layout));
    }
};

std::unique_ptr<FieldLayoutManager> FieldLayoutManager::Of(Session& session)
{
    return std::make_unique<FLManager>(session);
}

FieldLayoutManager::~FieldLayoutManager() = default;

} // namespace Engine
