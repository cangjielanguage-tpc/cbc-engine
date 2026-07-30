#include "runtimesupport/typeinfo_factory.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/options.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
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
#include <string_view>

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
static void* QueryTypeTemplate(Engine::Session& session, char const* typeName);

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
    DYN_FuncPtr typeTemplateOrFinalizer = nullptr;
    DYN_TypeInfo** typeArgs     = nullptr;
    DYN_TypeInfo** fields       = nullptr;

    DYN_TypeInfo* superTypeInfo = nullptr;

    DYN_ExtensionData** extDefs    = nullptr;
    OuterTIFuncUnion* flatMethods  = nullptr;
    DYN_ExtensionData* flatExtDefs = nullptr;

    Interpretation::FunctionHandle** dataMT = nullptr;

    CbcTypeInfo* typeInfo;

    bool built = false;

    std::string_view aotTypeDefName = "";
    bool isAot                      = false;
    bool needExtDefs                = false;
    bool needFields                 = false;
    bool isLambda                   = false;

    Engine::GlobalTerm term;
    Engine::Session& session;
    Engine::Term superType = Engine::Term::Predefined(Engine::TermKind::NIL);

    TypeInfoBuilder(CbcTypeInfo* typeInfo, Engine::Session& session, Engine::GlobalTerm term)
        : typeInfo(typeInfo),
          session(session),
          term(term)
    {}

    void Identify()
    {
        switch (term.GetKind()) {
            case Engine::TermKind::CANGJIE_ARRAY:
                type           = TYPE_KIND_RAWARRAY;
                needExtDefs    = false;
                needFields     = false;
                isAot          = true;
                aotTypeDefName = "RawArray";
                superType      = term.Subterm(0);
                flag           = HAS_REF_FIELD;
                return;
            case Engine::TermKind::TUPLE:
                type           = TYPE_KIND_TUPLE;
                needExtDefs    = false;
                needFields     = true;
                isAot          = true;
                aotTypeDefName = "Tuple";
                return;
            case Engine::TermKind::VARRAY:
                type           = TYPE_KIND_VARRAY;
                needExtDefs    = false;
                needFields     = false;
                isAot          = true;
                aotTypeDefName = "VArray";
                return;
            default: {
            }
        }
        Engine::ClassSubstitution sub(session, term);
        auto ident     = Engine::ExtractTypeDefIdentifier(term);
        auto def       = Symlevel::Reader::Read(session, ident);
        aotTypeDefName = Symlevel::Reader::Read(session, def.GetName());

        superType = sub.Substitute(Engine::TermManager::Resolve(session, def.GetSuperType()));

        isAot = def.GetFlags().Is(Symlevel::TypeFlag::AOT);
        switch (def.GetFlags().GetTypeKind()) {
            case Symlevel::TypeKind::INTERFACE:
                type        = TYPE_KIND_INTERFACE;
                needExtDefs = !isAot;
                needFields  = false;
                break;
            case Symlevel::TypeKind::RECORD:
                type        = TYPE_KIND_STRUCT;
                needExtDefs = true;
                needFields  = true;
                break;
            case Symlevel::TypeKind::CLASS:
                type        = TYPE_KIND_CLASS;
                needExtDefs = !isAot;
                needFields  = true;
                break;
            case Symlevel::TypeKind::LAMBDA:
                type        = TYPE_KIND_CLASS;
                needExtDefs = false;
                needFields  = true;
                isLambda    = true;
                break;
            case Symlevel::TypeKind::ENUM:
                type        = TYPE_KIND_ENUM;
                needExtDefs = !isAot;
                needFields  = true;
                break;
            default: FATAL("unreachable type kind");
        }
    }

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

        result->uuid            = 0;
        result->gctib           = gctib;
        result->align           = align;
        result->typeArgsNum     = typeArgsNum;
        result->validInheritNum = validInheritNum;
        result->fieldOffsets    = fieldOffsets;
        // This field is union between finalizer and type template.
        result->finalizerMethod = typeTemplateOrFinalizer;
        result->typeArgs        = typeArgs;
        result->fields          = fields;

        result->superTypeInfo = superTypeInfo;

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
            // std::free(longgctib);
            // std::free(fieldOffsets);
            // std::free(name);
            // std::free(typeArgs);
            // std::free(fields);
            // std::free(dataMT);
            // std::free(flatExtDefs);
            // std::free(extDefs);
            // std::free(flatMethods);
            // std::free(typeInfo);
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
        return { manager.Acquire(session, methodId),
                 Adapters::GetDynCallTrampoline(entryIdx, flags.Is(Symlevel::MethodFlag::SRET)) };
    }
}

static bool QuerySubterms(
    std::vector<DYN_TypeInfo*>& typeInfos, Engine::Session& session, Engine::TypeInfoManager& manager, Engine::Term term
);

static std::optional<TypeInfo> QueryTypeInfoAOT(
    Engine::Session& session, Engine::TypeInfoManager& manager, char const* typeName, Engine::Term term
);

static constexpr uint32_t GCTIB_MAX_SHORT_OFFSET = sizeof(uintptr_t) * 62;

static std::optional<DYN_GCTib> ConstructGCTib(TypeInfoBuilder& builder, std::vector<uint32_t>& refFieldOffs)
{
    if (refFieldOffs.empty()) {
        return std::make_optional<DYN_GCTib>(DYN_GCTib { .raw = 1ul << 63 });
    }

    auto maxOffset = *std::max_element(refFieldOffs.begin(), refFieldOffs.end());
    if (Engine::useShortGCTib && maxOffset < GCTIB_MAX_SHORT_OFFSET) {
        // Fast path: maximum offset to the reference field is small. We fit it into inline bitset gctib
        uintptr_t one   = 1;
        uintptr_t gctib = one << 63;
        for (auto offs : refFieldOffs) {
            gctib |= (one << (offs / sizeof(uintptr_t)));
        }
        return std::make_optional<DYN_GCTib>(DYN_GCTib { .raw = gctib });
    } else {
        // Slow path: it doesn't fit into inline gctib. Thus we construct gctib on the heap and gctib becomes ptr
        StdGCTib* gctib;

        constexpr uint32_t refAlignment = 8;

        auto bitsPerElement = sizeof(gctib->bitmapWords[0]) * 8;
        auto maxIndex       = maxOffset / refAlignment;
        auto maskCount      = maxIndex / refAlignment + 1;
        auto allocAmount    = maskCount * sizeof(gctib->bitmapWords[0]) + sizeof(*gctib);
        gctib               = static_cast<StdGCTib*>(std::calloc(1, allocAmount));
        if (!gctib) {
            return std::nullopt;
        }
        gctib->nBitmapWords = maskCount;
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

static int64_t FakeWhereCond() { return -1; }

// TODO: factory class, so it can hold state of other managers without recreating them.
// TODO: split function to smaller ones.
static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    struct TiWrapper {
        uint64_t magic;
        CbcTypeInfo info;
    };

    auto wrapper = Alloc<TiWrapper>();
    if (!wrapper) {
        return std::nullopt;
    }
    wrapper->magic       = 0xfedcba0987654321;
    auto currentTypeInfo = &wrapper->info;

    TypeInfoBuilder builder(currentTypeInfo, session, term);
    {
        Stream::StringBuffer stringBuffer;
        Engine::Term(term).GetName(session, stringBuffer, /* hasDebugPrefix = */ false);

        // Not guaranteed that name is constructed in the same way as CJNative does.
        // TODO: does it matter?
        builder.name = stringBuffer.ToCString();
    }

    // Register term to allow recursive queries.
    // TODO: handle unbounded recurisive queries.
    manager.RegisterPartial(term, TypeInfo(currentTypeInfo));

    if (builder.name == nullptr) {
        return std::nullopt;
    }

    builder.Identify();

    if (builder.isAot && term.GetLength() == 0) {
        std::string typeName(builder.aotTypeDefName);
        return QueryTypeInfoAOT(session, manager, typeName.c_str(), term);
    }

    bool needExtDefs = builder.needExtDefs;
    bool needFields  = builder.needFields;

    auto& termManager = Engine::TermManager::Of(session);

    auto acquireTypeInfo = [&manager, &session](Engine::Term term) {
        ASSERT(term.GetKind() != Engine::TermKind::AOT_TYPE);
        return manager.AcquireTypeInfo(session, term);
    };

    builder.superTypeInfo = nullptr;
    Engine::ClassSubstitution substitute(session, term);
    if (builder.superType.GetKind() != Engine::TermKind::NIL) {
        if (auto super = acquireTypeInfo(builder.superType); super) {
            builder.superTypeInfo = UnpackTypeInfo(*super);
        } else {
            return std::nullopt;
        }
    }

    if (needExtDefs) { // fill out ext defs
        auto& methodTableManager = Engine::MethodTableManager::Of(session);
        auto optMT               = methodTableManager.GetMethodTable(session, term);

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

        bool resolutionFailed = false;
        auto prepareExtDef = [&](DYN_ExtensionData& extDef, Engine::MethodSubTable const& smt) {
            // We are maintaining disjoint sub method table ranges!
            auto start      = smt.StartPos();
            auto end        = smt.EndPos();
            auto entryCount = end - start;

            // first `entryCount` slots are func ptrs, next `entryCount` slots are outer ti's.
            auto ft = &builder.flatMethods[2 * start];
            auto tt = &builder.flatMethods[2 * start + entryCount];

            for (int i = 0; i < entryCount; i++) {
                auto desc = funcDescs[start + i];
                if (auto ti = acquireTypeInfo(desc.declaredType); ti) {
                    tt[i].typeInfo = UnpackTypeInfo(*ti);
                } else {
                    resolutionFailed = true;
                }
                ft[i].func = desc.ptr;
            }

            // Values from cjnative runtime.
            uint8_t hasOuterTIFastPath = 0b00000001;
            uint8_t isFuncTableUpdated = 0b00000110;
            uint8_t isDirect           = 0b10000000;

            extDef.funcTable           = reinterpret_cast<DYN_FuncPtr*>(ft);
            extDef.funcTableSize       = entryCount;
            extDef.argNum              = 0;
            extDef.isInterfaceTypeInfo = 1;
            extDef.flag                = hasOuterTIFastPath | isFuncTableUpdated;
            extDef.whereCondFn         = (void*)&FakeWhereCond;

            extDef.ti = &currentTypeInfo->base;

            if (smt.DeclaringType() == term) {
                extDef.flag |= isDirect;
            }

            auto interface = smt.DeclaringType();
            if (auto ti = acquireTypeInfo(interface); ti) {
                extDef.interfaceTypeInfo = UnpackTypeInfo(*ti);
            } else {
                resolutionFailed = true;
            }
        };

        // fill out ext defs
        int extDefIndex = 0;
        for (auto st : mt->Classes()) {
            prepareExtDef(builder.flatExtDefs[extDefIndex++], st);
        }

        for (auto st : mt->Interfaces()) {
            prepareExtDef(builder.flatExtDefs[extDefIndex++], st);
        }
    } else if (builder.isLambda) {
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
            auto optOffs = field.offset;
            if (!optOffs.has_value()) {
                return std::nullopt;
            }
            auto fieldId = idx++;
            if (auto ti = acquireTypeInfo(fieldType); ti) {
                builder.fields[fieldId] = UnpackTypeInfo(*ti);
            } else {
                return std::nullopt;
            }
            builder.fieldOffsets[fieldId] = *optOffs;
            fieldManager->FillRefOffsets(fieldType, refFieldOffs, optOffs.value());
        }

        // options has insconsistent .offsets property and gctib in cjnative
        // why??
        if (term.GetKind() == Engine::TermKind::OPTION && term.IsReference()) {
            builder.flag  |= HAS_REF_FIELD;
            builder.gctib  = { .raw = (1ull << 63) | 1 };
        } else if (!refFieldOffs.empty()) {
            builder.flag |= HAS_REF_FIELD;

            auto gctib = ConstructGCTib(builder, refFieldOffs);
            if (!gctib.has_value()) {
                return std::nullopt;
            }
            builder.gctib = *gctib;
        }

        Log::typeinfo.Log(Logging::Level::INFO, [&](Stream::Output& out) {
            Stream::ResolvingOutput stream(session, out);
            stream << "Ref offsets for " << term << ":" << Stream::endl;
            for (auto offset : refFieldOffs) {
                stream << " - " << offset << Stream::endl;
            }
            out.PrintFmt("gctib: %lx", builder.gctib.raw);
            out.NewLine();
        });
    } else {
        builder.fieldNum     = 0;
        builder.fields       = nullptr;
        builder.fieldOffsets = nullptr;
        builder.align        = 1;
        builder.instanceSize = 0;
    }

    int typeArgsNum     = term.GetLength();
    builder.typeArgsNum = 0; // set type arg num to zero (so cjnative runtime won't query type templates)
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
        if (builder.isAot) {
            builder.typeArgsNum = typeArgsNum;
            std::string name(builder.aotTypeDefName);
            auto typeTemplate = QueryTypeTemplate(session, name.c_str());

            // FIXME: use RTTypes.h
            struct TypeTemplate {
                char* name;
                int8_t type;
                int8_t flag;
                uint16_t fieldNum;
                uint16_t typeArgNum;
                uint16_t uuid;
                void** fieldFns;
                void* superFn;
                void* finalizer;
                void* info;
                void** extensionDatas;
                uint16_t validInheritNum;
            };

            auto tt = (TypeTemplate*)typeTemplate;

            if (builder.type != TYPE_KIND_TUPLE && builder.type != TYPE_KIND_VARRAY) {
                ASSERT(tt->typeArgNum == typeArgsNum);
                ASSERT(tt->type == builder.type);
                ASSERT(tt->fieldNum == builder.fieldNum);
            }

            builder.typeTemplateOrFinalizer = typeTemplate;
            builder.validInheritNum = tt->validInheritNum;
            builder.extDefs = (DYN_ExtensionData**)tt->extensionDatas;
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

static void* FindTypeSymbol(Engine::Session& session, char const* typeName, char const* suffix)
{
    auto typeInfoName = std::string(typeName) + std::string(suffix);
    for (auto& file : session.GetEngine().Files()) {
        auto& deps = file.GetDependencies();
        auto sym = deps.FindTarget(typeInfoName.c_str());
        if (sym != nullptr) {
            return sym;
        }
    }

    return nullptr;
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
        // CJNative runtime can't find typeInfo with multiple ':' in it.
        // So we need to try to find it with just dlsym.

        typeTemplate = (DYN_TypeInfo*) FindTypeSymbol(session, typeName, ".tt");
        if (typeTemplate == nullptr) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&session, &typeName](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "failed to query template " << typeName << Stream::endl;
            });
        }
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
            // CJNative runtime can't find typeInfo with multiple ':' in it.
            // So we need to try to find it with just dlsym.

            typeInfo = (DYN_TypeInfo*) FindTypeSymbol(session, typeName, ".ti");
            if (typeInfo == nullptr) {
                Log::typeinfo.Log(Logging::Level::ERROR, [&session, &term](Stream::Output& out) {
                    Stream::ResolvingOutput stream(session, out);
                    stream << "failed to query " << term << Stream::endl;
                });
                return std::nullopt;
            }
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

std::optional<TypeInfo> CreateTypeInfo(Engine::Session& session, TypeInfoManager& manager, Engine::GlobalTerm term)
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
            case Engine::TermKind::UNION_ENUM:
            case Engine::TermKind::PRIMITIVE_ENUM:
            case Engine::TermKind::OPTION:
            case Engine::TermKind::TYPE:
            case Engine::TermKind::TUPLE:
            case Engine::TermKind::CANGJIE_ARRAY:
            case Engine::TermKind::VARRAY:  return CreateTypeInfoDyn(session, manager, term);

            case Engine::TermKind::AOT_TYPE:
                return QueryTypeInfoAOT(session, manager, GetAotTypeName(session, term), term);

            case Engine::TermKind::FUNCTIONAL: return QueryFunctional(session, manager, term);

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

Engine::GlobalTerm ReconstructTerm(Engine::Session& session, TypeInfoManager& manager, TypeInfo ti)
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
            case TYPE_KIND_VARRAY:   return g(TagTermId(TermKind::VARRAY), true);

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
