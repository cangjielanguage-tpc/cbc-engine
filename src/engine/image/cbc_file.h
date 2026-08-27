#pragma once

#include "engine/image/flags.h"
#include "engine/image/io/random_access_file.h"
#include "engine/image/version_metadata.h"
#include "string.h"
#include "utils/reinterpretation.h"
#include <cstdint>
#include <optional>

/// Intermediate, read-only data structures for parsed `.cbc` binary metadata files.
///
/// This module defines the raw structural layout, entities, and index mapping tables
/// extracted from Compiled Bytecode (`.cbc`) files. It acts as an **Intermediate Representation (IR)**
/// layer between low-level binary I/O operations and the full runtime/execution engine.
///
/// ### Architectural Overview
/// Data structures in this file represent un-hydrated runtime entities (`TypeDefinition`,
/// `MethodDefinition`, `FieldDefinition`, `Code`, etc.). Rather than holding direct C++
/// object pointers, references are expressed as lightweight handles, table indices,
/// or file offsets.
///
/// Key Design Principles & Invariants:
/// - **Immutability:** Entities in this file are strictly read-only and reflect on-disk state.
/// - **Symbol & Reference Resolution:** Entities store indices (`RefId<T>`) or byte offsets
///   (`Offset<T>`). Cross-file references are globally packed into 64-bit integer handles
///   (`Identifier<T>`, `RefIdentifier<T>`) combining a 24-bit `Image::FileId` with an offset/index.
/// - **Zero-Allocation Data Mapping:** Types like `MemberIndex<T>` represent static, bucket-based
///   hash tables stored directly flat inside the source file, avoiding dynamic runtime allocations.
/// - **Deferred Decoding:** Complex payloads (e.g., bytecode arrays, GC liveness vectors, stack
///   maps) are tracked via raw file markers (`RawData`) and read on demand by downstream execution components.
///
/// ### Pipeline Context
/// ```text
///  [ .cbc Binary File ]
///           |
///           v
/// +-----------------------------------------+
/// |  CbcFile / Image Models (This File)  |  <-- Unresolved IR / File Offsets
/// +-----------------------------------------+
///                      |
///                      v
/// +-----------------------------------------+
/// |  Engine / Reader / Decoder              |  <-- Live C++ Objects & Execution AST
/// +-----------------------------------------+
/// ```

namespace Engine {
/// Terms are entities that represent a type reference.
/// Such entities are part of execution engine, but could be referenced as part of model,
/// e.g. using RefIdentifier.
struct Term;
} // namespace Engine

namespace Image {

// Alias for convinience, since term references are used in model quite often.
using Term = Engine::Term;

struct FileId {
    static constexpr auto BIT_SIZE = 24;
    static constexpr auto MASK     = (1 << BIT_SIZE) - 1;

    uint32_t id;

    explicit FileId(uint32_t id) : id(id) { ASSERT((id & MASK) == id); }

    inline operator std::uint32_t() const { return id; }

    inline operator std::size_t() const { return id; }

    inline bool operator==(const FileId& another) const { return id == another.id; }
};

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

/// A globally unique, bit-packed 64-bit handle referencing an entity by **byte offset** within a file.
///
/// `T` The type of entity being addressed (e.g., `TypeDefinition`, `String`, `Code`).
///
/// `Identifier` combines a 32-bit file byte offset (`Offset<T>`) and a 24-bit file identifier (`Image::FileId`)
/// into a single 64-bit scalar (`Packed`). This allows cross-file binary offset references to be passed
/// by value, bitwise-compared, or used as hash keys (`Hasher`) without dynamic allocation or heap overhead.
///
/// Bit Layout (64-bit uint64_t):
/// +----------------------------------+----------------------------------+
/// | 57..64   | 32..56: File ID       |   0..31: Byte Offset             |
/// | Reserved | (Image::FileId)          |   (Image::Offset<T>)          |
/// +----------------------------------+----------------------------------+
template <typename T> struct Identifier {
    using Packed = uint64_t;

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    Identifier(Image::Offset<T> offs, Image::FileId fileId) : offs(offs), fileId(fileId) {}

    Identifier(Packed packed)
        : Identifier(
              Image::Offset<T>(packed & Image::Offset<T>::MASK),
              Image::FileId((packed >> Image::Offset<T>::BIT_SIZE) & Image::FileId::MASK)
          )
    {}

    Image::Offset<T> GetOffset() const { return offs; }

    Image::FileId GetFileId() const { return fileId; }

    bool operator==(const Identifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const
    {
        uint64_t low  = offs;
        uint64_t high = fileId;
        return low | (high << Image::Offset<T>::BIT_SIZE);
    }

private:
    Image::Offset<T> offs;
    Image::FileId fileId;
};

/// A globally unique, bit-packed 56-bit handle referencing an entity by **table/array index** within a file.
///
/// `T` The type of referenced symbol or entry (e.g., `Term`, `MethodReference`).
///
/// `RefIdentifier` combines a 32-bit 0-based index (`RefId<T>`) pointing into a symbol table or array,
/// with a 24-bit file identifier (`Image::FileId`). Like `Identifier`, it packs into a single 64-bit
/// integer (`Packed`) for zero-cost copies, fast comparison, and direct use in hash maps via `Hasher`.
///
/// Bit Layout (64-bit uint64_t):
/// +----------------------------------+----------------------------------+
/// | 57..64   | 32..56: File ID       |   Bits 0..31: Table Index        |
/// | Reserved | (Image::FileId)          |   (Image::RefId<T>)           |
/// +----------------------------------+----------------------------------+
template <typename T> struct RefIdentifier {
    using Packed = uint64_t;

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    RefIdentifier(Image::RefId<T> index, Image::FileId fileId) : index(index), fileId(fileId) {}

    RefIdentifier(Packed packed)
        : RefIdentifier(
              Image::RefId<T>(packed & Image::RefId<T>::MASK),
              Image::FileId((packed >> Image::RefId<T>::BIT_SIZE) & Image::FileId::MASK)
          )
    {}

    Image::RefId<T> GetIndex() const { return index; }

    Image::FileId GetFileId() const { return fileId; }

    bool operator==(const RefIdentifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const
    {
        uint64_t low  = index;
        uint64_t high = fileId;
        return low | (high << Image::RefId<T>::BIT_SIZE);
    }

private:
    Image::RefId<T> index;
    Image::FileId fileId;
};

// A lightweight descriptor representing a sequence of variable-length byte offsets stored in a file range.
//
// `T` The target entity type addressed by each offset in the sequence (e.g., `MethodDefinition`, `FieldDefinition`).
//
// `OffsetSequence` defines a byte slice `[startPos, endPos)` within a specific file (`file`).
// To save space, the array of offsets is not expanded in heap memory; instead, elements within
// the byte range are packed sequentially using **ULEB128 (Unsigned Little Endian Base 128)** encoding.
//
// Downstream readers stream through this byte range, decoding each ULEB128 integer
// to recover individual binary offsets (`Offset<T>`).
template <typename T> struct OffsetSequence {
    OffsetSequence(Image::FileId file, uint32_t startPos, uint32_t endPos)
        : file(file),
          startPos(startPos),
          endPos(endPos)
    {}

    OffsetSequence() : file(0), startPos(0), endPos(0) {}

    Image::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

/// A lightweight descriptor representing a sequence of variable-length table indices stored in a file range.
///
/// `T` The referenced target type (e.g., `Term`).
///
/// `RefSequence` defines a byte slice `[startPos, endPos)` within a specific file (`file`).
/// Like `OffsetSequence`, it avoids allocating container nodes by referencing a raw binary byte
/// range where elements are compressed via **ULEB128 (Unsigned Little Endian Base 128)** encoding.
///
/// Sequential decoding of the byte slice yields 0-based symbol/table indices (`RefId<T>`),
/// which can be looked up in the corresponding target symbol table (e.g., the `Term` table or `Interfaces` list).
template <typename T> struct RefSequence {
    RefSequence(Image::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos)
    {}

    RefSequence() : file(0), startPos(0), endPos(0) {}

    Image::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

/// Lightweight, zero-copy string wrapper around std::string_view.
///
/// Underlying string memory is managed by the session arena, ensuring the
/// character buffer remains valid for the entire duration of the session
/// without needing individual heap allocations or copying.
class String : public std::string_view {
public:
    String(std::string_view view) : std::string_view(std::move(view)) {}
};

class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

/// Bucket-based hash table of file entities which can be accessed by enitity name.
///
/// Let's say we have @c memberCount members in index which can be split to @c bucketCount buckets
/// by some hash-function. We store @c bucketTable, each element of which is @c start index
/// of corresponding bucket. The @c buckets themselves are ordered by indicies in the table
/// and stored flat in a single array.
///
/// Note that if @c bucketTable(i) is @c start index of bucket then @c bucketTable(i+1)
/// is @c end index of the same bucket exclusively.
///
/// For completness, we extend bucket table with additional element which holds @c buckets length.
///
/// Examples:
/// @code
/// 1.
///      bucketTable: [0,  0,  1,  3,  4,  4,  6]
///      buckets:     [x0, x1, x2, x3, x4, x5]
///
///      bucket0 = ()
///      bucket1 = (x0)
///      bucket2 = (x1, x2)
///      bucket3 = (x3)
///      bucket4 = ()
///      bucket5 = (x4, x5)
///
/// 2.
///      bucketTable: [0,  2,  2,  2,  3,  6,  6]
///      buckets:     [x0, x1, x2, x3, x4, x5]
///
///      bucket0 = (x0, x1)
///      bucket1 = ()
///      bucket2 = ()
///      bucket3 = (x2)
///      bucket4 = (x3, x4, x5)
///      bucket5 = ()
///
/// 3.
///      bucketTable: [0,  1,  2,  3,  4,  5,  6]
///      buckets:     [x0, x1, x2, x3, x4, x5]
///
///      bucket0 = (x0)
///      bucket1 = (x1)
///      bucket2 = (x2)
///      bucket3 = (x3)
///      bucket4 = (x4)
///      bucket5 = (x5)
/// @endcode
///
/// Note that @c buckets length is @c memberCount
/// Note that @c bucketTable length is @c bucketCount+1
template <typename T> struct MemberIndex {
    Image::FileId fileId;

    uint32_t bucketTableStart;
    uint32_t bucketTableSize;

    uint32_t bucketsStart;
    uint32_t bucketsSize;

    MemberIndex(
        Image::FileId fileId,
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

// -------------------------------- end of aot tables --------------------------------

/// Executable Code & Runtime Metadata.
///
/// The following structures capture executable byte sequences along with the
/// metadata required by the runtime for execution, exception handling, and GC tracing:
///
/// - Code: Main container for a method's executable code, holding frame allocation
///   counts, non-volatile register masks, and pointers to raw binary metadata streams.
/// - RawData: Generic byte range descriptor [start, end) within a specific .cbc file,
///   used for deferred reading of variable-length runtime tables.
/// - ExceptionRegion: Unwinding metadata mapping try-block bytecode ranges [start, end)
///   to handler targets.
/// - LivenessInfo & StackPtrsInfo: GC root-tracing metadata mapped to specific bytecode
///   offsets (cbcPos). Tracks active register masks, reference slot locations, stack
///   pointers, and object mutation pairs for exact garbage collection.
///
/// TODO: get rid of vectors
struct ExceptionRegion {
    uint32_t start;
    uint32_t end;
    uint32_t target;
};

struct RawData {
    Image::FileId fileId;
    uint32_t start;
    uint32_t end;
};

// Remove heap-allocated fields in case of moving those structures allocation into arena
struct LivenessInfo {
    uint32_t cbcPos;
    uint16_t regMask;
    std::vector<uint32_t> refSlotNums;                   // FIXME: light-weight handle
    std::vector<std::pair<uint32_t, uint32_t>> mutPairs; // FIXME: light-weight handle
};

struct StackPtrsInfo {
    uint32_t cbcPos;
    std::vector<uint32_t> resources; // FIXME: light-weight handle
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

    RawData rawExTable       = { Image::FileId(0), 0, 0 };
    RawData rawLivenessInfo  = { Image::FileId(0), 0, 0 };
    RawData rawStackPtrsInfo = { Image::FileId(0), 0, 0 };
};

enum class EnumKind : uint8_t {
    NOT_ENUM,
    UNION,
    OPTION0, // enum { Some(T); None }
    OPTION1, // enum { None; Some(T) }
    PRIMITIVE,
};

/// Represents the static metadata, hierarchy, and member index of a type parsed from .cbc.
///
/// Encompasses classes, interfaces, enums, and unions. It wraps an underlying `Content`
/// struct containing lookup tables for members (`MethodIndex`, `FieldIndex`),
/// offset sequences for virtual entries, and type references for superclasses,
/// interfaces, or enum representations.
///
/// Note: The interpretation of `superOrEnumType` depends on `enumKind` (i.e., whether
/// the type represents a standard class hierarchy or a tagged enum/union variant).
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
            return RefIdentifier(RefId<Term>(0), Image::FileId(0)); // NIL TERM
        }
        return content.superOrEnumType;
    }

    RefIdentifier<Term> const GetEnumType() const
    {
        if (content.enumKind == EnumKind::NOT_ENUM) {
            return RefIdentifier(RefId<Term>(0), Image::FileId(0)); // NIL TERM
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

/// Represents the static declaration and layout metadata of a type member field.
///
/// Stores the field's name offset, type reference (`fieldType` as a `Term`),
/// and field modifier flags (e.g., static, read-only, access level).
/// Name offsets are automatically converted to file-qualified `Identifier<String>`
/// handles via accessor methods.
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
        return ::Image::Identifier(content.nameOffset, content.identifier.GetFileId());
    }

    inline RefIdentifier<Term> FieldType() const { return content.fieldType; }

    inline Identifier<FieldDefinition> Identifier() { return content.identifier; }

    inline FieldFlags Flags() const { return content.flags; }

    FieldDefinition(Content&& content) : content(content) {}

    Content content;
};

/// Represents a declared method, function, or constructor parsed from .cbc.
///
/// Holds the method signature (`RefIdentifier<Term>`), parameter arity, modifier flags,
/// and an optional link to executable bytecode (`Identifier<Code>`).
/// Also stores optional debugging and linkage metadata, such as source file paths
/// and ABI linkage names for AOT functions.
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

    inline Image::FileId FileId() const { return content.identifier.GetFileId(); }

    Identifier<MethodDefinition> GetIdentifier() const { return content.identifier; }

    MethodFlags GetFlags() const { return content.flags; }

    MethodRefFlags GetABIFlags() const;

    Content const* operator->() const { return &content; }

    MethodDefinition(Content&& content) : content(content) {}

    Content content;
};

/// Represents the extension of type with superinterfaces and member index parsed from .cbc.
///
/// It wraps an underlying `Content` struct containing lookup table for methods (`MethodIndex`),
/// offset sequences for virtual entries, and type references for superinterfaces.
class Extension {
public:
    struct Content {
        Identifier<Extension> identifier;
        OffsetSequence<MethodDefinition> virtualMethods;
        RefIdentifier<Term> extendedType;
        uint8_t arity;
        RefSequence<Term> interfaces {};
    };

    Identifier<Extension> const GetIdentifier() { return content.identifier; }

    OffsetSequence<MethodDefinition> const GetVirtualMethods() const { return content.virtualMethods; }

    RefIdentifier<Term> const GetExtendedType() const
    {
        return content.extendedType;
    }

    RefSequence<Term> GetInterfaces() const { return content.interfaces; }

    Content const* operator->() const { return &content; }

    Content const* operator*() const { return &content; }

    Extension(Content&& content) : content(content) {}

    Content content;
};

/// Symbolic References, Offset Pools, and Region Data.
///
/// Symbolic references (`MethodReference`, `FieldReference`) and terms are indexed
/// using per-file tables. Each table stores an array of file offsets pointing to the
/// encoded reference metadata.
///
/// Key Components:
/// - MethodReference & FieldReference: Symbolic reference descriptors holding candidate
///   names, type terms, signatures, and call/access flags.
///
/// - OffsetPool & ErasedOffsetPool: Provides lazy ID iteration over encoded reference
///   tables. Incorporates an `adjustment` factor to reserve specific ID ranges—for
///   example, reserving IDs 0–19 for built-in primitive types (`FIRST_NON_PRIMITIVE_TERM_ID`).
///
/// - RegionData: A historical construct that groups term, method, and field offset pools.
///   It was originally designed to partition files into multiple table regions when
///   indices were constrained to 16-bit limits. Although index overflow is no longer
///   an issue due to ULEB128 variable-length encoding, `RegionData` remains as the
///   primary container for a file's reference pools.

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
    Image::FileId file;
    uint32_t offset;
    uint32_t size;
    uint32_t adjustment;
};

template <typename T, uint32_t adjustment = 0> struct OffsetPool {
    static constexpr uint32_t ADJUSTMENT = adjustment;

    struct RefIdIterator {
        Image::FileId file;
        uint32_t id;

        RefIdentifier<T> operator*() const { return RefIdentifier<T>(Image::RefId<T>(id + adjustment), file); }

        RefIdIterator& operator++()
        {
            id++;
            return *this;
        }

        bool operator!=(RefIdIterator const& another) const { return id != another.id; }
    };

    OffsetPool(Image::FileId file, uint32_t offset, uint32_t size) : file(file), offset(offset), size(size) {}

    RefIdIterator begin() const { return RefIdIterator { file, 0 }; }

    RefIdIterator end() const { return RefIdIterator { file, size }; }

    ErasedOffsetPool Erased() const { return { file, offset, size, adjustment }; }

private:
    Image::FileId file;
    uint32_t offset;
    uint32_t size;
};

struct RegionData {
    static constexpr uint32_t FIRST_NON_PRIMITIVE_TERM_ID = 20;

    RegionData(
        OffsetPool<MethodReference> methods,
        OffsetPool<FieldReference> fields,
        OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms
    )
        : methods(methods),
          fields(fields),
          terms(terms)
    {}

    OffsetPool<MethodReference> methods;
    OffsetPool<FieldReference> fields;
    OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms;

    template <typename T> ErasedOffsetPool ErasedPool() const;

    template <> ErasedOffsetPool ErasedPool<Term>() const { return terms.Erased(); }

    template <> ErasedOffsetPool ErasedPool<MethodReference>() const { return methods.Erased(); }

    template <> ErasedOffsetPool ErasedPool<FieldReference>() const { return fields.Erased(); }
};

/// Top-level handle and container representing a single loaded `.cbc` binary module.
///
/// Encapsulates all section offsets, index tables, dependency descriptors, and
/// version metadata parsed from a binary file.
///
/// TODO: Cleanup & Refactoring Tasks:
/// - Get rid of region data.
/// - Section Headers: use one, shared offset which is constant (or adjust raf by this offset).
class CbcFile {
public:
    Image::FileId Id() const;
    uint32_t GetCodeSectionOffs() const;
    uint32_t GetStringSectionOffs() const;
    uint32_t GetTypeDefSectionOffs() const;
    uint32_t GetMethodDefSectionOffs() const;
    uint32_t GetFieldDefSectionOffs() const;
    uint32_t GetTermSectionOffs() const;
    uint32_t GetMethodRefSectionOffs() const;
    uint32_t GetFieldRefSectionOffs() const;
    uint32_t GetAotDataSectionOffs() const;

    String GetPath() const;

    std::optional<Offset<String>> CbcDependencies() const;
    std::optional<Offset<String>> AotDependencies() const;
    const RegionData& GetRegionData() const;
    const TypeIndex& GetTypeIndex() const;
    const std::optional<Identifier<String>> GetMainTypeName() const;

    /// FIXME: tables should be assigned to corresponding regions.
    const DirectCallAotTable& GetDirectCallAotTable() const;
    const VirtualCallAotTable& GetVirtualCallAotTable() const;
    const InterfaceCallAotTable& GetInterfaceCallAotTable() const;
    const StaticFieldAotTable& GetStaticFieldAotTable() const;
    const InstanceFieldAotTable& GetInstanceFieldAotTable() const;

    TypeIndex typeIndex;
    RegionData regionData;

    DirectCallAotTable directCallAotTable;
    VirtualCallAotTable virtualCallAotTable;
    InterfaceCallAotTable interfaceCallAotTable;
    StaticFieldAotTable staticFieldAotTable;
    InstanceFieldAotTable instanceFieldAotTable;

    int aotDeps;
    int cbcDeps;
    std::optional<Identifier<String>> mainTypeName;

    uint32_t poolOffset;
    Image::FileId id;
    std::string name; // TODO: remove `name` and `GetPath`, use `isMain` flag or somehow mark in Engine which file
                      // contains main.
};

// FIXME:
//   - use this constant instead of cbcfile.Get*SectionOffs;
//   - remove garbage from cbc file format and make this constant stable between versions.
static constexpr size_t POOL_OFFSET_ADJUSTMENT = 57;

} // namespace Image
