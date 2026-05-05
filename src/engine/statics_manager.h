#pragma once

#include "arena.h"
#include "engine.h"
#include "identifiers.h"
#include "symlevel/definitions.h"

#include <functional>
#include <mutex>

namespace Engine {

struct RefLocation {
    uintptr_t reference;
};

// TODO segregate by sizes
struct PrimLocation {
    uint64_t value;
};

enum SlotKind {
    PRIMITIVE,
    REFERENCE,
    RECORD
};

class StaticFieldsBundle {
    using TypeIdent = struct Identifier<Symlevel::TypeDefinition>;
    using FieldIdent = struct Identifier<Symlevel::FieldDefinition>;

public:
    StaticFieldsBundle(uint32_t refFieldsNum, uint32_t primFieldsNum);

    ~StaticFieldsBundle() = default;

    uintptr_t GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent);

    void VisitRefLocations(std::function<void(RefLocation*)> action) const;

private:
    RefLocation* refFieldsStart;
    uint32_t refFieldsNum;
    PrimLocation* primFieldsStart;
    uint32_t primFieldsNum;

    std::vector<uint8_t> rawMemory;

    // TODO support record fields

};

class StaticsManager {
    using TypeIdent = struct Identifier<Symlevel::TypeDefinition>;
    using FieldIdent = struct Identifier<Symlevel::FieldDefinition>;

public:

    static StaticsManager& Of(Engine& engine);
    static StaticsManager& Of(Session& session);

    StaticsManager() = default;
    ~StaticsManager() = default;

    uintptr_t GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent);

    void VisitRefLocations(std::function<void(RefLocation*)> action) const;

private:
    mutable std::mutex lock;
    std::unordered_map<TypeIdent::Packed, StaticFieldsBundle, TypeIdent::Hasher> bundles;

    StaticFieldsBundle CreateBundle(Session& session, TypeIdent typeIdent);
};

} // namespace Engine
