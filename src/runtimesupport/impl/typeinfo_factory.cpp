#include "runtimesupport/typeinfo_factory.h"
#include "RuntimeTypes.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/terms.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/impl/cjnative.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

namespace RTSupport {

static char* Copy(std::string_view str)
{
    auto size  = str.size();
    auto data  = str.data();
    char* cStr = reinterpret_cast<char*>(std::malloc(size + 1));
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

    MRTExport::gc_tib_t gctib; // TODO: gctib builder
    uint32_t uuid;
    uint8_t align;
    int8_t typeArgsNum;
    uint16_t validInheritNum;
    uint32_t* fieldOffsets = nullptr;
    MRTExport::func_ptr_t finalizerMethod;
    MRTExport::type_info_t** typeArgs = nullptr;
    MRTExport::type_info_t** fields   = nullptr;

    MRTExport::type_info_t* superTypeInfo     = nullptr;
    MRTExport::type_info_t* componentTypeInfo = nullptr;

    MRTExport::extension_data_t** extDefs      = nullptr;
    MRTExport::func_ptr_t* flatMethods         = nullptr;
    MRTExport::extension_data_t* flatExtDefs   = nullptr;
    MRTExport::mtable_desc_t* mtableDesc       = nullptr;
    void* reflectOrDebugInfo                   = nullptr;

    Interpretation::FunctionHandle** dataMT = nullptr;

    MRTExport::type_info_t* Build()
    {
        auto result = reinterpret_cast<CbcTypeInfo*>(malloc(sizeof(MRTExport::type_info_t)));
        if (!result) {
            return nullptr;
        }

        result->type_info_name = std::exchange(this->name, nullptr);
        result->type           = type;
        result->flag           = flag;
        result->field_num      = fieldNum;
        if (instanceSize != -1) {
            result->instance_size = instanceSize;
        } else if (componentSize != -1) {
            result->component_size = componentSize;
        } else {
            ASSERTION(false, "neither of instance or component size was set");
        }
        result->gctib             = gctib;
        result->uuid              = uuid;
        result->align             = align;
        result->type_args_num     = typeArgsNum;
        result->valid_inherit_num = validInheritNum;
        result->field_offsets     = std::exchange(fieldOffsets, nullptr);
        result->finalizer_method  = finalizerMethod;
        result->type_args         = std::exchange(typeArgs, nullptr);
        result->fields            = std::exchange(fields, nullptr);
        if (superTypeInfo) {
            result->super_type_info = std::exchange(superTypeInfo, nullptr);
        } else if (componentTypeInfo) {
            result->component_type_info = std::exchange(componentTypeInfo, nullptr);
        } else {
            ASSERTION(false, "neither of super type TI or component TI was set");
        }

        result->v_extension_data_start = std::exchange(extDefs, nullptr);
        flatExtDefs                    = nullptr;
        result->mtable_desc            = std::exchange(mtableDesc, nullptr);
        result->reflect_or_debug_info  = std::exchange(reflectOrDebugInfo, nullptr);
        result->dataMT                 = dataMT;

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
    }
};

static MRTExport::func_ptr_t GetFunctionOrTrampoline(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method
)
{
    return nullptr;
}

static std::optional<TypeInfo> CreateTypeInfoDyn(
    Engine::Session& session, Engine::TypeInfoManager& manager, Symlevel::GlobalTerm term
)
{
    auto termIdent = term.GetIdentifier();
    ASSERT(termIdent.GetKind() == Symlevel::TemplateKind::TYPE);

    auto ident = termIdent.AsTypeIdent();
    auto file  = ident.GetFile();

    auto type = Symlevel::Reader::Read(session, file, ident.GetOffset());
    auto name = Symlevel::Reader::Read(session, file, type.NameOffset());

    TypeInfoBuilder builder;

    {
        // TODO: construct proper name
        auto typeInfoName = Copy(name);
        if (typeInfoName == nullptr) {
            return std::nullopt;
        }

        builder.name = typeInfoName;
    }

    // FIXME
    builder.type     = -128; // class
    builder.fieldNum = 0;
    builder.fields   = nullptr;
    builder.align    = 8;

    builder.instanceSize = 0;

    {
        auto& manager    = Symlevel::MethodTableManager::Of(session);
        auto& fuhManager = Interpretation::FunctionHandleManager::Of(session);
        auto mt          = manager.GetMethodTable(session, term);

        auto extDefCount       = mt.ClassSubTableCount() + mt.InterfaceSubTableCount();
        constexpr auto ptrSize = sizeof(void*);

        // To simplify memory management here, we will preallocate "flat" arrays
        // where corresponding structures would be filled out.
        // E.g. function tables are essentionally views in the big array.

        builder.dataMT = reinterpret_cast<Interpretation::FunctionHandle**>(std::calloc(mt.EntryCount(), ptrSize));

        builder.flatMethods = reinterpret_cast<MRTExport::func_ptr_t*>(std::calloc(mt.EntryCount(), ptrSize));

        builder.extDefs = reinterpret_cast<MRTExport::extension_data_t**>(
            std::calloc(extDefCount + 1, ptrSize) // extra slot for null-terminator
        );

        builder.flatExtDefs = reinterpret_cast<MRTExport::extension_data_t*>(
            std::calloc(extDefCount, sizeof(MRTExport::extension_data_t))
        );

        if (!builder.dataMT || !builder.flatMethods || !builder.extDefs || !builder.flatExtDefs) {
            return std::nullopt;
        }

        // fill out flat methods table and data method table
        int entryIdx = 0;
        for (auto it = mt.EntriesIter(); it.HasNext(), entryIdx++;) {
            auto entry                    = it.Next();
            builder.dataMT[entryIdx]      = fuhManager.Acquire(session, entry);
            builder.flatMethods[entryIdx] = GetFunctionOrTrampoline(session, entry);
        }

        // Fill out array of pointers to ext defs.
        for (auto i = 0; i < extDefCount; i++) {
            builder.extDefs[i] = &builder.flatExtDefs[i];
        }

        builder.extDefs[extDefCount] = nullptr;

        // fill out ext defs
        int extDefIndex = 0;
        for (auto it = mt.ClassSubTableIter(); it.HasNext();) {
            auto& smt                     = it.Next();
            auto funcTableStart           = &builder.flatMethods[smt.StartPos()];
            auto& extDef                  = builder.flatExtDefs[extDefIndex++];
            extDef.func_table             = funcTableStart;
            extDef.func_table_size        = smt.EndPos() - smt.StartPos();
            extDef.arg_num                = 0;
            extDef.is_interface_type_info = 0;

            // TODO:
            extDef.ti                  = nullptr;
            extDef.interface_type_info = nullptr;
        }

        // TODO: Intentionally copy-pasted. Should be updated accordingly
        for (auto it = mt.InterfaceSubTableIter(); it.HasNext();) {
            auto& smt                     = it.Next();
            auto funcTableStart           = &builder.flatMethods[smt.StartPos()];
            auto& extDef                  = builder.flatExtDefs[extDefIndex++];
            extDef.func_table             = funcTableStart;
            extDef.func_table_size        = smt.EndPos() - smt.StartPos();
            extDef.arg_num                = 0;
            extDef.is_interface_type_info = 1; // TODO: why 1?

            // TODO:
            extDef.ti                  = nullptr;
            extDef.interface_type_info = nullptr;
        }
    }

    auto typeInfo = builder.Build();
    if (typeInfo) {
        return TypeInfo(typeInfo);
    } else {
        return std::nullopt;
    }
}

static std::optional<TypeInfo> QueryTypeInfoAOT(Engine::Session& session, Symlevel::GlobalTerm term)
{
    auto termIdent = term.GetIdentifier();
    ASSERT(termIdent.GetKind() == Symlevel::TemplateKind::AOT_TYPE);

    auto ident    = termIdent.AsAotIdent();
    auto typeName = std::string(Symlevel::Reader::Read(session, ident.GetFile(), ident.GetOffset()));

    auto typeInfo = g_CJNativeInterfaceInstance.type_info(typeName.c_str());
    if (typeInfo == nullptr) {
        return std::nullopt;
    }
    return TypeInfo(typeInfo);
}

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Symlevel::GlobalTerm term
)
{
    auto termIdent = term.GetIdentifier();
    switch (termIdent.GetKind()) {
        case Symlevel::TemplateKind::AOT_TYPE: return QueryTypeInfoAOT(session, term);
        case Symlevel::TemplateKind::TYPE:     return CreateTypeInfoDyn(session, manager, term);
        default:                               {
            FATAL("Not supported yet %d", termIdent.GetKind());
            break;
        }
    }
    return std::nullopt;
}

} // namespace RTSupport
