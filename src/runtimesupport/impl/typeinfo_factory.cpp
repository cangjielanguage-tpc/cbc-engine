#include "runtimesupport/typeinfo_factory.h"
#include "RuntimeTypes.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/terms.h"
#include "runtimesupport/impl/cjnative.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include <cstdint>
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

    MRTExport::extension_data_t** extDataStart = nullptr;
    MRTExport::mtable_desc_t* mtableDesc       = nullptr;
    void* reflectOrDebugInfo;

    MRTExport::type_info_t* Build()
    {
        auto result = reinterpret_cast<MRTExport::type_info_t*>(malloc(sizeof(MRTExport::type_info_t)));
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
        result->v_extension_data_start = std::exchange(extDataStart, nullptr);
        result->mtable_desc            = std::exchange(mtableDesc, nullptr);
        result->reflect_or_debug_info  = std::exchange(reflectOrDebugInfo, nullptr);

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
        std::free(extDataStart);
        std::free(mtableDesc);
        std::free(reflectOrDebugInfo);
    }
};

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
        // TODO: build method table
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
