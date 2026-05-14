#include "runtimesupport/typeinfo_factory.h"
#include "RuntimeTypes.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/resolving_output.h"
#include "engine/rt_logger.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/impl/cjnative.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include <cstdint>
#include <cstdlib>
#include <optional>

namespace RTSupport {

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

    char* lastDot = nullptr;
    char* cursor  = cStr;
    for (;; cursor++) {
        if (*cursor == '.')
            lastDot = cursor;
        switch (*cursor) {
            case '.': lastDot = cursor; continue;
            case '<':
            case ',':
            case '>': {
                if (lastDot)
                    *lastDot = ':';
                continue;
            }
            case '\0': {
                if (lastDot)
                    *lastDot = ':';
                return cStr;
            }
        }
    }

    return cStr;
}

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
    //
    // assume that there is no 32-bit size objects
    int32_t instanceSize  = -1;
    int32_t componentSize = -1;

    DYN_GCTibT gctib; // TODO: gctib builder
    uint32_t uuid = 0;
    uint8_t align;
    int8_t typeArgsNum           = 0;
    uint16_t validInheritNum     = 0;
    uint32_t* fieldOffsets       = nullptr;
    DYN_FuncPtrT finalizerMethod = nullptr;
    DYN_TypeInfoT** typeArgs     = nullptr;
    DYN_TypeInfoT** fields       = nullptr;

    DYN_TypeInfoT* superTypeInfo     = nullptr;
    DYN_TypeInfoT* componentTypeInfo = nullptr;

    DYN_ExtensionDataT** extDefs    = nullptr;
    DYN_FuncPtrT* flatMethods       = nullptr;
    DYN_ExtensionDataT* flatExtDefs = nullptr;
    DYN_MTableDescT* mtableDesc     = nullptr;
    void* reflectOrDebugInfo        = nullptr;

    Interpretation::FunctionHandle** dataMT = nullptr;

    CbcTypeInfo* typeInfo;

    bool built = false;

    TypeInfoBuilder(CbcTypeInfo* typeInfo) : typeInfo(typeInfo), gctib({ .raw = (1lu << 63) }) {}

    DYN_TypeInfoT* Build()
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
        } else {
            ASSERTION(false, "neither of super type TI or component TI was set");
        }

        result->vExtensionDataStart = extDefs;
        result->mTableDesc          = mtableDesc;
        result->reflectOrDebugInfo  = reflectOrDebugInfo;
        typeInfo->dataMT            = dataMT;

        built = true;
        return result;
    }

    ~TypeInfoBuilder()
    {
        if (!built) {
            // free is no-op on nulls.
            std::free(fieldOffsets);
            std::free(name);
            std::free(typeArgs);
            std::free(fields);
            std::free(superTypeInfo);
            std::free(componentTypeInfo);
            std::free(mtableDesc);
            std::free(reflectOrDebugInfo);
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
    DYN_FuncPtrT function;
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
        auto& manager  = Interpretation::FunctionHandleManager::Of(session);
        return { manager.Acquire(session, methodId), Adapters::GetDynCallTrampoline(entryIdx) };
    }
}

static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    auto ident = Engine::TypeTermId(term).GetIdentifier();

    auto type = Symlevel::Reader::Read(session, ident);
    auto name = Symlevel::Reader::Read(session, type.GetName());

    auto currentTypeInfo = Alloc<CbcTypeInfo>();
    if (!currentTypeInfo) {
        return std::nullopt;
    }

    TypeInfoBuilder builder(currentTypeInfo);

    auto queryTypeInfo = [&session, &manager, term, currentTypeInfo](Engine::Term t
                         ) -> std::optional<RTSupport::TypeInfo> {
        if (t == term) {
            return TypeInfo(&currentTypeInfo->base);
        } else if (t.GetKind() == Engine::TermKind::NIL) {
            // special case;
            // method table of core.object is encoded as nil;
            return QueryTypeInfoAOTByName("std.core:Object");
        }
        return manager.AcquireTypeInfo(session, t);
    };

    // TODO: construct proper name
    builder.name = ConstructTypeInfoName(name);
    if (builder.name == nullptr) {
        return std::nullopt;
    }

    if (type.GetFlags().Is(Symlevel::TypeFlag::AOT)) {
        return QueryTypeInfoAOTByName(builder.name);
    }

    // FIXME
    builder.type     = -128; // class
    builder.fieldNum = 0;
    builder.fields   = nullptr;
    builder.align    = 8;

    builder.instanceSize = 0;

    auto superType = Engine::TermManager::Resolve(session, type.GetSuperType());

    if (auto superTypeInfo = queryTypeInfo(superType); superTypeInfo.has_value()) {
        builder.superTypeInfo = UnpackTypeInfo(superTypeInfo.value());
    } else {
        // TODO: log
        return std::nullopt;
    }

    { // fill out ext defs
        auto& manager    = Engine::MethodTableManager::Of(session);
        auto& fuhManager = Interpretation::FunctionHandleManager::Of(session);
        auto optMT       = manager.GetMethodTable(session, term);

        if (!optMT.has_value()) {
            Log::typeinfo.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
                Stream::ResolvingOutput stream(session, out);
                stream << "Failed to build method table for " << term << Stream::endl;
            });
            return std::nullopt;
        }
        auto mt = *optMT;

        auto extDefCount = mt->ClassCount() + mt->InterfaceCount();

        // To simplify memory management here, we will preallocate "flat" arrays
        // where corresponding structures would be filled out.
        // E.g. function tables are essentionally views in the big array.

        builder.dataMT      = Alloc<Interpretation::FunctionHandle*>(mt->EntryCount());
        builder.flatMethods = Alloc<DYN_FuncPtrT>(mt->EntryCount());
        builder.extDefs     = Alloc<DYN_ExtensionDataT*>(extDefCount + 1);
        builder.flatExtDefs = Alloc<DYN_ExtensionDataT>(extDefCount);

        if (!builder.dataMT || !builder.flatMethods || !builder.extDefs || !builder.flatExtDefs) {
            return std::nullopt;
        }

        // fill out flat methods table and data method table
        int entryIdx = 0;
        for (auto entry : mt->Entries()) {
            auto tm = GetTableMember(session, entry.method, entryIdx);

            builder.dataMT[entryIdx]      = tm.handle;
            builder.flatMethods[entryIdx] = tm.function;
            entryIdx++;
        }

        // Fill out array of pointers to ext defs.
        for (int i = 0; i < extDefCount; i++) {
            builder.extDefs[i] = &builder.flatExtDefs[i];
        }

        builder.extDefs[extDefCount] = nullptr;

        auto prepareExtDef = [&builder,
                              currentTypeInfo,
                              &queryTypeInfo](DYN_ExtensionDataT& extDef, Engine::MethodSubTable const& smt) -> bool {
            auto funcTableStart        = &builder.flatMethods[smt.StartPos()];
            extDef.funcTable           = funcTableStart;
            extDef.funcTableSize       = smt.EndPos() - smt.StartPos();
            extDef.argNum              = 0;
            extDef.isInterfaceTypeInfo = 1;
            extDef.flag                = 0b00000110; // FIXME: research how to properly implement this.

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

static std::optional<TypeInfo> QueryTypeInfoAOT(Engine::Session& session, Engine::Identifier<Symlevel::String> ident)
{
    auto typeName = std::string(Symlevel::Reader::Read(session, ident.GetFileId(), ident.GetOffset()));

    auto typeInfo = g_CJNativeInterfaceInstance.typeInfo(typeName.c_str());
    if (typeInfo == nullptr) {
        return std::nullopt;
    }
    return TypeInfo(typeInfo);
}

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    Log::typeinfo.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Stream::ResolvingOutput stream(session, out);
        stream << "start building " << term << Stream::endl;
    });

    auto createTypeInfo = [&]() {
        auto termIdent = term.GetId();
        switch (termIdent.GetKind()) {
        case Engine::TermKind::TYPE:    return CreateTypeInfoDyn(session, manager, term);

        case Engine::TermKind::AOT_TYPE: return QueryTypeInfoAOT(session, Engine::AotTermId(term).GetIdentifier());
        case Engine::TermKind::AOT_REC:  return QueryTypeInfoAOT(session, Engine::AotRecTermId(term).GetIdentifier());

            case Engine::TermKind::BOOLEAN: return QueryTypeInfoAOTByName("Bool");
            case Engine::TermKind::U8:      return QueryTypeInfoAOTByName("UInt8");
            case Engine::TermKind::I8:      return QueryTypeInfoAOTByName("Int8");
            case Engine::TermKind::U16:     return QueryTypeInfoAOTByName("UInt16");
            case Engine::TermKind::I16:     return QueryTypeInfoAOTByName("Int16");
            case Engine::TermKind::U32:     return QueryTypeInfoAOTByName("UInt32");
            case Engine::TermKind::I32:     return QueryTypeInfoAOTByName("Int32");
            case Engine::TermKind::U64:     return QueryTypeInfoAOTByName("UInt64");
            case Engine::TermKind::I64:     return QueryTypeInfoAOTByName("Int64");
            case Engine::TermKind::F16:     return QueryTypeInfoAOTByName("Float16");
            case Engine::TermKind::F32:     return QueryTypeInfoAOTByName("Float32");
            case Engine::TermKind::F64:     return QueryTypeInfoAOTByName("Float64");

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

} // namespace RTSupport
