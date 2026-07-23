#pragma once

#include "arena.h"
#include "engine.h"
#include "identifiers.h"
#include "symlevel/definitions.h"

#include <cstdint>
#include <functional>
#include <mutex>

namespace Engine {

struct RefLocation {
    uintptr_t reference;
};

struct PrimLocation {
    uint64_t value;
};

enum SlotKind {
    PRIMITIVE,
    REFERENCE,
    RECORD
};

struct StaticTypedSlotInfo {
    uint32_t offset;
    void* typeInfoPtr;
};

class StaticFieldsBundle {
    using TypeIdent  = struct Identifier<Symlevel::TypeDefinition>;
    using FieldIdent = struct Identifier<Symlevel::FieldDefinition>;

public:
    StaticFieldsBundle(
        uintptr_t refs,
        uintptr_t primitives,
        uintptr_t records,
        uint32_t* recordOffsets,
        uint32_t* referenceOffsets,
        uint32_t refCount,
        uint32_t refOffsetsCount
    );

    // underlying vector CAN NOT be copied.
    StaticFieldsBundle(StaticFieldsBundle const& another) = delete;
    StaticFieldsBundle(StaticFieldsBundle&& another) = default;

    ~StaticFieldsBundle();

    uintptr_t GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent);

    void VisitRefLocations(std::function<void(RefLocation*)> action) const;

private:
    uintptr_t refs;
    uintptr_t primitives;
    uintptr_t records;
    uint32_t* recordOffsets;
    uint32_t* referenceOffsets;
    uint32_t refCount;
    uint32_t refOffsetsCount;
};

class StaticsManager {
    using TypeIdent  = Identifier<Symlevel::TypeDefinition>;
    using FieldIdent = Identifier<Symlevel::FieldDefinition>;

public:
    static StaticsManager& Of(Engine& engine);
    static StaticsManager& Of(Session& session);

    StaticsManager()  = default;
    ~StaticsManager() = default;

    uintptr_t GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent);

    void VisitRefLocations(
        std::function<void(RefLocation*)> untypedSlotsVisitor,
        std::function<void(uint8_t* base, const StaticTypedSlotInfo&)> typedSlotsVisitor
    ) const;

private:
    mutable std::mutex lock;
    std::unordered_map<TypeIdent::Packed, StaticFieldsBundle, TypeIdent::Hasher> bundles;

    StaticFieldsBundle CreateBundle(Session& session, TypeIdent typeIdent);
};

} // namespace Engine
