#include "statics_manager.h"
#include "symlevel/flags.h"
#include "symlevel/definitions.h"
#include "utils/assertion.h"

namespace Engine
{

StaticFieldsBundle::StaticFieldsBundle(uint32_t refFieldsNum, uint32_t primFieldsNum): refFieldsNum(refFieldsNum), primFieldsNum(primFieldsNum),
    rawMemory(refFieldsNum * sizeof(RefLocation) + primFieldsNum * sizeof(PrimLocation), 0)
{
    uint8_t* ptr = rawMemory.data();
    refFieldsStart = reinterpret_cast<RefLocation*>(ptr);

    ptr += refFieldsNum * sizeof(RefLocation);
    primFieldsStart = reinterpret_cast<PrimLocation*>(ptr);
}

FieldType ComputeFieldType(Symlevel::FieldDefinition& definition) {
    // TODO support records
    if (definition.FieldType().GetIdentifier().IsReference()) {
        return REFERENCE;
    } else {
        return PRIMITIVE;
    }
}

uintptr_t StaticFieldsBundle::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    auto typeDef  = Symlevel::TypeDefinition::Resolve(session, typeIdent);
    auto fieldDef = Symlevel::FieldDefinition::Resolve(session, fieldIdent);

    auto targetName = Symlevel::String::Parse(session, fieldDef.Identifier().GetFileId(), fieldDef.NameOffset());
    auto targetType = ComputeFieldType(fieldDef);
    uint32_t idx = 0;

    typeDef.GetFieldIndex().Foreach(session, [&idx, &targetName, &targetType, &session](Symlevel::FieldDefinition& field) {
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            return false;
        }

        if (targetType != ComputeFieldType(field)) {
            return false;
        }

        auto name = Symlevel::String::Parse(session, field.Identifier().GetFileId(), field.NameOffset());
        if (targetName.compare(name) == 0) {
            return true;
        }

        idx++;
        return false;
    });

    switch (targetType)
    {
        case REFERENCE:
            ASSERTION(idx < refFieldsNum, "Incorrect static reference field index"); 
            return reinterpret_cast<uintptr_t>(refFieldsStart + idx);
        case PRIMITIVE:
            ASSERTION(idx < primFieldsNum, "Incorrect static primitive field index"); 
            return reinterpret_cast<uintptr_t>(primFieldsStart + idx);
        default:
            FATAL("Not supported yet");
            return 0;
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
    uint32_t refFieldsNum = 0;
    uint32_t primFieldsNum = 0;

    auto typeDef = Symlevel::TypeDefinition::Resolve(session, typeIdent);
    typeDef.GetFieldIndex().Foreach(session, [&refFieldsNum, &primFieldsNum](Symlevel::FieldDefinition& field) {
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            return false;
        }

        if (field.FieldType().GetIdentifier().IsReference()) {
            refFieldsNum++;
        } else { 
            primFieldsNum++;
        }

        return false;
    });

    return StaticFieldsBundle(refFieldsNum, primFieldsNum);
}

uintptr_t StaticsManager::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    std::lock_guard guard(lock);

    auto it = bundles.find(typeIdent);
    if (it == bundles.end()) {
        it = bundles.emplace(typeIdent, CreateBundle(session, typeIdent)).first;
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