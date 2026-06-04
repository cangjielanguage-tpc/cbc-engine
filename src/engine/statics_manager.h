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
        uint32_t refFieldsNum,
        uint32_t primFieldsNum,
        uint32_t recordFieldsNum,
        uint32_t recordFieldsSize,
        std::vector<StaticTypedSlotInfo> typedSlotsInfo
    );

    // underlying vector CAN NOT be copied.
    StaticFieldsBundle(StaticFieldsBundle const& another) = delete;
    StaticFieldsBundle(StaticFieldsBundle&& another) = default;

    ~StaticFieldsBundle() = default;

    uintptr_t GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent);

    void VisitRefLocations(std::function<void(RefLocation*)> action) const;

    void VisitTypedSlots(std::function<void(uint8_t* base, const StaticTypedSlotInfo&)> action) const;

private:
    RefLocation* refFieldsStart;
    uint32_t refFieldsNum;
    PrimLocation* primFieldsStart;
    uint32_t primFieldsNum;
    uint8_t* recordFieldsStart;
    uint32_t recordFieldsNum;

    std::vector<uint8_t> rawMemory;

    /// Info about typed slots for GC scanning: (offset, typeInfoPtr)
    std::vector<StaticTypedSlotInfo> typedSlotsInfo;
};

class StaticsManager {
    using TypeIdent  = struct Identifier<Symlevel::TypeDefinition>;
    using FieldIdent = struct Identifier<Symlevel::FieldDefinition>;

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
