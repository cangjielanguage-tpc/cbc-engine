#pragma once

#include "engine/identifiers.h"
#include "index.h"
#include "io/file_id.h"
#include "io/random_access_file.h"
#include "references.h"

namespace Symlevel {

/**
 * AotTables contain data for call invocation and field accessing of aot-compiled enityties.
 * In case of direct calls @c linkageName of the correcponding method.
 * In case of virtual calls each entry contains @c vnum and @c extDefNum of the corresponding method.
 * In case of interface calls each entry contains @c inum of the corresponding method.
 * In case of static fields each entry contains @c linkageName of the corresponding field.
 * In case of intstance fields each entry contains @c oridinal of the corresponding field.
 *
 * Resolving of aot-compiled entytity is proceed by @c refType of the entity ref, @see TemplateKind::AotType
 */
struct AotTable {
    IO::FileId fileId;

    uint32_t bucketTableStart;
    uint32_t bucketTableSize;

    uint32_t bucketsStart;
    uint32_t bucketsSize;

    static AotTable Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);
};

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

template <typename Table> class AotDataTable {
public:
    AotTable table;

    static Table Read(IO::FileId fileId, IO::RandomAccessFile& file, Offset<Table> offset)
    {
        return Table { AotTable::Read(fileId, file, offset) };
    }
};

struct VirtualCallAotTable : AotDataTable<VirtualCallAotTable> {
    VirtualCallAotData GetData(Engine::Session& session, RefId<MethodReference> index) const;
};

struct DirectCallAotTable : AotDataTable<DirectCallAotTable> {
    DirectCallAotData GetData(Engine::Session& session, RefId<MethodReference> index) const;
};

struct InterfaceCallAotTable : AotDataTable<InterfaceCallAotTable> {
    InterfaceCallAotData GetData(Engine::Session& session, RefId<MethodReference> index) const;
};

struct StaticFieldAotTable : AotDataTable<StaticFieldAotTable> {
    StaticFieldAotData GetData(Engine::Session& session, RefId<FieldReference> index) const;
};

struct InstanceFieldAotTable : AotDataTable<InstanceFieldAotTable> {
    InstanceFieldAotData GetData(Engine::Session& session, RefId<FieldReference> index) const;
};

} // namespace Symlevel
