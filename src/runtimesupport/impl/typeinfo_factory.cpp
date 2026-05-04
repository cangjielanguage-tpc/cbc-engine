#include "runtimesupport/typeinfo_factory.h"
#include "RuntimeTypes.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/impl/cjnative.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

namespace RTSupport {

template <typename T> static T* Alloc(size_t cnt = 1) { return reinterpret_cast<T*>(std::malloc(sizeof(T) * cnt)); }

static char* Copy(std::string_view str)
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
    uint8_t flag = 0;
    uint16_t fieldNum;
    //
    // assume that there is no 32-bit size objects
    int32_t instanceSize  = -1;
    int32_t componentSize = -1;

    DYN_GCTibT gctib; // TODO: gctib builder
    uint32_t uuid;
    uint8_t align;
    int8_t typeArgsNum;
    uint16_t validInheritNum;
    uint32_t* fieldOffsets = nullptr;
    DYN_FuncPtrT finalizerMethod;
    DYN_TypeInfoT** typeArgs = nullptr;
    DYN_TypeInfoT** fields   = nullptr;

    DYN_TypeInfoT* superTypeInfo     = nullptr;
    DYN_TypeInfoT* componentTypeInfo = nullptr;

    DYN_ExtensionDataT** extDefs               = nullptr;
    DYN_FuncPtrT* flatMethods                  = nullptr;
    DYN_ExtensionDataT* flatExtDefs            = nullptr;
    DYN_MTableDescT* mtableDesc                = nullptr;
    void* reflectOrDebugInfo                   = nullptr;

    Interpretation::FunctionHandle** dataMT = nullptr;

    CbcTypeInfo* typeInfo;

    TypeInfoBuilder(CbcTypeInfo* typeInfo) : typeInfo(typeInfo), gctib({ .raw = (1lu << 63) }) {}

    DYN_TypeInfoT* Build()
    {
        auto typeInfo = std::exchange(this->typeInfo, nullptr);
        auto result   = &typeInfo->base;

        result->typeInfoName   = std::exchange(this->name, nullptr);
        result->type           = type;
        result->flag           = flag;
        result->fieldNum       = fieldNum;
        if (instanceSize != -1) {
            result->instanceSize = instanceSize;
        } else if (componentSize != -1) {
            result->componentSize = componentSize;
        } else {
            ASSERTION(false, "neither of instance or component size was set");
        }
        result->gctib             = std::exchange(gctib, {}); // FIXME
        result->uuid              = uuid;
        result->align             = align;
        result->typeArgsNum       = typeArgsNum;
        result->validInheritNum   = validInheritNum;
        result->fieldOffsets      = std::exchange(fieldOffsets, nullptr);
        result->finalizerMethod   = finalizerMethod;
        result->typeArgs          = std::exchange(typeArgs, nullptr);
        result->fields            = std::exchange(fields, nullptr);
        if (superTypeInfo) {
            result->superTypeInfo = std::exchange(superTypeInfo, nullptr);
        } else if (componentTypeInfo) {
            result->componentTypeInfo = std::exchange(componentTypeInfo, nullptr);
        } else {
            ASSERTION(false, "neither of super type TI or component TI was set");
        }

        result->vExtensionDataStart    = std::exchange(extDefs, nullptr);
        flatExtDefs                    = nullptr;
        result->mTableDesc             = std::exchange(mtableDesc, nullptr);
        result->reflectOrDebugInfo     = std::exchange(reflectOrDebugInfo, nullptr);
        typeInfo->dataMT               = dataMT;

        return result;
    }

    ~TypeInfoBuilder()
    {
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
};

static DYN_FuncPtrT GetFunctionOrTrampoline(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method, int entryIdx
)
{
    // FIXME: expects only CBC methods for now.
    return Adapters::GetDynCallTrampoline(entryIdx);
}

static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    auto termIdent = term.GetIdentifier();
    ASSERT(termIdent.GetKind() == Engine::TemplateKind::TYPE);

    auto ident = termIdent.AsTypeIdent();
    auto file  = ident.GetFile();

    auto type = Symlevel::Reader::Read(session, file, ident.GetOffset());
    auto name = Symlevel::Reader::Read(session, file, type.NameOffset());

    auto currentTypeInfo = Alloc<CbcTypeInfo>();
    if (!currentTypeInfo) {
        return std::nullopt;
    }

    TypeInfoBuilder builder(currentTypeInfo);

    auto queryTypeInfo =
        [&session, &manager, term, currentTypeInfo](Engine::Term t) -> std::optional<RTSupport::TypeInfo> {
        if (t == term) {
            return TypeInfo(&currentTypeInfo->base);
        }
        return manager.AcquireTypeInfo(session, t);
    };

    // TODO: construct proper name
    auto typeInfoName = Copy(name);
    if (typeInfoName == nullptr) {
        return std::nullopt;
    }

    builder.name = typeInfoName;

    // FIXME
    builder.type     = -128; // class
    builder.fieldNum = 0;
    builder.fields   = nullptr;
    builder.align    = 8;

    builder.instanceSize = 0;

    { // fill out ext defs
        auto& manager    = Symlevel::MethodTableManager::Of(session);
        auto& fuhManager = Interpretation::FunctionHandleManager::Of(session);
        auto mt          = manager.GetMethodTable(session, term);

        auto extDefCount       = mt.ClassSubTableCount() + mt.InterfaceSubTableCount();
        constexpr auto ptrSize = sizeof(void*);

        // To simplify memory management here, we will preallocate "flat" arrays
        // where corresponding structures would be filled out.
        // E.g. function tables are essentionally views in the big array.

        builder.dataMT      = Alloc<Interpretation::FunctionHandle*>(mt.EntryCount());
        builder.flatMethods = Alloc<DYN_FuncPtrT>(mt.EntryCount());
        builder.extDefs     = Alloc<DYN_ExtensionDataT*>(extDefCount);
        builder.flatExtDefs = Alloc<DYN_ExtensionDataT>(extDefCount);

        if (!builder.dataMT || !builder.flatMethods || !builder.extDefs || !builder.flatExtDefs) {
            return std::nullopt;
        }

        // fill out flat methods table and data method table
        int entryIdx = 0;
        for (auto it = mt.EntriesIter(); it.HasNext(), entryIdx++;) {
            auto entry                    = it.Next();
            builder.dataMT[entryIdx]      = fuhManager.Acquire(session, entry);
            builder.flatMethods[entryIdx] = GetFunctionOrTrampoline(session, entry, entryIdx);
        }

        // Fill out array of pointers to ext defs.
        for (auto i = 0; i < extDefCount; i++) {
            builder.extDefs[i] = &builder.flatExtDefs[i];
        }

        builder.extDefs[extDefCount] = nullptr;

        auto prepareExtDef = [&builder,
                              currentTypeInfo,
                              &queryTypeInfo](DYN_ExtensionDataT& extDef, Symlevel::MethodSubTable const& smt) -> bool {
            auto funcTableStart           = &builder.flatMethods[smt.StartPos()];
            extDef.funcTable              = funcTableStart;
            extDef.funcTableSize          = smt.EndPos() - smt.StartPos();
            extDef.argNum                 = 0;
            extDef.isInterfaceTypeInfo    = 1;
            extDef.flag                   = 0b00000001; // FIXME: research how to properly implement this.

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
        for (auto it = mt.ClassSubTableIter(); it.HasNext();) {
            if (!prepareExtDef(builder.flatExtDefs[extDefIndex++], it.Next())) {
                return std::nullopt;
            }
        }

        for (auto it = mt.InterfaceSubTableIter(); it.HasNext();) {
            if (!prepareExtDef(builder.flatExtDefs[extDefIndex++], it.Next())) {
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

static std::optional<TypeInfo> QueryTypeInfoAOT(Engine::Session& session, Engine::GlobalTerm term)
{
    auto termIdent = term.GetIdentifier();
    ASSERT(termIdent.GetKind() == Engine::TemplateKind::AOT_TYPE);

    auto ident    = termIdent.AsAotIdent();
    auto typeName = std::string(Symlevel::Reader::Read(session, ident.GetFile(), ident.GetOffset()));

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
    auto termIdent = term.GetIdentifier();
    switch (termIdent.GetKind()) {
        case Engine::TemplateKind::AOT_TYPE: return QueryTypeInfoAOT(session, term);
        case Engine::TemplateKind::TYPE:     return CreateTypeInfoDyn(session, manager, term);

        case Engine::TemplateKind::BOOLEAN: return QueryTypeInfoAOTByName("Bool");
        case Engine::TemplateKind::U8:      return QueryTypeInfoAOTByName("UInt8");
        case Engine::TemplateKind::I8:      return QueryTypeInfoAOTByName("Int8");
        case Engine::TemplateKind::U16:     return QueryTypeInfoAOTByName("UInt16");
        case Engine::TemplateKind::I16:     return QueryTypeInfoAOTByName("Int16");
        case Engine::TemplateKind::U32:     return QueryTypeInfoAOTByName("UInt32");
        case Engine::TemplateKind::I32:     return QueryTypeInfoAOTByName("Int32");
        case Engine::TemplateKind::U64:     return QueryTypeInfoAOTByName("UInt64");
        case Engine::TemplateKind::I64:     return QueryTypeInfoAOTByName("Int64");
        case Engine::TemplateKind::F16:     return QueryTypeInfoAOTByName("Float16");
        case Engine::TemplateKind::F32:     return QueryTypeInfoAOTByName("Float32");
        case Engine::TemplateKind::F64:     return QueryTypeInfoAOTByName("Float64");

        default: {
            FATAL("Not supported yet %d", termIdent.GetKind());
            break;
        }
    }
    return std::nullopt;
}

} // namespace RTSupport
