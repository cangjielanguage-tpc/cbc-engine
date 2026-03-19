#pragma once

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
 * Resolving of aot-compiled entytity is proceed by @c refType of the entity ref, @see Terms::TemplateKind::AotType
 */
class AotTable;

class DirectCallAotData {
public:
    static DirectCallAotData ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<DirectCallAotData> offset
    );

    String GetLinkageName() const { return linkageName; }

private:
    DirectCallAotData(String linkageName) : linkageName(linkageName) {}

    String linkageName;
};

class DirectCallAotTable {
public:
    static DirectCallAotTable Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    DirectCallAotTable(DirectCallAotTable&&);
    ~DirectCallAotTable();

    std::optional<DirectCallAotData> GetData(Engine::Session& session, Index<MethodReference> index) const;

private:
    DirectCallAotTable(std::unique_ptr<AotTable> aotTable);

    std::unique_ptr<AotTable> aotTable;
};

class VirtualCallAotData {
public:
    static VirtualCallAotData ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<VirtualCallAotData> offset
    );

    uint32_t GetVNum() const { return vnum; }

    uint32_t GetExtDefNum() const { return extDefNum; }

private:
    VirtualCallAotData(uint32_t vnum, uint32_t extDefNum) : vnum(vnum), extDefNum(extDefNum) {}

    uint32_t vnum;
    uint32_t extDefNum;
};

class VirtualCallAotTable {
public:
    static VirtualCallAotTable Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    VirtualCallAotTable(VirtualCallAotTable&&);
    ~VirtualCallAotTable();

    std::optional<VirtualCallAotData> GetData(Engine::Session& session, Index<MethodReference> index) const;

private:
    VirtualCallAotTable(std::unique_ptr<AotTable> aotTable);

    std::unique_ptr<AotTable> aotTable;
};

class InterfaceCallAotData {
public:
    static InterfaceCallAotData ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<InterfaceCallAotData> offset
    );

    uint32_t GetINum() const { return inum; }

private:
    InterfaceCallAotData(uint32_t inum) : inum(inum) {}

    uint32_t inum;
};

class InterfaceCallAotTable {
public:
    static InterfaceCallAotTable Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    InterfaceCallAotTable(InterfaceCallAotTable&&);
    ~InterfaceCallAotTable();

    std::optional<InterfaceCallAotData> GetData(Engine::Session& session, Index<MethodReference> index) const;

private:
    InterfaceCallAotTable(std::unique_ptr<AotTable> aotTable);

    std::unique_ptr<AotTable> aotTable;
};

class StaticFieldAotData {
public:
    static StaticFieldAotData ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<StaticFieldAotData> offset
    );

    String GetLinkageName() const { return linkageName; }

private:
    StaticFieldAotData(String linkageName) : linkageName(linkageName) {}

    String linkageName;
};

class StaticFieldAotTable {
public:
    static StaticFieldAotTable Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    StaticFieldAotTable(StaticFieldAotTable&&);
    ~StaticFieldAotTable();

    std::optional<StaticFieldAotData> GetData(Engine::Session& session, Index<FieldReference> index) const;

private:
    StaticFieldAotTable(std::unique_ptr<AotTable> aotTable);

    std::unique_ptr<AotTable> aotTable;
};

class InstanceFieldAotData {
public:
    static InstanceFieldAotData ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<InstanceFieldAotData> offset
    );

    uint32_t GetOrdinal() const { return ordinal; }

private:
    InstanceFieldAotData(uint32_t ordinal) : ordinal(ordinal) {}

    uint32_t ordinal;
};

class InstanceFieldAotTable {
public:
    static InstanceFieldAotTable Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    InstanceFieldAotTable(InstanceFieldAotTable&&);
    ~InstanceFieldAotTable();

    std::optional<InstanceFieldAotData> GetData(Engine::Session& session, Index<FieldReference> index) const;

private:
    InstanceFieldAotTable(std::unique_ptr<AotTable> aotTable);

    std::unique_ptr<AotTable> aotTable;
};

} // namespace Symlevel
