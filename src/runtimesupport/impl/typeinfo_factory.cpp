#include "runtimesupport/typeinfo_factory.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/type_kind.h"
#include "engine/terms.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/impl/cjnative.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

namespace RTSupport {

// TypeInfo flags
static constexpr uint8_t HAS_REF_FIELD    = 0b00000001;
static constexpr uint8_t HAS_FINALIZER    = 0b00000010;
static constexpr uint8_t FUTURE_CLASS     = 0b00000100;
static constexpr uint8_t MUTEX_CLASS      = 0b00001000;
static constexpr uint8_t MONITOR_CLASS    = 0b00010000;
static constexpr uint8_t WAIT_QUEUE_CLASS = 0b00100000;
static constexpr uint8_t HAS_REFLECTION   = 0b01000000;
static constexpr uint8_t HAS_EXT_PART     = 0b10000000;

enum TypeKind : int8_t {
    // reference type
    TYPE_KIND_CLASS          = -128,
    TYPE_KIND_INTERFACE      = -127,
    TYPE_KIND_RAWARRAY       = -126,
    TYPE_KIND_FUNC           = -125,
    TYPE_KIND_TEMP_ENUM      = -124,
    TYPE_KIND_WEAKREF_CLASS  = -123,
    TYPE_KIND_FOREIGN_PROXY  = -122,
    TYPE_KIND_EXPORTED_REF   = -121,
    TYPE_KIND_GENERIC_TI     = -1,
    TYPE_KIND_GENERIC_CUSTOM = -2,

    // value type
    TYPE_KIND_NOTHING = 0,
    TYPE_KIND_UNIT,
    TYPE_KIND_BOOL,
    TYPE_KIND_RUNE,
    TYPE_KIND_UINT8,
    TYPE_KIND_UINT16 = 5,
    TYPE_KIND_UINT32,
    TYPE_KIND_UINT64,
    TYPE_KIND_UINT_NATIVE,
    TYPE_KIND_INT8,
    TYPE_KIND_INT16 = 10,
    TYPE_KIND_INT32,
    TYPE_KIND_INT64,
    TYPE_KIND_INT_NATIVE,
    TYPE_KIND_FLOAT16,
    TYPE_KIND_FLOAT32 = 15,
    TYPE_KIND_FLOAT64,
    TYPE_KIND_CSTRING,
    TYPE_KIND_CPOINTER,
    TYPE_KIND_CFUNC,
    TYPE_KIND_VARRAY = 20,
    TYPE_KIND_TUPLE,
    TYPE_KIND_STRUCT,
    TYPE_KIND_ENUM,
    TYPE_KIND_MAX,
};

template <typename T> static T* Alloc(size_t cnt = 1) { return reinterpret_cast<T*>(std::malloc(sizeof(T) * cnt)); }

static std::optional<TypeInfo> QueryTypeInfoAOTByName(char const* str);

static char* ConstructTypeInfoName(std::string_view str)
{
    auto size  = str.size();
    auto data  = str.data();
    char* cStr = Alloc<char>(size + 1);
    if (!cStr) {
        return nullptr;
    }

    std::memcpy(cStr, data, size);
    cStr[size] = 0;

    return cStr;
}

/// OuterTI plays the role of "class type context".
/// Example:
/// interface J<T> {
///     func foo(): Unit
/// }
/// interface I<T, K> <: J<T> {
///     func foo(): Unit {
///         // outerTI == TypeInfo[I<T, K>]
///         // Use(outerTI.typeArgs[0], outerTi.typeArgs[1])
///         Use(T, K)
///     }
/// }
/// class Foo <: I<Int64, Int64> {}
/// func use(f: Foo) {
///     // outerTI := GetMethodOuterTI(f.ti, TypeInfo[I<i64, i64>], foo_idx)
///     f.foo()
/// }
/// To use `T` and `K` type vars in `foo` an extra parameter `outerTI` will be passed,
/// which holds TypeInfo of `I<T, K>`.
///
/// These outer ti instances are stored in function table as:
/// [f0, f1, .., fn, ti0, ti1, .., tin]
union OuterTIFuncUnion {
    DYN_FuncPtr func;
    DYN_TypeInfo* typeInfo;
};

/// This class is almost 1-to-1 maps to fields of TypeInfo.
/// The builder is needed mainly to properly manage memory in case of unexpected
/// errors like resolution failures.
///
/// The main building strategy is:
/// - fill out all fields of builder;
/// - create an instance of TypeInfo and move everything to the allocated instance.
///
/// In case of errors, the fields would be freed in destructor. To avoid use-after-free,
/// after successful build, fields of builder are zeroed.
struct TypeInfoBuilder {
    char* name = nullptr;
    int8_t type;
    uint8_t flag      = 0;
    uint16_t fieldNum = 0;

    // holds uint32_t
    int64_t instanceSize  = -1;
    int64_t componentSize = -1;

    DYN_GCTib gctib { .raw = GCTIB_SIGN_BIT };
    StdGCTib* longgctib = nullptr;
    uint32_t uuid       = 0;
    uint8_t align;
    int8_t typeArgsNum          = 0;
    uint16_t validInheritNum    = 0;
    uint32_t* fieldOffsets      = nullptr;
    DYN_FuncPtr finalizerMethod = nullptr;
    DYN_TypeInfo** typeArgs     = nullptr;
    DYN_TypeInfo** fields       = nullptr;

    DYN_TypeInfo* superTypeInfo     = nullptr;
    DYN_TypeInfo* componentTypeInfo = nullptr;

    DYN_ExtensionData** extDefs    = nullptr;
    OuterTIFuncUnion* flatMethods  = nullptr;
    DYN_ExtensionData* flatExtDefs = nullptr;

    Interpretation::FunctionHandle** dataMT = nullptr;

    CbcTypeInfo* typeInfo;

    bool built = false;

    TypeInfoBuilder(CbcTypeInfo* typeInfo) : typeInfo(typeInfo) {}

    DYN_TypeInfo* Build()
    {
        auto typeInfo = this->typeInfo;
        auto result   = &typeInfo->base;

        result->typeInfoName = name;
        result->type         = type;
        result->flag         = flag;
        result->fieldNum     = fieldNum;

        if (instanceSize != -1) {
            result->instanceSize = instanceSize;
        } else if (componentSize != -1) {
            result->componentSize = componentSize;
        } else {
            ASSERTION(false, "neither of instance or component size was set");
        }

        result->gctib           = gctib;
        result->uuid            = uuid;
        result->align           = align;
        result->typeArgsNum     = typeArgsNum;
        result->validInheritNum = validInheritNum;
        result->fieldOffsets    = fieldOffsets;
        result->finalizerMethod = finalizerMethod;
        result->typeArgs        = typeArgs;
        result->fields          = fields;

        if (superTypeInfo) {
            result->superTypeInfo = superTypeInfo;
        } else if (componentTypeInfo) {
            result->componentTypeInfo = componentTypeInfo;
        }

        result->vExtensionDataStart = extDefs;
        result->mTableDesc          = nullptr;
        typeInfo->dataMT            = dataMT;

        built = true;
        return result;
    }

    ~TypeInfoBuilder()
    {
        if (!built) {
            // free is no-op on nulls.
            std::free(longgctib);
            std::free(fieldOffsets);
            std::free(name);
            std::free(typeArgs);
            std::free(fields);
            std::free(dataMT);
            std::free(flatExtDefs);
            std::free(extDefs);
            std::free(flatMethods);
            std::free(typeInfo);
        }
    }
};

struct MethodTableMember {
    Interpretation::FunctionHandle* handle;
    DYN_FuncPtr function;
};

/// Returns pair of (handle, function) that describes member in method table.
static MethodTableMember GetTableMember(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodId, int entryIdx
)
{
    auto method = Symlevel::Reader::Read(session, methodId);
    auto flags  = method.GetFlags();

    ASSERTION(flags.Is(Symlevel::MethodFlag::VIRTUAL), "Only virtual methods are expected");

    if (flags.Is(Symlevel::MethodFlag::ABSTRACT)) {
        // can not be called
        // TODO: put stub method that throws
        return { nullptr, nullptr };
    } else if (flags.Is(Symlevel::MethodFlag::AOT)) {
        // target must be present with aot flag
        auto& manager  = Interpretation::FunctionHandleManager::Of(session);
        auto fuh       = manager.AcquireTagged(session, methodId);
        auto staticFuh = std::get<Interpretation::StaticFunctionHandle*>(fuh);
        return { &staticFuh->base, staticFuh->function };
    } else {
        auto& manager = Interpretation::FunctionHandleManager::Of(session);
        return { manager.Acquire(session, methodId), Adapters::GetDynCallTrampoline(entryIdx) };
    }
}

static bool QuerySubterms(
    std::vector<DYN_TypeInfo*>& typeInfos, Engine::Session& session, Engine::TypeInfoManager& manager, Engine::Term term
);

static std::optional<TypeInfo> QueryTypeInfoAOT(
    Engine::Session& session, Engine::TypeInfoManager& manager, char const* typeName, Engine::Term term
);

static constexpr uint32_t GCTIB_MAX_SHORT_OFFSET = sizeof(void*) * 62;

static std::optional<DYN_GCTib> ConstructGCTib(TypeInfoBuilder& builder, std::vector<uint32_t>& refFieldOffs)
{
    if (refFieldOffs.empty()) {
        return std::make_optional<DYN_GCTib>(DYN_GCTib { .raw = 1ul << 63 });
    }

    auto maxOffset = *std::max_element(refFieldOffs.begin(), refFieldOffs.end());
    if (maxOffset < GCTIB_MAX_SHORT_OFFSET) {
        // Fast path: maximum offset to the reference field is small. We fit it into inline bitset gctib
        uintptr_t gctib = 1ul << 63;
        size_t i        = 1;
        for (auto offs : refFieldOffs) {
            gctib |= (1 << offs / sizeof(uintptr_t));
        }
        return std::make_optional<DYN_GCTib>(DYN_GCTib { .raw = gctib });
    } else {
        // Slow path: it doesn't fit into inline gctib. Thus we construct gctib on the heap and gctib becomes ptr
        StdGCTib* gctib;

        constexpr uint32_t refAlignment = 8;

        auto bitsPerElement = sizeof(gctib->bitmapWords[0]) * 8;
        auto maxIndex       = maxOffset / refAlignment;
        auto maskCount      = maxIndex / refAlignment + 1;
        auto allocAmount    = gctib->nBitmapWords * sizeof(gctib->bitmapWords[0]) + sizeof(*gctib);
        gctib               = static_cast<StdGCTib*>(std::calloc(1, allocAmount));
        gctib->nBitmapWords = maskCount;
        if (!gctib) {
            return std::nullopt;
        }
        for (auto offs : refFieldOffs) {
            auto ref                  = offs / sizeof(uintptr_t);
            auto slot                 = ref / bitsPerElement;
            auto localSlot            = ref % bitsPerElement;
            gctib->bitmapWords[slot] |= (1 << localSlot);
        }
        builder.longgctib = gctib;
        return std::make_optional<DYN_GCTib>(DYN_GCTib { .ptr = gctib });
    }
}

// TODO: factory class, so it can hold state of other managers without recreating them.
// TODO: split function to smaller ones.
static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    auto ident = Engine::ExtractTypeDefIdentifier(term);

    auto type = Symlevel::Reader::Read(session, ident);
    auto name = Symlevel::Reader::Read(session, type.GetName());

    if (type.GetFlags().Is(Symlevel::TypeFlag::AOT)) {
        std::string copiedName(name);
        return QueryTypeInfoAOT(session, manager, copiedName.c_str(), term);
    }

    auto currentTypeInfo = Alloc<CbcTypeInfo>();
    if (!currentTypeInfo) {
        return std::nullopt;
    }

    TypeInfoBuilder builder(currentTypeInfo);

    {
        Stream::StringBuffer stringBuffer;
        Engine::Term(term).GetName(session, stringBuffer);

        // Not guaranteed that name is constructed in the same way as CJNative does.
        // TODO: does it matter?
        builder.name = stringBuffer.ToCString();
    }

    if (builder.name == nullptr) {
        return std::nullopt;
    }

    bool needExtDefs;
    bool needFields;

    auto typeKind = type.GetFlags().GetTypeKind();
    switch (typeKind) {
        case Symlevel::TypeKind::INTERFACE:
            builder.type = TYPE_KIND_INTERFACE;
            needExtDefs  = true;
            needFields   = false;
            break;
        case Symlevel::TypeKind::RECORD:
            builder.type = TYPE_KIND_STRUCT;
            needExtDefs  = true;
            needFields   = true;
            break;
        case Symlevel::TypeKind::CLASS:
            builder.type = TYPE_KIND_CLASS;
            needExtDefs  = true;
            needFields   = true;
            break;
        case Symlevel::TypeKind::LAMBDA:
            builder.type = TYPE_KIND_CLASS;
            needExtDefs  = false;
            needFields   = true;
            break;
        case Symlevel::TypeKind::ENUM:
            builder.type = TYPE_KIND_ENUM;
            needExtDefs  = true;
            needFields   = false;
        default: FATAL("unreachable type kind");
    }

    auto queryTypeInfo = [&session, &manager, term, currentTypeInfo](Engine::Term t
                         ) -> std::optional<RTSupport::TypeInfo> {
        if (t == term) {
            return TypeInfo(&currentTypeInfo->base);
        }
        return manager.AcquireTypeInfo(session, t);
    };

    Engine::ClassSubstitution substitute(session, term);
    auto superType = Engine::TermManager::Resolve(session, type.GetSuperType());
    superType      = substitute(superType);

    if (superType.GetKind() == Engine::TermKind::NIL) {
        // nothing TODO
    } else if (auto superTypeInfo = queryTypeInfo(superType); superTypeInfo.has_value()) {
        builder.superTypeInfo = UnpackTypeInfo(superTypeInfo.value());
    } else {
        // TODO: log
        return std::nullopt;
    }

    if (needExtDefs) { // fill out ext defs
        auto& manager = Engine::MethodTableManager::Of(session);
        auto optMT    = manager.GetMethodTable(session, term);

        if (!optMT.has_value()) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Failed to build method table for " << term << Stream::endl;
            });
            return std::nullopt;
        }
        auto mt = *optMT;

        auto extDefCount        = mt->ClassCount() + mt->InterfaceCount();
        builder.validInheritNum = extDefCount;

        // To simplify memory management here, we will preallocate "flat" arrays
        // where corresponding structures would be filled out.
        // E.g. function tables are essentionally views in the big array.

        auto entryCount = mt->EntryCount();

        struct FuncDesc {
            DYN_FuncPtr ptr;
            Engine::Term declaredType;
        };

        std::vector<FuncDesc> funcDescs;
        funcDescs.resize(entryCount);

        builder.dataMT      = Alloc<Interpretation::FunctionHandle*>(entryCount);
        builder.flatMethods = Alloc<OuterTIFuncUnion>(2 * entryCount);
        builder.extDefs     = Alloc<DYN_ExtensionData*>(extDefCount + 1);
        builder.flatExtDefs = Alloc<DYN_ExtensionData>(extDefCount);

        if (!builder.dataMT || !builder.flatMethods || !builder.extDefs || !builder.flatExtDefs) {
            return std::nullopt;
        }

        // fill out flat methods table and data method table
        int entryIdx = 0;
        for (auto entry : mt->Entries()) {
            auto tm                  = GetTableMember(session, entry.method, entryIdx);
            builder.dataMT[entryIdx] = tm.handle;
            funcDescs[entryIdx]      = FuncDesc { tm.function, entry.genericContext };
            entryIdx++;
        }

        // Fill out array of pointers to ext defs.
        for (int i = 0; i < extDefCount; i++) {
            builder.extDefs[i] = &builder.flatExtDefs[i];
        }

        builder.extDefs[extDefCount] = nullptr;

        auto prepareExtDef = [&builder, currentTypeInfo, &queryTypeInfo, &funcDescs](
                                 DYN_ExtensionData& extDef, Engine::MethodSubTable const& smt
                             ) -> bool {
            // We are maintaining disjoint sub method table ranges!
            auto start      = smt.StartPos();
            auto end        = smt.EndPos();
            auto entryCount = end - start;

            // first `entryCount` slots are func ptrs, next `entryCount` slots are outer ti's.
            auto ft = &builder.flatMethods[2 * start];
            auto tt = &builder.flatMethods[2 * start + entryCount];

            for (int i = 0; i < entryCount; i++) {
                auto desc              = funcDescs[start + i];
                auto funcDeclaringType = queryTypeInfo(desc.declaredType);
                if (!funcDeclaringType.has_value()) {
                    return false;
                }
                ft[i].func     = desc.ptr;
                tt[i].typeInfo = UnpackTypeInfo(*funcDeclaringType);
            }

            uint8_t hasOuterTIFastPath = 0b00000001;

            extDef.funcTable           = reinterpret_cast<DYN_FuncPtr*>(ft);
            extDef.funcTableSize       = entryCount;
            extDef.argNum              = 0;
            extDef.isInterfaceTypeInfo = 1;
            extDef.flag                = hasOuterTIFastPath;
            extDef.whereCondFn         = nullptr;

            extDef.ti = &currentTypeInfo->base;

            auto declaringTypeInfo = queryTypeInfo(smt.DeclaringType());
            if (declaringTypeInfo.has_value()) {
                auto unpacked            = UnpackTypeInfo(declaringTypeInfo.value());
                extDef.interfaceTypeInfo = unpacked;
                if (unpacked == &currentTypeInfo->base) {
                    extDef.flag |= 0b10000000;
                }
                return true;
            } else {
                return false;
            }
        };

        // fill out ext defs
        int extDefIndex = 0;
        for (auto st : mt->Classes()) {
            if (!prepareExtDef(builder.flatExtDefs[extDefIndex++], st)) {
                return std::nullopt;
            }
        }

        for (auto st : mt->Interfaces()) {
            if (!prepareExtDef(builder.flatExtDefs[extDefIndex++], st)) {
                return std::nullopt;
            }
        }
    } else if (typeKind == Symlevel::TypeKind::LAMBDA) {
        auto& manager = Engine::MethodTableManager::Of(session);
        auto optMT    = manager.GetMethodTable(session, term);

        if (!optMT.has_value()) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Failed to build method table for " << term << Stream::endl;
            });
            return std::nullopt;
        }
        auto mt = *optMT;

        // In cbc closures are represented as type definitions with two virtual methods,
        // which are represent "generic" and "instantiated" version of closure.
        // Note, that order of methods is important (first is "generic", second is "instantiated").
        // FIXME: pure generic closure
        auto entryCount = mt->EntryCount();
        ASSERTION(entryCount == 2, "Closures should have only two virtual methods");
        builder.dataMT = Alloc<Interpretation::FunctionHandle*>(entryCount);
        if (!builder.dataMT) {
            return std::nullopt;
        }

        // It is assumed that closures in CBC have only CBC methods (not aot compiled),
        // so we will place a function handle in data mt, which would be referenced
        // by 0/1-indexed trampolines, which would be placed as first and second fields of an object.
        int entryIdx = 0;
        for (auto entry : mt->Entries()) {
            auto tm = GetTableMember(session, entry.method, entryIdx);

            builder.dataMT[entryIdx] = tm.handle;
            entryIdx++;
        }
    }

    if (needFields) {
        auto fieldManager = Engine::FieldLayoutManager::New(session, manager);
        auto optlayout    = fieldManager->GetLayout(term);

        bool hasProperLayout = optlayout.has_value();
        if (hasProperLayout) {
            auto layout = *optlayout;
            if (!layout->desc.size.has_value()) {
                hasProperLayout = false;
            }
        }

        if (!hasProperLayout) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Failed to build field layout for " << term << Stream::endl;
            });
            return std::nullopt;
        }
        auto layout          = *optlayout;
        builder.align        = layout->desc.alignment;
        builder.instanceSize = layout->desc.size.value();
        builder.fieldNum     = layout->fields.size();

        builder.fields       = Alloc<DYN_TypeInfo*>(builder.fieldNum);
        builder.fieldOffsets = Alloc<uint32_t>(builder.fieldNum);

        if (builder.fieldNum != 0 && (builder.fieldOffsets == nullptr || builder.fieldOffsets == nullptr)) {
            return std::nullopt;
        }

        std::vector<uint32_t> refFieldOffs;

        size_t idx = 0;
        for (auto& field : layout->fields) {
            auto fieldType = field.fieldType;
            auto typeInfo  = queryTypeInfo(fieldType);
            if (!typeInfo.has_value()) {
                return std::nullopt;
            }
            auto optOffs = field.offset;
            if (!optOffs.has_value()) {
                return std::nullopt;
            }
            auto fieldId                  = idx++;
            builder.fields[fieldId]       = UnpackTypeInfo(*typeInfo);
            builder.fieldOffsets[fieldId] = *optOffs;
            fieldManager->FillRefOffsets(fieldType, refFieldOffs, optOffs.value());
        }

        if (!refFieldOffs.empty()) {
            builder.flag |= HAS_REF_FIELD;

            auto gctib = ConstructGCTib(builder, refFieldOffs);
            if (!gctib.has_value()) {
                return std::nullopt;
            }
            builder.gctib = *gctib;
        }
    } else if (term.GetKind() == Engine::TermKind::NULLABLE_OPTION) {
        auto underlying = Engine::TermManager::Resolve(session, type.GetEnumType());
        underlying      = substitute.Substitute(underlying);

        builder.fieldNum = 1;

        builder.fields       = Alloc<DYN_TypeInfo*>(builder.fieldNum);
        builder.fieldOffsets = Alloc<uint32_t>(builder.fieldNum);

        if (builder.fieldNum != 0 && (builder.fieldOffsets == nullptr || builder.fieldOffsets == nullptr)) {
            return std::nullopt;
        }

        auto underlyingTI = queryTypeInfo(underlying);
        if (!underlyingTI) {
            return std::nullopt;
        }

        builder.fieldOffsets[0] = 0;
        builder.fields[0]       = UnpackTypeInfo(*underlyingTI);
        builder.gctib           = { .raw = (1ul << 63) | 1 };
        builder.align           = sizeof(void*);
        builder.instanceSize    = sizeof(void*);
    } else if (term.GetKind() == Engine::TermKind::UNION_OPTION) {
        auto fieldManager = Engine::FieldLayoutManager::New(session, manager);
        auto underlying   = Engine::TermManager::Resolve(session, type.GetEnumType());
        underlying        = substitute.Substitute(underlying);

        builder.fieldNum = 1;

        builder.fields       = Alloc<DYN_TypeInfo*>(builder.fieldNum);
        builder.fieldOffsets = Alloc<uint32_t>(builder.fieldNum);

        if (builder.fieldNum != 0 && (builder.fieldOffsets == nullptr || builder.fieldOffsets == nullptr)) {
            return std::nullopt;
        }

        auto underlyingTI = queryTypeInfo(underlying);
        if (!underlyingTI) {
            return std::nullopt;
        }
        // calculation of size/alignment for such kind of term is supported, but without explicit fields
        auto optLayout = fieldManager->GetLayout(term);
        if (!optLayout) {
            return std::nullopt;
        }
        auto layout = *optLayout;
        auto size   = layout->desc.size;
        if (!size) {
            return std::nullopt;
        }

        builder.fieldOffsets[0] = sizeof(uint32_t);
        builder.fields[0]       = UnpackTypeInfo(*underlyingTI);
        builder.gctib           = { .raw = (1ul << 63) | 1 };
        builder.align           = layout->desc.alignment;
        builder.instanceSize    = *size;
    } else {
        builder.fieldNum     = 0;
        builder.fields       = nullptr;
        builder.fieldOffsets = nullptr;
        builder.align        = 1;
        builder.instanceSize = 0;
    }

    builder.typeArgsNum = 0; // Otherwise, runtime would expect type template to be present.
    int typeArgsNum     = term.GetLength();
    if (typeArgsNum > 0) {
        builder.typeArgs = Alloc<DYN_TypeInfo*>(typeArgsNum);
        if (builder.typeArgs == nullptr) {
            return std::nullopt;
        }
        std::vector<DYN_TypeInfo*> typeInfos;
        auto resolved = QuerySubterms(typeInfos, session, manager, term);
        if (!resolved) {
            return std::nullopt;
        }
        for (int i = 0; i < typeArgsNum; i++) {
            builder.typeArgs[i] = typeInfos[i];
        }
    }

    return TypeInfo(builder.Build());
}

static std::optional<TypeInfo> QueryTypeInfoAOTByName(char const* str)
{
    auto res = g_CJNativeInterfaceInstance.typeInfo(str);
    if (res != nullptr) {
        return TypeInfo(res);
    } else {
        return std::nullopt;
    }
}

char const* GetAotTypeName(Engine::Session& session, Engine::Term term)
{
    ASSERT(term.GetKind() == Engine::TermKind::AOT_TYPE);
    auto& manager = Engine::TermManager::Of(session);
    return manager.GetNameOfAotType(Engine::AotTermId(term)).str;
}

/// Find typeinfos of subterms with `nulls` on place of subterms that are not found.
/// Returns `true` if all typeinfos of subterms are found.
static bool QuerySubterms(
    std::vector<DYN_TypeInfo*>& typeInfos, Engine::Session& session, Engine::TypeInfoManager& manager, Engine::Term term
)
{
    bool allResolved = true;
    for (auto i = 0; i < term.GetLength(); i++) {
        auto subterm = term.Subterm(i);

        Log::typeinfo.Log(Logging::Level::TRACE, [&session, &subterm](Stream::Output& out) {
            Stream::ResolvingOutput stream(session, out);
            stream << "querying param " << subterm << Stream::endl;
        });

        auto ti = manager.AcquireTypeInfo(session, subterm);
        TypeInfo info(nullptr);

        if (ti.has_value()) {
            info = *ti;
        } else {
            allResolved = false;
        }
        typeInfos.emplace_back((DYN_TypeInfo*)info.Raw());
    }
    return allResolved;
}

// Can return null!
static void* QueryTypeTemplate(Engine::Session& session, char const* typeName)
{
    auto typeTemplate = g_CJNativeInterfaceInstance.typeTemplate(typeName);
    if (!typeTemplate) {
        Log::typeinfo.Log(Logging::Level::ERROR, [&session, &typeName](Stream::Output& out) {
            Stream::ResolvingOutput stream(session, out);
            stream << "failed to query template " << typeName << Stream::endl;
        });
    }
    return typeTemplate;
}

static std::optional<TypeInfo> QueryTypeInfoAOT(
    Engine::Session& session, Engine::TypeInfoManager& manager, char const* typeName, Engine::Term term
)
{
    ASSERT(!term.IsGeneric());
    if (term.GetLength() > 0) {
        std::vector<DYN_TypeInfo*> infos;

        Log::typeinfo.Log(Logging::Level::TRACE, [&session, &term](Stream::Output& out) {
            Stream::ResolvingOutput stream(session, out);
            stream << "querying aot generic " << term << Stream::endl;
        });

        bool allResolved = QuerySubterms(infos, session, manager, term);
        if (!allResolved) {
            return std::nullopt;
        }

        auto typeTemplate = QueryTypeTemplate(session, typeName);
        if (!typeTemplate) {
            return std::nullopt;
        }
        auto typeInfoG = g_CJNativeInterfaceInstance.getOrCreateTypeInfo(typeTemplate, infos.size(), infos.data());
        return TypeInfo(typeInfoG);
    } else {
        Log::typeinfo.Log(Logging::Level::TRACE, [&session, &term](Stream::Output& out) {
            Stream::ResolvingOutput stream(session, out);
            stream << "querying " << term << Stream::endl;
        });

        auto typeInfo = g_CJNativeInterfaceInstance.typeInfo(typeName);
        if (typeInfo == nullptr) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&session, &term](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "failed to query " << term << Stream::endl;
            });
            return std::nullopt;
        }
        return TypeInfo(typeInfo);
    }
}

static std::optional<TypeInfo> QueryFunctional(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::Term term
)
{
    ASSERT(term.GetKind() == Engine::TermKind::FUNCTIONAL);
    std::vector<DYN_TypeInfo*> infos;

    bool allResolved = QuerySubterms(infos, session, manager, term);
    if (!allResolved) {
        return std::nullopt;
    }

    // CBC encodes return type as last parameter, but CJNative expects it as the first.
    // TODO: maybe we should change encoding?
    auto retType = infos.back();
    auto size    = infos.size();
    for (size_t i = size - 1; i > 0; i--) {
        infos[i] = infos[i - 1];
    }
    infos[0] = retType;

    struct Templates {
        void* cfunc;
        void* closure;
    };

    static auto templates =
        Templates { .cfunc = QueryTypeTemplate(session, "CFunc"), .closure = QueryTypeTemplate(session, "Closure") };

    auto cfuncTypeInfo = g_CJNativeInterfaceInstance.getOrCreateTypeInfo(templates.cfunc, infos.size(), infos.data());
    DYN_TypeInfo* cfuncTIBox[1] = { cfuncTypeInfo };

    auto closureTypeInfo = g_CJNativeInterfaceInstance.getOrCreateTypeInfo(templates.closure, 1, cfuncTIBox);
    return TypeInfo(closureTypeInfo);
}

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    Log::typeinfo.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Stream::ResolvingOutput stream(session, out);
        stream << "start building " << term << Stream::endl;
    });

    ASSERT(!Engine::Term(term).IsGeneric());

    auto createTypeInfo = [&]() -> std::optional<TypeInfo> {
        using namespace Interpretation;
        auto termIdent = term.GetId();
        switch (termIdent.GetKind()) {
            case Engine::TermKind::NULLABLE_OPTION:
            case Engine::TermKind::UNION_OPTION:
            case Engine::TermKind::TYPE: return CreateTypeInfoDyn(session, manager, term);

            case Engine::TermKind::AOT_TYPE:
                return QueryTypeInfoAOT(session, manager, GetAotTypeName(session, term), term);

            case Engine::TermKind::CANGJIE_ARRAY: return QueryTypeInfoAOT(session, manager, "RawArray", term);

            case Engine::TermKind::FUNCTIONAL: return QueryFunctional(session, manager, term);
            case Engine::TermKind::TUPLE:      return QueryTypeInfoAOT(session, manager, "Tuple", term);

            case Engine::TermKind::UNIT:    return builtinTypeInfos[BUILTIN_UNIT];
            case Engine::TermKind::BOOLEAN: return builtinTypeInfos[BUILTIN_BOOLEAN];
            case Engine::TermKind::U8:      return builtinTypeInfos[BUILTIN_U8];
            case Engine::TermKind::I8:      return builtinTypeInfos[BUILTIN_I8];
            case Engine::TermKind::U16:     return builtinTypeInfos[BUILTIN_U16];
            case Engine::TermKind::I16:     return builtinTypeInfos[BUILTIN_I16];
            case Engine::TermKind::U32:     return builtinTypeInfos[BUILTIN_U32];
            case Engine::TermKind::I32:     return builtinTypeInfos[BUILTIN_I32];
            case Engine::TermKind::U64:     return builtinTypeInfos[BUILTIN_U64];
            case Engine::TermKind::I64:     return builtinTypeInfos[BUILTIN_I64];
            case Engine::TermKind::UADDR:   return builtinTypeInfos[BUILTIN_UADDR];
            case Engine::TermKind::IADDR:   return builtinTypeInfos[BUILTIN_IADDR];
            case Engine::TermKind::F16:     return builtinTypeInfos[BUILTIN_F16];
            case Engine::TermKind::F32:     return builtinTypeInfos[BUILTIN_F32];
            case Engine::TermKind::F64:     return builtinTypeInfos[BUILTIN_F64];
            case Engine::TermKind::UCHAR32: return builtinTypeInfos[BUILTIN_RUNE];

            default: {
                FATAL("Not supported yet %d", termIdent.GetKind());
                break;
            }
        }
    };
    auto ti = createTypeInfo();
    Log::typeinfo.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Stream::ResolvingOutput stream(session, out);
        if (ti.has_value()) {
            stream << "successfuly built " << term << " with " << ti->Raw() << Stream::endl;
        } else {
            stream << "failed to build " << term << Stream::endl;
        }
    });
    return ti;
}

Engine::GlobalTerm ReconstructTerm(Engine::Session& session, Engine::TypeInfoManager& manager, TypeInfo ti)
{
    using namespace Engine;
    DYN_TypeInfo* typeInfo = UnpackTypeInfo(ti);

    bool isGeneric, shouldUseComponentType;
    switch (typeInfo->type) {
        case TYPE_KIND_CPOINTER:
        case TYPE_KIND_RAWARRAY:
            isGeneric              = true;
            shouldUseComponentType = true;
            break;
        default:
            isGeneric              = typeInfo->typeArgsNum > 0;
            shouldUseComponentType = false;
            break;
    }

    bool isRef = typeInfo->type < 0;

    switch (typeInfo->type) {
        case TYPE_KIND_TEMP_ENUM:
        case TYPE_KIND_FUNC:
        case TYPE_KIND_GENERIC_CUSTOM:
        case TYPE_KIND_GENERIC_TI:
        case TYPE_KIND_FOREIGN_PROXY:
        case TYPE_KIND_WEAKREF_CLASS:
        case TYPE_KIND_VARRAY:
        case TYPE_KIND_ENUM:           FATAL("type kind %d not implemented yet", typeInfo->type);
    }

    if (isGeneric) {
        auto& termManager = TermManager::Of(session);

        struct DYN_TypeInfo* singleTypeSubterms[1];

        struct DYN_TypeInfo** subTypes;
        int argNum;
        if (shouldUseComponentType) {
            singleTypeSubterms[0] = typeInfo->componentTypeInfo;
            subTypes              = singleTypeSubterms;
            argNum                = 1;
        } else {
            subTypes = typeInfo->typeArgs;
            argNum   = typeInfo->typeArgsNum;
        }

        // TODO: do not use vectors!
        std::vector<Term> subTerms;
        subTerms.resize(argNum);
        for (int i = 0; i < argNum; i++) {
            subTerms[i] = manager.AcquireTerm(session, TypeInfo(subTypes[i]));
        }

        auto g = [&subTerms, &session, &termManager](TermId tk, bool isRef) {
            auto term = termManager.NewTermWithId(session, tk, isRef, subTerms);
            return termManager.Globalize(term);
        };

        switch (typeInfo->type) {
            case TYPE_KIND_RAWARRAY: return g(TagTermId(TermKind::CANGJIE_ARRAY), true);
            case TYPE_KIND_CPOINTER: return g(TagTermId(TermKind::C_POINTER), true);
            case TYPE_KIND_TUPLE:    return g(TagTermId(TermKind::TUPLE), true);

            case TYPE_KIND_STRUCT:
            case TYPE_KIND_INTERFACE:
            case TYPE_KIND_CLASS:     break;

            default: FATAL("Unexpected type kind %d", typeInfo->type);
        }

        // treats the rest as Aot type

        // FIXME: union field
        // FIXME: explicit DYN_TypeTemplate* type
        struct TypeTemplate {
            char* name;
        };

        auto typeTemplate = reinterpret_cast<TypeTemplate*>(typeInfo->finalizerMethod);
        auto name         = typeTemplate->name;

        Term term = termManager.NewAotTerm(session, name, subTerms, isRef);
        return termManager.Globalize(term);
    } else {
        auto g = Term::Predefined;
        switch (typeInfo->type) {
            case TYPE_KIND_NOTHING:     return g(TermKind::NOTHING);
            case TYPE_KIND_UNIT:        return g(TermKind::UNIT);
            case TYPE_KIND_BOOL:        return g(TermKind::BOOLEAN);
            case TYPE_KIND_RUNE:        return g(TermKind::UCHAR32);
            case TYPE_KIND_UINT8:       return g(TermKind::U8);
            case TYPE_KIND_UINT16:      return g(TermKind::U16);
            case TYPE_KIND_UINT32:      return g(TermKind::U32);
            case TYPE_KIND_UINT64:      return g(TermKind::U64);
            case TYPE_KIND_UINT_NATIVE: return g(TermKind::UADDR);
            case TYPE_KIND_INT8:        return g(TermKind::I8);
            case TYPE_KIND_INT16:       return g(TermKind::I16);
            case TYPE_KIND_INT32:       return g(TermKind::I32);
            case TYPE_KIND_INT64:       return g(TermKind::I64);
            case TYPE_KIND_INT_NATIVE:  return g(TermKind::IADDR);
            case TYPE_KIND_FLOAT16:     return g(TermKind::F16);
            case TYPE_KIND_FLOAT32:     return g(TermKind::F32);
            case TYPE_KIND_FLOAT64:     return g(TermKind::F64);

            case TYPE_KIND_STRUCT:
            case TYPE_KIND_INTERFACE:
            case TYPE_KIND_CLASS:     break;

            default: FATAL("Unexpected type kind %d", typeInfo->type);
        }

        // treats the rest as Aot type
        std::vector<Term> noSubTerms;

        auto& termManager = TermManager::Of(session);
        auto name         = typeInfo->typeInfoName;

        Term term = termManager.NewAotTerm(session, name, noSubTerms, isRef);
        return termManager.Globalize(term);
    }
}

} // namespace RTSupport
