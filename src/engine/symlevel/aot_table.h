#pragma once

#include "engine/identifiers.h"
#include "engine/symlevel/member_index.h"
#include "string.h"
#include <cstdint>

namespace Symlevel {

// AotTables contain data for call invocation and field accessing of aot-compiled enityties.
// In case of direct calls @c linkageName of the correcponding method.
// In case of virtual calls each entry contains @c vnum and @c extDefNum of the corresponding method.
// In case of interface calls each entry contains @c inum of the corresponding method.
// In case of static fields each entry contains @c linkageName of the corresponding field.
// In case of intstance fields each entry contains @c oridinal of the corresponding field.
//
// Resolving of aot-compiled entytity is proceed by @c refType of the entity ref, @see TemplateKind::AotType

struct DirectCallAotData {
    Engine::Identifier<String> linkangeName;
};

struct VirtualCallAotData {
    uint16_t methodNum;
    uint16_t extDefNum;
};

struct InterfaceCallAotData {
    int inum;
};

struct StaticFieldAotData {
    Engine::Identifier<String> linkangeName;
};

struct InstanceFieldAotData {
    uint32_t ordinal;
};

using DirectCallAotTable    = MemberIndex<DirectCallAotData>;
using InterfaceCallAotTable = MemberIndex<InterfaceCallAotData>;
using VirtualCallAotTable   = MemberIndex<VirtualCallAotData>;
using InstanceFieldAotTable = MemberIndex<InstanceFieldAotData>;
using StaticFieldAotTable   = MemberIndex<StaticFieldAotData>;

} // namespace Symlevel
