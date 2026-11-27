#include "runtimesupport/typeinfo_factory.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/image/flags.h"
#include "engine/image/type_kind.h"
#include "engine/method_table.h"
#include "engine/options.h"
#include "engine/resolving_output.h"
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
#include "utils/span.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string_view>

namespace RTSupport {

namespace {

enum {
    ERR_OK = 0,
    ERR_RESOLUTION = -1,
    ERR_METHOD_TABLE_FAILURE = -2,
    ERR_OOM = -3,
    ERR_FIELD_LAYOUT_BUILD = -4,
    ERR_FIELD_GCTIB = -5,
    ERR_FIELD_TYPE = -6,
    ERR_VARRAY_TOO_BIG = -7,
};

// TypeInfo flags
constexpr uint8_t HAS_REF_FIELD    = 0b00000001;
constexpr uint8_t HAS_FINALIZER    = 0b00000010;
constexpr uint8_t FUTURE_CLASS     = 0b00000100;
constexpr uint8_t MUTEX_CLASS      = 0b00001000;
constexpr uint8_t MONITOR_CLASS    = 0b00010000;
constexpr uint8_t WAIT_QUEUE_CLASS = 0b00100000;
constexpr uint8_t HAS_REFLECTION   = 0b01000000;
constexpr uint8_t HAS_EXT_PART     = 0b10000000;

constexpr std::string_view CORE_FUTURE_TYPE_NAME      = "std.core:Future";
constexpr std::string_view STD_REF_WEAK_REF_TYPE_NAME = "std.ref:WeakRef";

int64_t FakeWhereCond() { return -1; }
}

template <typename T> static T* Alloc(size_t cnt = 1) { return reinterpret_cast<T*>(std::malloc(sizeof(T) * cnt)); }

// Common allocation unit of the engine-built type infos.
struct TiWrapper {
    uint64_t magic;
    CbcTypeInfo info;
};

// The magic is never checked, it only marks the allocation as an engine-built type info (handy in heap dumps).
static constexpr uint64_t TI_WRAPPER_MAGIC = 0xfedcba0987654321;

static TiWrapper* AllocTiWrapper()
{
    auto wrapper = Alloc<TiWrapper>();
    if (wrapper != nullptr) {
        wrapper->magic = TI_WRAPPER_MAGIC;
    }
    return wrapper;
}

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

struct MethodTableMember {
    Interpretation::FunctionHandle* handle;
    DYN_FuncPtr function;
};

struct TypeInfoBuilder;


static MethodTableMember GetTableMember(
    Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodId, int entryIdx
);

static std::optional<DYN_GCTib> ConstructGCTib(TypeInfoBuilder& builder, Utils::Vector<uint32_t>& refFieldOffs);

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
    int8_t typeArgsNum                  = 0;
    uint16_t validInheritNum            = (1 << 15); // high bit means method table is not initialized
    uint32_t* fieldOffsets              = nullptr;
    DYN_FuncPtr typeTemplateOrFinalizer = nullptr;
    DYN_TypeInfo** typeArgs             = nullptr;
    DYN_TypeInfo** fields               = nullptr;

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

    int errorCode = ERR_OK;

    Engine::GlobalTerm targetTerm;
    Engine::Session& session;
    Engine::Term superType = Engine::Term::Predefined(Engine::TermKind::NIL);

    TypeInfoManager& manager;

    TypeInfoBuilder(CbcTypeInfo* typeInfo, Engine::Session& session, Engine::GlobalTerm term, TypeInfoManager& manager)
        : typeInfo(typeInfo),
          targetTerm(term),
          session(session),
          manager(manager)
    {}

    std::optional<TypeInfo> AcquireTypeInfo(Engine::Term term)
    {
        return manager.AcquireTypeInfo(session, term);
    }

    int InitName()
    {
        Stream::StringBuffer stringBuffer;
        Engine::Term(targetTerm).GetName(session, stringBuffer, /* hasDebugPrefix = */ false);

        // Not guaranteed that name is constructed in the same way as CJNative does.
        // TODO: does it matter?
        name = stringBuffer.ToCString();
        return name != nullptr ? 0 : -1;
    }

    void InitMethodTable()
    {
        if (errorCode != ERR_OK) return;
        auto& methodTableManager = Engine::MethodTableManager::Of(session);
        auto optMT               = methodTableManager.GetMethodTable(session, targetTerm);

        if (!optMT.has_value()) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Failed to build method table for " << targetTerm << Stream::endl;
            });
            errorCode = ERR_METHOD_TABLE_FAILURE;
            return;
        }
        auto mt = *optMT;

        auto extDefCount = mt->ClassCount() + mt->InterfaceCount();
        validInheritNum  = extDefCount;

        // To simplify memory management here, we will preallocate "flat" arrays
        // where corresponding structures would be filled out.
        // E.g. function tables are essentionally views in the big array.

        auto entryCount = mt->EntryCount();

        struct FuncDesc {
            DYN_FuncPtr ptr;
            Engine::Term declaredType;
        };

        Utils::Vector<FuncDesc> funcDescs;
        funcDescs.Resize(entryCount);

        dataMT      = Alloc<Interpretation::FunctionHandle*>(entryCount);
        flatMethods = Alloc<OuterTIFuncUnion>(2 * entryCount);
        extDefs     = Alloc<DYN_ExtensionData*>(extDefCount + 1);
        flatExtDefs = Alloc<DYN_ExtensionData>(extDefCount);

        if (!dataMT || !flatMethods || !extDefs || !flatExtDefs) {
            errorCode = ERR_OOM;
            return;
        }

        // fill out flat methods table and data method table
        int entryIdx = 0;
        for (auto entry : mt->Entries()) {
            auto tm             = GetTableMember(session, entry.method, entryIdx);
            dataMT[entryIdx]    = tm.handle;
            funcDescs[entryIdx] = FuncDesc { tm.function, entry.genericContext };
            entryIdx++;
        }

        // Fill out array of pointers to ext defs.
        for (int i = 0; i < extDefCount; i++) {
            extDefs[i] = &flatExtDefs[i];
        }

        extDefs[extDefCount] = nullptr;

        bool resolutionFailed = false;
        auto prepareExtDef    = [&](DYN_ExtensionData& extDef, Engine::MethodSubTable const& smt) {
            // We are maintaining disjoint sub method table ranges!
            auto start      = smt.StartPos();
            auto end        = smt.EndPos();
            auto entryCount = end - start;

            // first `entryCount` slots are func ptrs, next `entryCount` slots are outer ti's.
            auto ft = &flatMethods[2 * start];
            auto tt = &flatMethods[2 * start + entryCount];

            for (int i = 0; i < entryCount; i++) {
                auto desc = funcDescs[start + i];
                if (auto ti = AcquireTypeInfo(desc.declaredType); ti) {
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

            extDef.ti = &typeInfo->base;

            if (smt.DeclaringType() == targetTerm) {
                extDef.flag |= isDirect;
            }

            auto interface = smt.DeclaringType();
            if (auto ti = AcquireTypeInfo(interface); ti) {
                extDef.interfaceTypeInfo = UnpackTypeInfo(*ti);
            } else {
                resolutionFailed = true;
            }
        };

        // fill out ext defs
        int extDefIndex = 0;
        for (auto st : mt->Classes()) {
            prepareExtDef(flatExtDefs[extDefIndex++], st);
        }

        for (auto st : mt->Interfaces()) {
            prepareExtDef(flatExtDefs[extDefIndex++], st);
        }
    }

    void InitLambdaMethods()
    {
        if (errorCode != ERR_OK) return;
        auto& manager = Engine::MethodTableManager::Of(session);
        auto optMT    = manager.GetMethodTable(session, targetTerm);

        if (!optMT.has_value()) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Failed to build method table for " << targetTerm << Stream::endl;
            });
            errorCode = ERR_METHOD_TABLE_FAILURE;
            return;
        }
        auto mt = *optMT;

        // In cbc closures are represented as type definitions with two virtual methods,
        // which are represent "generic" and "instantiated" version of closure.
        // Note, that order of methods is important (first is "generic", second is "instantiated").
        // FIXME: pure generic closure
        auto entryCount = mt->EntryCount();
        dataMT  = Alloc<Interpretation::FunctionHandle*>(entryCount);
        if (!dataMT) {
            errorCode = ERR_OOM;
            return;
        }
        for (int i = 0; i < entryCount; i++) {
            dataMT[i] = (Interpretation::FunctionHandle*)0x1234567890abcdef;
        }

        // It is assumed that closures in CBC have only CBC methods (not aot compiled),
        // so we will place a function handle in data mt, which would be referenced
        // by 0/1-indexed trampolines, which would be placed as first and second fields of an object.
        int entryIdx = 0;
        for (auto entry : mt->Entries()) {
            auto tm = GetTableMember(session, entry.method, entryIdx);

            dataMT[entryIdx] = tm.handle;
            entryIdx++;
        }
    }

    void InitFields(bool needGcTib)
    {
        if (errorCode != ERR_OK) return;
        auto fieldManager = Engine::FieldLayoutManager::New(session, manager);
        auto optlayout    = fieldManager->GetLayout(targetTerm);

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
                stream << "Failed to build field layout for " << targetTerm << Stream::endl;
            });
            errorCode = ERR_FIELD_LAYOUT_BUILD;
            return;
        }
        auto layout          = *optlayout;
        align        = layout->desc.alignment;
        instanceSize = layout->desc.size.value();
        fieldNum     = layout->fields.Size();

        fields       = Alloc<DYN_TypeInfo*>(fieldNum);
        fieldOffsets = Alloc<uint32_t>(fieldNum);

        if (fieldNum != 0 && (fieldOffsets == nullptr || fieldOffsets == nullptr)) {
            errorCode = ERR_OOM;
            return;
        }

        Utils::Vector<uint32_t> refFieldOffs;

        auto acquireFieldTi = [this](Engine::Term term) {
            if (term.GetKind() == Engine::TermKind::BOX) {
                // Boxed fields in AOT code are represented as
                // special TypeInfo that has `CLASS` kind and
                // the **exact same name** as TI of boxed type.
                //
                // They do not have a special kind of `Box` TI
                // that represents such fields. Instead of this,
                // they are using such a hacky solution to make
                // `TypeInfoSize` and similar routines work properly.
                // Additionally, it helps with their implementation
                // of reflection.
                //
                // It seems this behavior is not needed for execution purposes at all.
                // So we will use Object to represent such fields,
                // since we do not (and will not) support reflection using
                // TypeInfo/TypeTemplate machinery.
                return QueryTypeInfoAOTByName("std.core:Object");
            }
            return AcquireTypeInfo(term);
        };

        size_t idx = 0;
        for (auto& field : layout->fields) {
            auto fieldType = field.fieldType;
            auto optOffs   = *field.offset; // offset can't be invalid if field layout is proper.
            auto fieldId = idx++;
            if (auto ti = acquireFieldTi(fieldType); ti) {
                fields[fieldId] = UnpackTypeInfo(*ti);
            } else {
                errorCode = ERR_FIELD_TYPE;
                return;
            }
            fieldOffsets[fieldId] = optOffs;
            fieldManager->FillRefOffsets(fieldType, refFieldOffs, optOffs);
        }

        if (needGcTib && !refFieldOffs.Empty()) {
            flag |= HAS_REF_FIELD;

            auto gctib = ConstructGCTib(*this, refFieldOffs);
            if (!gctib) {
                errorCode = ERR_FIELD_GCTIB;
                return;
            }
            this->gctib = *gctib;

            Log::typeinfo.Log(Logging::Level::INFO, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Ref offsets for " << targetTerm << ":" << Stream::endl;
                for (auto offset : refFieldOffs) {
                    stream << " - " << offset << Stream::endl;
                }
                out.PrintFmt("gctib: %lx", this->gctib);
                out.NewLine();
            });
        }
    }

    void InitVArrayFields()
    {
        if (errorCode != ERR_OK) return;
        auto length = Engine::VArrayTermId(targetTerm).GetNum();
        if (length > UINT16_MAX) {
            errorCode = ERR_VARRAY_TOO_BIG;
            return;
        }
        fieldNum = static_cast<uint16_t>(length);
        auto fields      = Engine::FieldLayoutManager::New(session, manager);
        auto size        = fields->GetFlatSize(targetTerm);
        if (!size) {
            errorCode = ERR_FIELD_LAYOUT_BUILD;
            return;
        }
        align        = fields->GetFlatAlignment(targetTerm);
        instanceSize = *size;
        Utils::Vector<uint32_t> offsets;
        fields->FillRefOffsets(targetTerm, offsets, 0);
        if (!offsets.Empty()) {
            flag |= HAS_REF_FIELD;
            auto gctib    = ConstructGCTib(*this, offsets);
            if (!gctib) {
                errorCode = ERR_FIELD_GCTIB;
                return;
            }
            this->gctib = *gctib;
        }
    }

    void InitNoFields()
    {
        if (errorCode != ERR_OK) return;
        fieldNum     = 0;
        fields       = nullptr;
        fieldOffsets = nullptr;
        align        = 1;
        instanceSize = 0;
    }

    void InitTypeArgs()
    {
        if (errorCode != ERR_OK) return;
        if (targetTerm.GetLength() == 0) return; // Only generic types are processed
        typeArgsNum = targetTerm.GetLength();

        this->typeArgs = Alloc<DYN_TypeInfo*>(typeArgsNum);
        if (this->typeArgs == nullptr) {
            return;
        }
        for (int i = 0; i < typeArgsNum; i++) {
            auto ti = manager.AcquireTypeInfo(session, targetTerm.Subterm(i));
            if (!ti) {
                errorCode = ERR_RESOLUTION;
                return;
            }
            this->typeArgs[i] = (DYN_TypeInfo*) ti->Raw();
        }
    }

    void FixupAotGenericType(char const* typeDefName, int typeArgsNum)
    {
        if (errorCode != ERR_OK) return;
        if (targetTerm.GetLength() == 0) return; // Only generic types are processed

        this->typeArgsNum = typeArgsNum;
        auto typeTemplate = QueryTypeTemplate(session, typeDefName);

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

        if (tt->type == TYPE_KIND_TEMP_ENUM) {
            // Workaround for #159.
            // `TEMP_ENUM` tag is needed for cjnative reflection support.
            // In other cases, `TEMP_ENUM` behaves exactly as usual classes.
            ASSERT(TYPE_KIND_CLASS == this->type);
            this->type = TYPE_KIND_TEMP_ENUM;
        }

        // Sanity checks: our typeinfo creation routines are working in the same way as CJNative one.
        if (this->type != TYPE_KIND_TUPLE && this->type != TYPE_KIND_VARRAY) {
            ASSERT(tt->typeArgNum == typeArgsNum);
            ASSERT(tt->type == this->type);
            ASSERT(tt->fieldNum == this->fieldNum);
        }

        this->typeTemplateOrFinalizer = typeTemplate;
        this->validInheritNum         = this->type == TYPE_KIND_VARRAY ? (1 << 15) : tt->validInheritNum;

        this->extDefs = this->type == TYPE_KIND_VARRAY ? nullptr : (DYN_ExtensionData**)tt->extensionDatas;
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

        Log::typeinfo.Log(Logging::Level::INFO, [&](Stream::Output& out) {
            Stream::ResolvingOutput stream(session, out);
            stream << "TypeInfo for " << targetTerm << ":\n"
                   << "\t typeInfoName: " << result->typeInfoName << "\n"
                   << "\t type: " << result->type << "\n"
                   << "\t flag: " << result->flag << "\n"
                   << "\t fieldNum: " << result->fieldNum << "\n"
                   << "\t instance(component)Size: "
                   << ((result->instanceSize != -1) ? result->instanceSize : result->componentSize) << "\n"
                   << "\t gctib: " << result->gctib.raw << "\n"
                   << "\t uuid: " << result->uuid << "\n"
                   << "\t align: " << result->align << "\n"
                   << "\t typeArgsNum: " << result->typeArgsNum << "\n"
                   << "\t validInheritNum: " << result->validInheritNum << "\n"
                   << "\t fieldOffsets: " << result->fieldOffsets << "\n"
                   << "\t finalizerMethod: " << result->finalizerMethod << "\n"
                   << "\t typeArgs: " << result->typeArgs << "\n"
                   << "\t fields: " << result->fields << "\n"
                   << "\t super/componentTypeInfo: " << result->superTypeInfo << "\n"
                   << "\t super/componentTypeName: "
                   << (result->superTypeInfo ? result->superTypeInfo->typeInfoName : "<none>") << "\n"
                   << "\t vExtensionDataStart: " << result->vExtensionDataStart << "\n"
                   << "\t mTableDesc: " << result->mTableDesc << "\n"
                   << "\t reflectOrDebugInfo: " << result->reflectOrDebugInfo;
            out.NewLine();
        });
        return result;
    }

    ~TypeInfoBuilder()
    {
        if (!built) {
            // free is no-op on nulls.
            // FIXME: use after free
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

/// Returns pair of (handle, function) that describes member in method table.
static MethodTableMember GetTableMember(
    Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodId, int entryIdx
)
{
    auto method = Decode::Read(session, methodId);
    auto flags  = method.GetFlags();

    ASSERTION(flags.Is(Image::MethodFlag::VIRTUAL), "Only virtual methods are expected");

    auto dynamicMember = [&](Interpretation::DynamicFunctionHandle* fuh) -> MethodTableMember {
        // Static virtual methods keep their slot for I2I dispatch, but fail explicitly when called from native code.
        auto function = Adapters::GetDynCallTrampoline(
            entryIdx, flags.Is(Image::MethodFlag::SRET), method->abiKind == Image::MethodAbiKind::HAS_THIS_TI
        );
        return { &fuh->base, function };
    };

    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    if (flags.Is(Image::MethodFlag::ABSTRACT)) {
        return dynamicMember(manager.GetHelper(Interpretation::Helper::ThrowAbstractMethodCallError));
    } else if (flags.Is(Image::MethodFlag::AOT)) {
        // target must be present with aot flag
        auto fuh       = manager.AcquireTagged(session, methodId);
        auto staticFuh = std::get<Interpretation::StaticFunctionHandle*>(fuh);
        return { &staticFuh->base, staticFuh->function };
    } else {
        auto fuh = manager.AcquireTagged(session, methodId);
        return dynamicMember(std::get<Interpretation::DynamicFunctionHandle*>(fuh));
    }
}

static bool QuerySubterms(
    Utils::Vector<DYN_TypeInfo*>& typeInfos,
    Engine::Session& session,
    Engine::TypeInfoManager& manager,
    Engine::Term term
);

static std::optional<TypeInfo> QueryTypeInfoAOT(
    Engine::Session& session, Engine::TypeInfoManager& manager, char const* typeName, Engine::Term term
);

static constexpr uint32_t GCTIB_MAX_SHORT_OFFSET = sizeof(uintptr_t) * 62;

static std::optional<DYN_GCTib> ConstructGCTib(TypeInfoBuilder& builder, Utils::Vector<uint32_t>& refFieldOffs)
{
    if (refFieldOffs.Empty()) {
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

static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, TypeInfoManager& manager, Engine::GlobalTerm term
);

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
    for (auto& fileDeps : session.GetEngine().Dependencies()) {
        auto sym = fileDeps.FindSymbol(typeInfoName.c_str());
        if (sym != nullptr) {
            return sym;
        }
    }

    return nullptr;
}

/// Find typeinfos of subterms with `nulls` on place of subterms that are not found.
/// Returns `true` if all typeinfos of subterms are found.
static bool QuerySubterms(
    Utils::Vector<DYN_TypeInfo*>& typeInfos,
    Engine::Session& session,
    Engine::TypeInfoManager& manager,
    Engine::Term term
)
{
    typeInfos.Reserve(term.GetLength());
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
        typeInfos.EmplaceBack((DYN_TypeInfo*)info.Raw());
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

        typeTemplate = (DYN_TypeInfo*)FindTypeSymbol(session, typeName, ".tt");
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
        Utils::Vector<DYN_TypeInfo*> infos;

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
        auto typeInfoG = g_CJNativeInterfaceInstance.getOrCreateTypeInfo(typeTemplate, infos.Size(), infos.Data());
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

            typeInfo = (DYN_TypeInfo*)FindTypeSymbol(session, typeName, ".ti");
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
    Utils::Vector<DYN_TypeInfo*> infos;

    bool allResolved = QuerySubterms(infos, session, manager, term);
    if (!allResolved) {
        return std::nullopt;
    }

    // CBC encodes return type as last parameter, but CJNative expects it as the first.
    // TODO: maybe we should change encoding?
    auto retType = infos.Back();
    auto size    = infos.Size();
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

    auto cfuncTypeInfo = g_CJNativeInterfaceInstance.getOrCreateTypeInfo(templates.cfunc, infos.Size(), infos.Data());
    DYN_TypeInfo* cfuncTIBox[1] = { cfuncTypeInfo };

    auto closureTypeInfo = g_CJNativeInterfaceInstance.getOrCreateTypeInfo(templates.closure, 1, cfuncTIBox);
    return TypeInfo(closureTypeInfo);
}

// Shapes of the built-in primitive type infos.
struct BuiltinTypeInfoSpec {
    Interpretation::BuiltinType typeKind;
    char const* name;
    TypeKind type;
    uint32_t instanceSize;
    uint8_t align;
};

constexpr BuiltinTypeInfoSpec builtinTypeInfoSpecs[] = {
    {Interpretation::BUILTIN_UNIT,    "Unit",       TYPE_KIND_UNIT,        0,             1},
    {Interpretation::BUILTIN_BOOLEAN, "Bool",       TYPE_KIND_BOOL,        1,             1},
    {Interpretation::BUILTIN_U8,      "UInt8",      TYPE_KIND_UINT8,       1,             1},
    {Interpretation::BUILTIN_I8,      "Int8",       TYPE_KIND_INT8,        1,             1},
    {Interpretation::BUILTIN_U16,     "UInt16",     TYPE_KIND_UINT16,      2,             2},
    {Interpretation::BUILTIN_I16,     "Int16",      TYPE_KIND_INT16,       2,             2},
    {Interpretation::BUILTIN_U32,     "UInt32",     TYPE_KIND_UINT32,      4,             4},
    {Interpretation::BUILTIN_I32,     "Int32",      TYPE_KIND_INT32,       4,             4},
    {Interpretation::BUILTIN_U64,     "UInt64",     TYPE_KIND_UINT64,      8,             8},
    {Interpretation::BUILTIN_I64,     "Int64",      TYPE_KIND_INT64,       8,             8},
    {Interpretation::BUILTIN_UADDR,   "UIntNative", TYPE_KIND_UINT_NATIVE, sizeof(void*), sizeof(void*)},
    {Interpretation::BUILTIN_IADDR,   "IntNative",  TYPE_KIND_INT_NATIVE,  sizeof(void*), sizeof(void*)},
    {Interpretation::BUILTIN_F16,     "Float16",    TYPE_KIND_FLOAT16,     2,             2},
    {Interpretation::BUILTIN_F32,     "Float32",    TYPE_KIND_FLOAT32,     4,             4},
    {Interpretation::BUILTIN_F64,     "Float64",    TYPE_KIND_FLOAT64,     8,             8},
    {Interpretation::BUILTIN_RUNE,    "Rune",       TYPE_KIND_RUNE,        4,             4},
    {Interpretation::BUILTIN_NOTHING, "Nothing",    TYPE_KIND_NOTHING,     0,             1},
    {Interpretation::BUILTIN_CSTRING, "CString",    TYPE_KIND_CSTRING,     sizeof(void*), sizeof(void*)},
};

static_assert(Interpretation::BUILTIN_COUNT == std::size(builtinTypeInfoSpecs));

static TypeInfo CreateBuiltinTypeInfo(BuiltinTypeInfoSpec const& spec)
{
    auto wrapper = AllocTiWrapper();
    if (!wrapper) {
        FATAL("failed to allocate type info for the builtin type %s", spec.name);
    }

    // Zero initialization gives the AOT std-core shape of a primitive type info:
    // flag = 0, fieldNum = 0, gctib = 0, uuid = 0, validInheritNum = 0, and null
    // fields/typeArgs/extension data/method table.
    auto info = &wrapper->info;
    *info     = CbcTypeInfo{};

    info->base.typeInfoName = spec.name;
    info->base.type         = static_cast<int8_t>(spec.type);
    info->base.instanceSize = spec.instanceSize;
    info->base.align        = spec.align;

    Log::typeinfo.Log(Logging::Level::INFO, [&spec](Stream::Output& out) {
        out.PrintFmt(
            "Builtin type info %s: type=%d instanceSize=%u align=%u", spec.name, spec.type, spec.instanceSize, spec.align
        );
        out.NewLine();
    });

    return TypeInfo(info);
}

void InitBuiltinTypeInfos()
{
    for (auto const& spec : builtinTypeInfoSpecs) {
        Interpretation::builtinTypeInfos[spec.typeKind] = CreateBuiltinTypeInfo(spec);
    }
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
            case Engine::TermKind::C_POINTER:
            case Engine::TermKind::VARRAY:
            case Engine::TermKind::CANGJIE_ARRAY:
            case Engine::TermKind::GENERIC_CONTEXT: return CreateTypeInfoDyn(session, manager, term);

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
            case Engine::TermKind::BSTRING: return builtinTypeInfos[BUILTIN_CSTRING];
            case Engine::TermKind::NOTHING: return builtinTypeInfos[BUILTIN_NOTHING];

            case Engine::TermKind::PACKED_TUPLE_HACK: {
                FATAL("Should not reach here: packed tuple");
                break;
            }

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

// Native reference enums use ordinary class definitions in current CBC metadata.
static Engine::Term ReconstructReferenceEnum(
    Engine::Session& session, std::string_view name, Utils::Span<Engine::Term const> arguments
)
{
    auto type = session.GetEngine().FindType(session, name);
    if (type.has_value()) {
        auto def = Decode::Read(session, *type);
        if (!def.GetFlags().Is(Image::TypeKind::CLASS) || def->enumKind != Image::EnumKind::NOT_ENUM) {
            FATAL("Native TEMP_ENUM %.*s requires a CBC class definition", int(name.size()), name.data());
        }
        if (def->arity != arguments.Size()) {
            FATAL("Native TEMP_ENUM %.*s has incompatible CBC arity", int(name.size()), name.data());
        }
    }
    return Engine::TermManager::Of(session).NewAotTerm(session, name, arguments, true);
}

Engine::GlobalTerm ReconstructTerm(Engine::Session& session, TypeInfoManager& manager, TypeInfo ti)
{
    using namespace Engine;
    DYN_TypeInfo* typeInfo = UnpackTypeInfo(ti);

    bool isGeneric, shouldUseComponentType;
    switch (typeInfo->type) {
        case TYPE_KIND_CPOINTER:
        case TYPE_KIND_RAWARRAY:
        case TYPE_KIND_VARRAY:
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
        case TYPE_KIND_FUNC:
        case TYPE_KIND_GENERIC_CUSTOM:
        case TYPE_KIND_GENERIC_TI:
        case TYPE_KIND_FOREIGN_PROXY:  FATAL("type kind %d not implemented yet", typeInfo->type);
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
        Utils::Vector<Term> subTerms;
        subTerms.Resize(argNum);
        for (int i = 0; i < argNum; i++) {
            subTerms[i] = manager.AcquireTerm(session, TypeInfo(subTypes[i]));
        }

        auto g = [&subTerms, &session, &termManager](TermId tk, bool isRef) {
            auto term = TermManager::NewTermWithId(session, tk, isRef, subTerms);
            return termManager.Globalize(term);
        };

        bool isEnum = false;

        switch (typeInfo->type) {
            case TYPE_KIND_VARRAY:   return g(VArrayTermId(typeInfo->fieldNum), false);
            case TYPE_KIND_RAWARRAY: return g(TagTermId(TermKind::CANGJIE_ARRAY), true);
            case TYPE_KIND_CPOINTER: return g(TagTermId(TermKind::C_POINTER), true);
            case TYPE_KIND_TUPLE:    return g(TagTermId(TermKind::TUPLE), true);

            case TYPE_KIND_STRUCT:
            case TYPE_KIND_INTERFACE:
            case TYPE_KIND_CLASS:
            case TYPE_KIND_WEAKREF_CLASS: break;
            case TYPE_KIND_TEMP_ENUM:     break;
            case TYPE_KIND_ENUM:          isEnum = true; break;

            default: FATAL("Unexpected type kind %d", typeInfo->type);
        }

        // treats the rest as Aot type

        // FIXME: union field
        // FIXME: explicit DYN_TypeTemplate* type
        struct TypeTemplate {
            char* name;
        };

        // TypeTemplate is stored in an union with `finalizerMethod`.
        // FIXME: proper headers from cjnative runtime side.
        auto typeTemplate = reinterpret_cast<TypeTemplate*>(typeInfo->finalizerMethod);
        auto name         = typeTemplate->name;

        Term term;
        if (typeInfo->type == TYPE_KIND_TEMP_ENUM) {
            term = ReconstructReferenceEnum(session, name, subTerms);
        } else if (isEnum) {
            term = termManager.NewEnumTerm(session, name, subTerms);
        } else {
            term = termManager.NewAotTerm(session, name, subTerms, isRef);
        }
        return termManager.Globalize(term);
    } else {
        auto g      = Term::Predefined;
        bool isEnum = false;
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
            case TYPE_KIND_CSTRING:     return g(TermKind::BSTRING);

            case TYPE_KIND_STRUCT:
            case TYPE_KIND_INTERFACE:
            case TYPE_KIND_CLASS:
            case TYPE_KIND_WEAKREF_CLASS: break;
            case TYPE_KIND_TEMP_ENUM:     break;
            case TYPE_KIND_ENUM:          isEnum = true; break;

            default: FATAL("Unexpected type kind %d", typeInfo->type);
        }

        // treats the rest as Aot type
        Utils::Vector<Term> noSubTerms;

        auto& termManager = TermManager::Of(session);
        auto name         = typeInfo->typeInfoName;
        Term term;

        if (typeInfo->type == TYPE_KIND_TEMP_ENUM) {
            term = ReconstructReferenceEnum(session, name, noSubTerms);
        } else if (isEnum) {
            term = termManager.NewEnumTerm(session, name, noSubTerms);
        } else {
            term = termManager.NewAotTerm(session, name, noSubTerms, isRef);
        }
        return termManager.Globalize(term);
    }
}

static void BuildTypeDef(TypeInfoBuilder& builder, bool needGcTib)
{
    auto ident = Engine::ExtractTypeDefIdentifier(builder.targetTerm);
    auto def   = Decode::Read(builder.session, ident);
    bool isAot = def.GetFlags().Is(Image::TypeFlag::AOT);
    auto name  = Decode::Read(builder.session, def.GetName());

    TypeKind type;
    switch (def.GetFlags().GetTypeKind()) {
        case Image::TypeKind::INTERFACE: type = TYPE_KIND_INTERFACE; break;
        case Image::TypeKind::RECORD:    type = TYPE_KIND_STRUCT;    break;
        case Image::TypeKind::LAMBDA:    type = TYPE_KIND_CLASS;     break;
        case Image::TypeKind::ENUM:      type = TYPE_KIND_ENUM;      break;

        case Image::TypeKind::CLASS: {
            if (name.compare(STD_REF_WEAK_REF_TYPE_NAME) == 0) {
                type = TYPE_KIND_WEAKREF_CLASS;
            } else {
                type = TYPE_KIND_CLASS;
            }
        }
    }

    builder.type = type;

    if (builder.type != TYPE_KIND_INTERFACE) {
        builder.InitFields(needGcTib);
    }
    if (!isAot) {
        builder.InitMethodTable();
    } else {
        std::string cname(name);
        builder.FixupAotGenericType(cname.c_str(), builder.targetTerm.GetLength());
    }
    builder.InitTypeArgs();
}

static void BuildOption(TypeInfoBuilder& builder)
{
    auto term = builder.targetTerm;
    bool isRefOption = term.GetKind() == Engine::TermKind::OPTION && term.IsReference();
    BuildTypeDef(builder, !isRefOption);
    if (isRefOption) {
        // options has insconsistent .offsets property and gctib in cjnative
        // why??
        builder.flag |= HAS_REF_FIELD;
        builder.gctib = {.raw = (1ull << 63) | 1 };
    }
}

static void BuildGenericContext(TypeInfoBuilder& builder)
{
    builder.type = TYPE_KIND_STRUCT;
    builder.InitNoFields();
    builder.InitTypeArgs();
}

static void BuildLambda(TypeInfoBuilder& builder)
{
    builder.type = TYPE_KIND_CLASS;
    builder.InitLambdaMethods();
    builder.InitFields(true);
    builder.InitTypeArgs();
}

static void BuildRawArray(TypeInfoBuilder& builder)
{
    builder.type = TYPE_KIND_RAWARRAY;
    builder.InitNoFields();
    builder.superType = builder.targetTerm.Subterm(0);

    builder.InitNoFields();
    builder.flag = HAS_REF_FIELD;
    builder.gctib = { .ptr = nullptr };
    builder.align = 8;
    builder.instanceSize = 8;

    builder.InitTypeArgs();
}

static void BuildTuple(TypeInfoBuilder& builder)
{
    builder.type = TYPE_KIND_TUPLE;
    builder.InitFields(true);
    builder.FixupAotGenericType("Tuple", builder.targetTerm.GetLength());
    builder.InitTypeArgs();
}

static void BuildVArray(TypeInfoBuilder& builder)
{
    builder.type      = TYPE_KIND_VARRAY;
    builder.superType = builder.targetTerm.Subterm(0);
    builder.InitVArrayFields();
    builder.FixupAotGenericType("VArray", 0);
}

static void BuildCPointer(TypeInfoBuilder& builder)
{
    builder.type = TYPE_KIND_CPOINTER;

    builder.InitNoFields();
    builder.align        = 8;
    builder.instanceSize = 8;

    builder.superType     = builder.targetTerm.Subterm(0);
    builder.FixupAotGenericType("CPointer", 0);
}

static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, TypeInfoManager& manager, Engine::GlobalTerm term
) {
    auto wrapper = AllocTiWrapper();
    if (!wrapper) {
        return std::nullopt;
    }
    auto currentTypeInfo = &wrapper->info;

    TypeInfoBuilder builder(currentTypeInfo, session, term, manager);
    switch (term.GetKind()) {
        case Engine::TermKind::CANGJIE_ARRAY:
            BuildRawArray(builder);
            break;
        case Engine::TermKind::VARRAY:
            BuildVArray(builder);
            break;
        case Engine::TermKind::TUPLE:
            BuildTuple(builder);
            break;
        case Engine::TermKind::C_POINTER:
            BuildCPointer(builder);
            break;
        case Engine::TermKind::GENERIC_CONTEXT:
            BuildGenericContext(builder);
            break;
        case Engine::TermKind::TYPE:
        case Engine::TermKind::AOT_TYPE:
        case Engine::TermKind::UNION_ENUM:
        case Engine::TermKind::PRIMITIVE_ENUM:
            BuildTypeDef(builder, true);
            break;
        case Engine::TermKind::OPTION:
            BuildOption(builder);
            break;
        default:
            return std::nullopt;
    }

    if (builder.errorCode != ERR_OK) {
        return TypeInfo(builder.Build());
    }
    return std::nullopt;
}

} // namespace RTSupport
