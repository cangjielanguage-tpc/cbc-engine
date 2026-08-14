#pragma once

#include "engine/symlevel/flags.h"
#include "engine/symlevel/io/random_access_file.h"
#include "engine/symlevel/version_metadata.h"
#include "io/file_id.h"
#include "string.h"
#include "utils/reinterpretation.h"
#include <cstdint>
#include <optional>

namespace Engine {
struct Term;
}

namespace Symlevel {

using Term = Engine::Term;

template <typename T> struct Offset {
    static constexpr uint64_t BIT_SIZE = 32;
    static constexpr uint64_t MASK     = (1llu << BIT_SIZE) - 1;

    Offset(uint32_t value) : value(value) {}

    bool operator==(const Offset& another) const { return value == another.value; }

    operator uint32_t() const { return value; }

    uint32_t value;
};

/// References and terms in CBC are referenced by index in the corresponding table.
template <typename T> struct RefId {
    static constexpr uint64_t BIT_SIZE = 32;
    static constexpr uint64_t MASK     = (1llu << BIT_SIZE) - 1;

    constexpr explicit RefId(uint32_t index) : value(index) {}

    operator uint32_t() const { return value; }

    uint32_t GetValue() const { return value; }

    bool operator==(const RefId& another) const { return value == another.value; }

private:
    uint32_t value;
};

template <typename T> struct Identifier {
    using Packed = uint64_t;

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    Identifier(Symlevel::Offset<T> offs, IO::FileId fileId) : offs(offs), fileId(fileId) {}

    Identifier(Packed packed)
        : Identifier(
              Symlevel::Offset<T>(packed & Symlevel::Offset<T>::MASK),
              IO::FileId((packed >> Symlevel::Offset<T>::BIT_SIZE) & IO::FileId::MASK)
          )
    {}

    Symlevel::Offset<T> GetOffset() const { return offs; }

    IO::FileId GetFileId() const { return fileId; }

    bool operator==(const Identifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const
    {
        uint64_t low  = offs;
        uint64_t high = fileId;
        return low | (high << Symlevel::Offset<T>::BIT_SIZE);
    }

private:
    Symlevel::Offset<T> offs;
    IO::FileId fileId;
};

template <typename T> struct RefIdentifier {
    using Packed = uint64_t;

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    RefIdentifier(Symlevel::RefId<T> index, IO::FileId fileId) : index(index), fileId(fileId) {}

    RefIdentifier(Packed packed)
        : RefIdentifier(
              Symlevel::RefId<T>(packed & Symlevel::RefId<T>::MASK),
              IO::FileId((packed >> Symlevel::RefId<T>::BIT_SIZE) & IO::FileId::MASK)
          )
    {}

    Symlevel::RefId<T> GetIndex() const { return index; }

    IO::FileId GetFileId() const { return fileId; }

    bool operator==(const RefIdentifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const
    {
        uint64_t low  = index;
        uint64_t high = fileId;
        return low | (high << Symlevel::RefId<T>::BIT_SIZE);
    }

private:
    Symlevel::RefId<T> index;
    IO::FileId fileId;
};

template <typename T> struct OffsetSequence {
    OffsetSequence(IO::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos)
    {}

    OffsetSequence() : file(0), startPos(0), endPos(0) {}

    IO::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

template <typename T> struct RefSequence {
    RefSequence(IO::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos) {}

    RefSequence() : file(0), startPos(0), endPos(0) {}

    IO::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

class String : public std::string_view {
public:
    String(std::string_view view) : std::string_view(std::move(view)) {}
};

class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

/**
 * @brief Bucket-based hash table of file entities which can be accessed by enitity name.
 *
 * Let's say we have @c memberCount members in index which can be split to @c bucketCount buckets
 * by some hash-function. We store @c bucketTable, each element of which is @c start index
 * of corresponding bucket. The @c buckets themselves are ordered by indicies in the table
 * and stored flat in a single array.
 *
 * Note that if @c bucketTable(i) is @c start index of bucket then @c bucketTable(i+1)
 * is @c end index of the same bucket exclusively.
 *
 * For completness, we extend bucket table with additional element which holds @c buckets length.
 *
 * Examples:
 * @code
 * 1.
 *      bucketTable: [0,  0,  1,  3,  4,  4,  6]
 *      buckets:     [x0, x1, x2, x3, x4, x5]
 *
 *      bucket0 = ()
 *      bucket1 = (x0)
 *      bucket2 = (x1, x2)
 *      bucket3 = (x3)
 *      bucket4 = ()
 *      bucket5 = (x4, x5)
 *
 * 2.
 *      bucketTable: [0,  2,  2,  2,  3,  6,  6]
 *      buckets:     [x0, x1, x2, x3, x4, x5]
 *
 *      bucket0 = (x0, x1)
 *      bucket1 = ()
 *      bucket2 = ()
 *      bucket3 = (x2)
 *      bucket4 = (x3, x4, x5)
 *      bucket5 = ()
 *
 * 3.
 *      bucketTable: [0,  1,  2,  3,  4,  5,  6]
 *      buckets:     [x0, x1, x2, x3, x4, x5]
 *
 *      bucket0 = (x0)
 *      bucket1 = (x1)
 *      bucket2 = (x2)
 *      bucket3 = (x3)
 *      bucket4 = (x4)
 *      bucket5 = (x5)
 * @endcode
 *
 * Note that @c buckets length is @c memberCount
 * Note that @c bucketTable length is @c bucketCount+1
 */
template <typename T> struct MemberIndex {
    IO::FileId fileId;

    uint32_t bucketTableStart;
    uint32_t bucketTableSize;

    uint32_t bucketsStart;
    uint32_t bucketsSize;

    MemberIndex(
        IO::FileId fileId,
        uint32_t bucketTableStart,
        uint32_t bucketTableSize,
        uint32_t bucketsStart,
        uint32_t bucketsSize
    )
        : fileId(fileId),
          bucketTableStart(bucketTableStart),
          bucketTableSize(bucketTableSize),
          bucketsStart(bucketsStart),
          bucketsSize(bucketsSize)
    {}

    MemberIndex(MemberIndex<void> const& index)
        : fileId(index.fileId),
          bucketTableStart(index.bucketTableStart),
          bucketTableSize(index.bucketTableSize),
          bucketsStart(index.bucketsStart),
          bucketsSize(index.bucketsSize)
    {}
};

using TypeIndex   = MemberIndex<TypeDefinition>;
using FieldIndex  = MemberIndex<FieldDefinition>;
using MethodIndex = MemberIndex<MethodDefinition>;

// AotTables contain data for call invocation and field accessing of aot-compiled enityties.
// In case of direct calls @c linkageName of the correcponding method.
// In case of virtual calls each entry contains @c vnum and @c extDefNum of the corresponding method.
// In case of interface calls each entry contains @c inum of the corresponding method.
// In case of static fields each entry contains @c linkageName of the corresponding field.
// In case of intstance fields each entry contains @c oridinal of the corresponding field.
//
// Resolving of aot-compiled entytity is proceed by @c refType of the entity ref, @see TemplateKind::AotType

struct DirectCallAotData {
    Identifier<String> linkangeName;
};

struct VirtualCallAotData {
    uint16_t methodNum;
    uint16_t extDefNum;
};

struct InterfaceCallAotData {
    int inum;
};

struct StaticFieldAotData {
    Identifier<String> linkangeName;
};

struct InstanceFieldAotData {
    uint32_t ordinal;
};

using DirectCallAotTable    = MemberIndex<DirectCallAotData>;
using InterfaceCallAotTable = MemberIndex<InterfaceCallAotData>;
using VirtualCallAotTable   = MemberIndex<VirtualCallAotData>;
using InstanceFieldAotTable = MemberIndex<InstanceFieldAotData>;
using StaticFieldAotTable   = MemberIndex<StaticFieldAotData>;

struct ExceptionRegion {
    uint32_t start;
    uint32_t end;
    uint32_t target;
};

struct RawData {
    IO::FileId fileId;
    uint32_t start;
    uint32_t end;
};

// Remove heap-allocated fields in case of moving those structures allocation into arena
struct LivenessInfo {
    uint32_t cbcPos;
    uint16_t regMask;
    std::vector<uint32_t> refSlotNums;
    std::vector<std::pair<uint32_t, uint32_t>> mutPairs;
};

struct StackPtrsInfo {
    uint32_t cbcPos;
    std::vector<uint32_t> resources;
};

class Code {
public:
    static Code Mock(uint8_t* codePtr, uint32_t codeSize) { return Code(codePtr, codeSize); }

    uint8_t* CodePtr() { return codePtr; }

    uint32_t CodeSize() { return codeSize; }

    uint32_t UntypedSlotCount() { return untypedSlotCount; }

    uint32_t StackAllocSigsCount() { return stackAllocSigsCount; }

    uint32_t* StackAllocSigs() { return stackAllocSigs; }

    uint8_t UsedNonVolIRegMask() { return usedNonVolIRegMask; }

    uint8_t UsedNonVolFRegMask() { return usedNonVolFRegMask; }

    Code(uint8_t* codePtr, uint32_t codeSize) : codePtr(codePtr), codeSize(codeSize) {}

    Code(
        uint32_t untypedSlotCount,
        uint32_t stackAllocSigsCount,
        uint32_t* stackAllocSigs,
        uint32_t ohmSlotCount,
        uint8_t usedNonVolIRegMask,
        uint8_t usedNonVolFRegMask,
        uint32_t maxCalleeStackArgsCount,
        bool mayHaveNativeCalls,
        uint32_t codeSize,
        uint8_t* codePtr,
        RawData rawExTable,
        RawData rawLivenessInfo,
        RawData rawStackPtrsInfo
    )
        : untypedSlotCount(untypedSlotCount),
          stackAllocSigsCount(stackAllocSigsCount),
          stackAllocSigs(stackAllocSigs),
          ohmSlotCount(ohmSlotCount),
          usedNonVolIRegMask(usedNonVolIRegMask),
          usedNonVolFRegMask(usedNonVolFRegMask),
          maxCalleeStackArgsCount(maxCalleeStackArgsCount),
          mayHaveNativeCalls(mayHaveNativeCalls),
          codePtr(codePtr),
          codeSize(codeSize),
          rawExTable(rawExTable),
          rawLivenessInfo(rawLivenessInfo),
          rawStackPtrsInfo(rawStackPtrsInfo)
    {}

    uint32_t untypedSlotCount    = 0;
    uint32_t stackAllocSigsCount = 0;
    uint32_t ohmSlotCount        = 0;

    uint32_t* stackAllocSigs = nullptr;

    uint8_t usedNonVolIRegMask       = 0;
    uint8_t usedNonVolFRegMask       = 0;
    uint32_t maxCalleeStackArgsCount = 0;

    bool mayHaveNativeCalls = false;

    uint32_t codeSize;
    uint8_t* codePtr;

    RawData rawExTable       = { IO::FileId(0), 0, 0 };
    RawData rawLivenessInfo  = { IO::FileId(0), 0, 0 };
    RawData rawStackPtrsInfo = { IO::FileId(0), 0, 0 };
};

enum class EnumKind : uint8_t {
    NOT_ENUM,
    UNION,
    OPTION0, // enum { Some(T); None }
    OPTION1, // enum { None; Some(T) }
    PRIMITIVE,
};

class TypeDefinition {
public:
    struct Content {
        Identifier<TypeDefinition> identifier;
        Identifier<String> name;
        MethodIndex methods;
        FieldIndex fields;
        OffsetSequence<MethodDefinition> virtualMethods;
        OffsetSequence<FieldDefinition> instanceFields;
        RefIdentifier<Term> superOrEnumType;
        TypeFlags flags;
        uint8_t arity;
        EnumKind enumKind;
        RefSequence<Term> interfaces {};
        RefSequence<Term> unionFields {};
    };

    Identifier<TypeDefinition> const GetIdentifier() { return content.identifier; }

    Identifier<String> const GetName() const { return content.name; }

    MethodIndex const GetMethods() const { return content.methods; }

    FieldIndex const GetFields() const { return content.fields; }

    OffsetSequence<MethodDefinition> const GetVirtualMethods() const { return content.virtualMethods; }

    OffsetSequence<FieldDefinition> const GetInstanceFields() const { return content.instanceFields; }

    RefIdentifier<Term> const GetSuperType() const
    {
        if (content.enumKind != EnumKind::NOT_ENUM) {
            return RefIdentifier(RefId<Term>(0), IO::FileId(0)); // NIL TERM
        }
        return content.superOrEnumType;
    }

    RefIdentifier<Term> const GetEnumType() const
    {
        if (content.enumKind == EnumKind::NOT_ENUM) {
            return RefIdentifier(RefId<Term>(0), IO::FileId(0)); // NIL TERM
        }
        return content.superOrEnumType;
    }

    TypeFlags const GetFlags() const { return content.flags; }

    RefSequence<Term> GetInterfaces() const { return content.interfaces; }

    Content const* operator->() const { return &content; }

    Content const* operator*() const { return &content; }

    TypeDefinition(Content&& content) : content(content) {}

    Content content;
};

class FieldDefinition {
public:
    struct Content {
        Identifier<FieldDefinition> identifier;
        Offset<String> nameOffset;
        RefIdentifier<Term> fieldType;
        FieldFlags flags;
    };

    inline Identifier<String> GetName() const
    {
        return ::Symlevel::Identifier(content.nameOffset, content.identifier.GetFileId());
    }

    inline RefIdentifier<Term> FieldType() const { return content.fieldType; }

    inline Identifier<FieldDefinition> Identifier() { return content.identifier; }

    inline FieldFlags Flags() const { return content.flags; }

    FieldDefinition(Content&& content) : content(content) {}

    Content content;
};

class MethodDefinition {
public:
    struct Content {
        Identifier<MethodDefinition> identifier;
        RefIdentifier<Term> signature;
        Offset<String> typeNameOffset;
        Offset<String> nameOffset;
        MethodFlags flags;
        uint8_t arity;

        std::optional<Identifier<Code>> code             = std::nullopt;
        std::optional<Identifier<String>> sourceFile     = std::nullopt;
        std::optional<Identifier<String>> sourceFullName = std::nullopt;
        std::optional<Identifier<String>> linkageName    = std::nullopt;
    };

    inline Identifier<String> Name() const { return Identifier(content.nameOffset, content.identifier.GetFileId()); }

    inline Identifier<String> TypeName() const
    {
        return Identifier(content.typeNameOffset, content.identifier.GetFileId());
    }

    inline RefIdentifier<Term> Signature() const { return content.signature; }

    inline std::optional<Identifier<Code>> MethodCode() const { return content.code; }

    std::optional<Identifier<String>> SourceFile() const { return content.sourceFile; }

    std::optional<Identifier<String>> SourceFullName() const { return content.sourceFullName; }

    std::optional<Identifier<String>> LinkageName() const { return content.linkageName; }

    inline IO::FileId FileId() const { return content.identifier.GetFileId(); }

    Identifier<MethodDefinition> GetIdentifier() const { return content.identifier; }

    MethodFlags GetFlags() const { return content.flags; }

    MethodRefFlags GetABIFlags() const;

    Content const* operator->() const { return &content; }

    MethodDefinition(Content&& content) : content(content) {}

    Content content;
};

struct MethodReference {
    Identifier<String> name;
    RefIdentifier<Term> refType;
    RefIdentifier<Term> methodSig;
    RefIdentifier<Term> tvars;
    MethodRefFlags flags;
};

struct FieldReference {
    Identifier<String> name;
    RefIdentifier<Term> refType;
    RefIdentifier<Term> fieldType;
    bool isRecord;
};

struct ErasedOffsetPool {
    IO::FileId file;
    uint32_t offset;
    uint32_t size;
    uint32_t adjustment;
};

template <typename T, uint32_t adjustment = 0> struct OffsetPool {
    static constexpr uint32_t ADJUSTMENT = adjustment;

    struct RefIdIterator {
        IO::FileId file;
        uint32_t id;

        RefIdentifier<T> operator*() const { return RefIdentifier<T>(Symlevel::RefId<T>(id + adjustment), file); }

        RefIdIterator& operator++()
        {
            id++;
            return *this;
        }

        bool operator!=(RefIdIterator const& another) const { return id != another.id; }
    };

    OffsetPool(IO::FileId file, uint32_t offset, uint32_t size) : file(file), offset(offset), size(size) {}

    RefIdIterator begin() const { return RefIdIterator { file, 0 }; }

    RefIdIterator end() const { return RefIdIterator { file, size }; }

    ErasedOffsetPool Erased() const { return { file, offset, size, adjustment }; }

private:
    IO::FileId file;
    uint32_t offset;
    uint32_t size;
};

struct RegionData {
    static constexpr uint32_t FIRST_NON_PRIMITIVE_TERM_ID = 20;

    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    RegionData(
        OffsetPool<MethodReference> methods,
        OffsetPool<FieldReference> fields,
        OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms
    );

    OffsetPool<MethodReference> methods;
    OffsetPool<FieldReference> fields;
    OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms;

    template <typename T> ErasedOffsetPool ErasedPool() const;

    template <> ErasedOffsetPool ErasedPool<Term>() const { return terms.Erased(); }

    template <> ErasedOffsetPool ErasedPool<MethodReference>() const { return methods.Erased(); }

    template <> ErasedOffsetPool ErasedPool<FieldReference>() const { return fields.Erased(); }
};

// metadata
class Dependencies;

// misc

class CbcFile {
public:
    static CbcFile Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name);

    CbcFile(CbcFile&& other);
    ~CbcFile();

    IO::FileId Id() const;
    uint32_t GetCodeSectionOffs() const;
    uint32_t GetStringSectionOffs() const;
    uint32_t GetTypeDefSectionOffs() const;
    uint32_t GetMethodDefSectionOffs() const;
    uint32_t GetFieldDefSectionOffs() const;
    uint32_t GetTermSectionOffs() const;
    uint32_t GetMethodRefSectionOffs() const;
    uint32_t GetFieldRefSectionOffs() const;
    uint32_t GetAotDataSectionOffs() const;

    String GetName() const;
    String GetPath() const;

    const VersionMetadata& GetVersionMetadata() const;

    const RegionData& GetRegionData() const;
    const TypeIndex& GetTypeIndex() const;
    const Dependencies& GetDependencies() const;
    const std::optional<Identifier<String>> GetMainTypeName() const;

    /// FIXME: tables should be assigned to corresponding regions.
    const DirectCallAotTable& GetDirectCallAotTable() const;
    const VirtualCallAotTable& GetVirtualCallAotTable() const;
    const InterfaceCallAotTable& GetInterfaceCallAotTable() const;
    const StaticFieldAotTable& GetStaticFieldAotTable() const;
    const InstanceFieldAotTable& GetInstanceFieldAotTable() const;

private:
    struct Impl;
    friend struct Impl;

    CbcFile(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
