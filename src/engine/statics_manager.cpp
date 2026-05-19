#include "statics_manager.h"
#include "symlevel/definitions.h"
#include "symlevel/flags.h"
#include "utils/assertion.h"

namespace Engine {

StaticFieldsBundle::StaticFieldsBundle(uint32_t refFieldsNum, uint32_t primFieldsNum)
    : refFieldsNum(refFieldsNum),
      primFieldsNum(primFieldsNum),
      rawMemory(refFieldsNum * sizeof(RefLocation) + primFieldsNum * sizeof(PrimLocation), 0)
{
    uint8_t* ptr   = rawMemory.data();
    refFieldsStart = reinterpret_cast<RefLocation*>(ptr);

    ptr             += refFieldsNum * sizeof(RefLocation);
    primFieldsStart  = reinterpret_cast<PrimLocation*>(ptr);
}

SlotKind ComputeSlotKind(Session& session, Symlevel::FieldDefinition& definition)
{
    // TODO support records
    auto fieldType = TermManager::Resolve(session, definition.FieldType());
    if (fieldType.GetId().IsReference()) {
        return REFERENCE;
    } else {
        // Consider undefined term as primitive
        return PRIMITIVE;
    }
}

uintptr_t StaticFieldsBundle::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    auto typeDef  = Symlevel::TypeDefinition::Resolve(session, typeIdent);
    auto fieldDef = Symlevel::FieldDefinition::Resolve(session, fieldIdent);

    auto targetKind = ComputeSlotKind(session, fieldDef);
    uint32_t idx    = 0;

    typeDef.GetFields().Find(session, [&idx, &fieldIdent, &targetKind, &session](Symlevel::FieldDefinition& field) {
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            return false;
        }

        if (targetKind != ComputeSlotKind(session, field)) {
            return false;
        }

        if (fieldIdent == field.Identifier()) {
            return true;
        }

        idx++;
        return false;
    });

    switch (targetKind) {
        case REFERENCE:
            ASSERTION(idx < refFieldsNum, "Incorrect static reference field index");
            return reinterpret_cast<uintptr_t>(refFieldsStart + idx);
        case PRIMITIVE:
            ASSERTION(idx < primFieldsNum, "Incorrect static primitive field index");
            return reinterpret_cast<uintptr_t>(primFieldsStart + idx);
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

StaticFieldsBundle StaticsManager::CreateBundle(Session& session, TypeIdent typeIdent)
{
    // TODO support records
    uint32_t refFieldsNum  = 0;
    uint32_t primFieldsNum = 0;

    auto typeDef = Symlevel::TypeDefinition::Resolve(session, typeIdent);
    typeDef.GetFields().Find(session, [&](Symlevel::FieldDefinition& field) {
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            return false;
        }

        if (ComputeSlotKind(session, field) == REFERENCE) {
            refFieldsNum++;
        } else {
            primFieldsNum++;
        }

        return false;
    });

    // URVO guaranteed non-copy
    return StaticFieldsBundle(refFieldsNum, primFieldsNum);
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

void StaticsManager::VisitRefLocations(std::function<void(RefLocation*)> action) const
{
    std::lock_guard guard(lock);

    for (const auto& [key, group] : bundles) {
        group.VisitRefLocations(action);
    }
}

} // namespace Engine
