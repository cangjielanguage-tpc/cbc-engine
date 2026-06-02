#include "statics_manager.h"
#include "field_layout.h"
#include "symlevel/definitions.h"
#include "symlevel/flags.h"
#include "symlevel/type_kind.h"
#include "terms.h"
#include "typeinfo_manager.h"
#include "utils/assertion.h"
#include "utils/math.h"
#include <algorithm>
#include <numeric>

#define RECORD_ALIGNMENT sizeof(uintptr_t)

namespace Engine {

StaticFieldsBundle::StaticFieldsBundle(
    uint32_t refFieldsNum,
    uint32_t primFieldsNum,
    uint32_t recordFieldsNum,
    uint32_t recordFieldsSize,
    std::vector<StaticTypedSlotInfo> typedSlotsInfo
)
    : refFieldsNum(refFieldsNum),
      primFieldsNum(primFieldsNum),
      recordFieldsNum(recordFieldsNum),
      typedSlotsInfo(std::move(typedSlotsInfo))
{
    ASSERTION(
        std::is_sorted(
            typedSlotsInfo.begin(),
            typedSlotsInfo.end(),
            [](const StaticTypedSlotInfo& a, const StaticTypedSlotInfo& b) { return a.offset < b.offset; }
        ),
        "Typed slots info must be sorted by offset"
    );

    size_t totalMemory = refFieldsNum * sizeof(RefLocation) + primFieldsNum * sizeof(PrimLocation) + recordFieldsSize;
    rawMemory.resize(totalMemory, 0);

    uint8_t* ptr   = rawMemory.data();
    refFieldsStart = reinterpret_cast<RefLocation*>(ptr);

    ptr             += refFieldsNum * sizeof(RefLocation);
    primFieldsStart  = reinterpret_cast<PrimLocation*>(ptr);

    ptr               += primFieldsNum * sizeof(PrimLocation);
    recordFieldsStart  = ptr;
}

SlotKind ComputeSlotKind(Session& session, FieldLayoutManager& flm, Symlevel::FieldDefinition& definition)
{
    auto fieldType = TermManager::Resolve(session, definition.FieldType());
    if (fieldType.GetKind() == TermKind::TYPE || fieldType.GetKind() == TermKind::AOT_REC) {
        auto typeDef = Symlevel::TypeDefinition::Resolve(session, TypeTermId(fieldType).GetIdentifier());
        if (typeDef.GetFlags().GetTypeKind() == Symlevel::TypeKind::RECORD) {
            return RECORD;
        }
    }
    if (fieldType.GetId().IsReference()) {
        return REFERENCE;
    }
    return PRIMITIVE;
}

uintptr_t StaticFieldsBundle::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    auto typeDef  = Symlevel::TypeDefinition::Resolve(session, typeIdent);
    auto fieldDef = Symlevel::FieldDefinition::Resolve(session, fieldIdent);

    auto flm                 = FieldLayoutManager::New(session);
    auto targetKind          = ComputeSlotKind(session, *flm, fieldDef);
    uint32_t untypedSlotIdx  = 0;
    uint32_t typedSlotOffset = 0;

    typeDef.GetFields().Find(session, [&](Symlevel::FieldDefinition& field) {
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            return false;
        }

        auto kind = ComputeSlotKind(session, *flm, field);
        if (targetKind != kind) {
            return false;
        }

        if (fieldIdent == field.Identifier()) {
            return true;
        }

        // advance state for next iteration
        switch (kind) {
            case REFERENCE:
            case PRIMITIVE: untypedSlotIdx++; break;
            case RECORD:    {
                auto fterm = TermManager::Resolve(session, field.FieldType());
                auto size  = flm->GetFlatSize(fterm);
                if (!size.has_value()) {
                    FATAL("Couldn't get size for record field: %s", fterm.GetName(session).c_str());
                }
                typedSlotOffset += size.value();
                break;
            }
        }

        return false;
    });

    switch (targetKind) {
        case REFERENCE:
            ASSERTION(untypedSlotIdx < refFieldsNum, "Incorrect static reference field index");
            return reinterpret_cast<uintptr_t>(refFieldsStart + untypedSlotIdx);
        case PRIMITIVE:
            ASSERTION(untypedSlotIdx < primFieldsNum, "Incorrect static primitive field index");
            return reinterpret_cast<uintptr_t>(primFieldsStart + untypedSlotIdx);
        case RECORD:
            ASSERTION(typedSlotOffset <= typedSlotsInfo.back().offset, "Incorrect static record offset");
            return reinterpret_cast<uintptr_t>(recordFieldsStart + typedSlotOffset);
        default: FATAL("Not supported yet"); return 0;
    }
}

void StaticFieldsBundle::VisitRefLocations(std::function<void(RefLocation*)> action) const
{
    RefLocation* location = refFieldsStart;
    for (auto i = 0; i < refFieldsNum; i++) {
        action(location);
        location++;
    }
}

void StaticFieldsBundle::VisitTypedSlots(std::function<void(uint8_t* base, const StaticTypedSlotInfo&)> action) const
{
    for (const auto& info : typedSlotsInfo) {
        action(recordFieldsStart, info);
    }
}

StaticFieldsBundle StaticsManager::CreateBundle(Session& session, TypeIdent typeIdent)
{
    uint32_t refFieldsNum    = 0;
    uint32_t primFieldsNum   = 0;
    uint32_t recordFieldsNum = 0;

    auto flm     = FieldLayoutManager::New(session);
    auto& tim    = TypeInfoManager::Of(session);
    auto typeDef = Symlevel::TypeDefinition::Resolve(session, typeIdent);

    uint32_t recordSlotsSize = 0;
    std::vector<StaticTypedSlotInfo> typedSlotsInfo;

    typeDef.GetFields().Find(session, [&](Symlevel::FieldDefinition& field) {
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            return false;
        }

        auto kind = ComputeSlotKind(session, *flm, field);
        switch (kind) {
            case REFERENCE: refFieldsNum++; break;
            case PRIMITIVE: primFieldsNum++; break;
            case RECORD:    {
                recordFieldsNum++;
                auto fterm    = TermManager::Resolve(session, field.FieldType());
                auto size     = flm->GetFlatSize(fterm);
                auto typeInfo = tim.AcquireTypeInfo(session, fterm);
                if (!size.has_value() || !typeInfo.has_value()) {
                    FATAL("Couldn't get info about record field: %s", fterm.GetName(session).c_str());
                }

                auto alignedSize = MathUtils::AlignUp(size.value(), RECORD_ALIGNMENT);

                typedSlotsInfo.push_back({ recordSlotsSize, typeInfo->Raw() });
                recordSlotsSize += alignedSize;
                break;
            }
        }

        return false;
    });

    return StaticFieldsBundle(refFieldsNum, primFieldsNum, recordFieldsNum, recordSlotsSize, std::move(typedSlotsInfo));
}

uintptr_t StaticsManager::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    std::lock_guard guard(lock);

    auto it = bundles.find(typeIdent.Pack());
    if (it == bundles.end()) {
        // move bundle, so the underlying vector won't be copied.
        bundles.try_emplace(typeIdent.Pack(), std::move(CreateBundle(session, typeIdent)));
        it = bundles.find(typeIdent.Pack());
    }

    return it->second.GetLocation(session, typeIdent, fieldIdent);
}

void StaticsManager::VisitRefLocations(
    std::function<void(RefLocation*)> untypedSlotsVisitor,
    std::function<void(uint8_t* base, const StaticTypedSlotInfo&)> typedSlotsVisitor
) const
{
    std::lock_guard guard(lock);

    for (const auto& [key, bundle] : bundles) {
        bundle.VisitRefLocations(untypedSlotsVisitor);
        bundle.VisitTypedSlots(typedSlotsVisitor);
    }
}

} // namespace Engine
