#include "resolver_impl.h"
#include "engine/identifiers.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
#include "field_impl.h"
#include "method_impl.h"
#include "type_impl.h"
#include "utils/assertion.h"
#include <vector>

namespace API {
namespace Impl {

//////////////////////////////////
// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Symlevel::Term> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto termOpt = cbcFile.GetRegionData().queryTerm(session, index);
    if (!termOpt) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve term");
    }

    return Resolve(termOpt.value());
}

Type* ResolverImpl::Resolve(Symlevel::Term term)
{
    using namespace Symlevel;

    auto termKind = term.GetIdentifier().GetKind();
    const char* typeName;
    switch (termKind) {
        case TemplateKind::AOT_TYPE: {
            auto nameFileId     = term.GetIdentifier().AsAotIdent().GetFile();
            auto typeNameOffset = term.GetIdentifier().AsAotIdent().GetOffset();
            auto _typeName      = Reader::Read(session, nameFileId, Offset<String>(typeNameOffset));

            typeName = std::string(_typeName).c_str();
            break;
        }
        case TemplateKind::U8:
        case TemplateKind::I8:
        case TemplateKind::U16:
        case TemplateKind::I16:
        case TemplateKind::U32:
        case TemplateKind::I32:
        case TemplateKind::U64:
        case TemplateKind::I64:
        case TemplateKind::F16:
        case TemplateKind::F32:
        case TemplateKind::F64: { // TODO support other built-in types
            auto _typeName = term.GetIdentifier().GetKindName();
            if (!_typeName.has_value()) {
                FATAL("Cannot get type info of template kind: %d", termKind);
            }

            typeName = _typeName.value();
            break;
        }
        default: {
            FATAL("Not supported yet");
            return nullptr;
            ;
        }
    }

    TypeInfo typeInfo = RTSupport::RuntimeInterface<RTSupport::Impl>::GetTypeInfo(typeName);

    ASSERTION(typeInfo != nullptr, "Couldn't resolve AOT type");
    return session.Allocator().New<TypeImpl>(term, typeInfo);
}

VirtualMethod* ResolverImpl::ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto methodRef = cbcFile.GetRegionData().queryMethod(session, index);
    if (!methodRef.has_value()) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve method ref");
    }

    auto ref = methodRef.value();
    ASSERTION(ref.AccessKind() == Symlevel::MethodAccessKind::VIRTUAL, "resolving virtual method is not virtual");

    auto refType   = ref.RefType();
    auto refTypeId = refType.GetIdentifier();
    switch (refTypeId.GetKind()) {
        case Symlevel::TemplateKind::TYPE: {
            auto& manager = Symlevel::MethodTableManager::Of(session);
            auto mt       = manager.GetMethodTable(session, refType);

            std::vector<Symlevel::MethodTableEntry> entries;
            mt.Find(session, ref.Name(), entries);

            if (entries.size() != 1) {
                // TODO: - implement signature comparison
                //       - proper error handling
                ASSERTION(false, "failed to resolve virtual method");
            }

            auto methodInfo = entries.at(0);
            return session.Allocator().New<VirtualMethodImpl>(
                session, ref, methodInfo.methodNum, methodInfo.subTableNum
            );
        }

        case Symlevel::TemplateKind::AOT_TYPE: {
            auto data = session.CbcFileOf(method.GetFileId()).GetVirtualCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                // TODO: handle this case
                throw std::runtime_error("aot method ref has no aot data");
            }

            auto methodInfo = data.value();
            return session.Allocator().New<VirtualMethodImpl>(
                session, ref, methodInfo.GetVNum(), methodInfo.GetExtDefNum()
            );
        }

        default: {
            ASSERTION(false, "should not reach here");
            return nullptr;
        }
    }
}

DirectMethod* ResolverImpl::ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto methodRef = cbcFile.GetRegionData().queryMethod(session, index);
    if (!methodRef.has_value()) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve method ref");
    }

    auto ref = methodRef.value();
    ASSERTION(ref.AccessKind() == Symlevel::MethodAccessKind::DIRECT, "resolving direct method is not static");

    auto refTypeId = ref.RefType().GetIdentifier();
    switch (refTypeId.GetKind()) {
        case Symlevel::TemplateKind::TYPE: {
            auto ident = refTypeId.AsTypeIdent();
            Engine::Identifier<Symlevel::TypeDefinition> typeId(ident.GetOffset(), ident.GetFile());
            auto refTypeDef = Symlevel::TypeDefinition::Resolve(session, typeId);

            auto candidates = refTypeDef.GetMethodIndex().FindMethods(session, ref.Name());

            ASSERTION(candidates.size() == 1, "not implemented yet");
            auto target = candidates[0];

            return session.Allocator().New<DirectMethodCbc>(session, target);
        }

        case Symlevel::TemplateKind::AOT_TYPE: {
            auto data = session.CbcFileOf(method.GetFileId()).GetDirectCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                // TODO: handle this case
                throw std::runtime_error("aot method ref has no aot data");
            }

            return session.Allocator().New<DirectMethodAot>(session, ref, data.value());
        }

        default: {
            FATAL("should not reach here");
            return nullptr;
        }
    }
}

template <typename T> T* ResolverImpl::ResolveField(Symlevel::Index<Symlevel::FieldReference> index)
{
    static_assert(std::is_same_v<T, InstanceFieldImpl> || std::is_same_v<T, StaticFieldImpl>);
    using namespace Symlevel;

    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto fieldRefOpt = cbcFile.GetRegionData().queryField(session, index);
    if (!fieldRefOpt.has_value()) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve method ref");
    }

    auto fieldRef = fieldRefOpt.value();

    Type* fieldType  = Resolve(fieldRef.FieldType());
    FieldFlags flags = fieldRef.IsRecord() ? FieldFlags(FieldFlag::Shift::RECORD) : FieldFlags();

    switch (fieldRef.RefType().GetIdentifier().GetKind()) {
        case TemplateKind::AOT_TYPE: {
            ASSERTION(
                fieldRef.FieldType().GetIdentifier().GetKind() != TemplateKind::TYPE,
                "aot types cannot have fields of cbc type"
            );

            if constexpr (std::is_same_v<T, InstanceFieldImpl>) {
                Type* refType = Resolve(fieldRef.RefType());

                InstanceFieldAotData data = cbcFile.GetInstanceFieldAotTable().GetData(session, index).value();

                return session.Allocator().New<InstanceFieldImpl>(
                    fieldRef.Name(), data.GetOrdinal(), flags, fieldType, refType
                );
            } else {
                static_assert(std::is_same_v<T, StaticFieldImpl>);
                StaticFieldAotData data = cbcFile.GetStaticFieldAotTable().GetData(session, index).value();

                String linkageName = data.GetLinkageName();
                auto location      = cbcFile.GetDependencies().FindTarget(linkageName);

                return session.Allocator().New<StaticFieldImpl>(
                    reinterpret_cast<uintptr_t>(location), fieldRef.Name(), flags, fieldType
                );
            }
        }
        default: {
            FATAL("Not supported yet");
            return nullptr;
        }
    }
}

InstanceField* ResolverImpl::ResolveInstanceField(Symlevel::Index<Symlevel::FieldReference> index)
{
    return ResolveField<InstanceFieldImpl>(index);
}

StaticField* ResolverImpl::ResolveStaticField(Symlevel::Index<Symlevel::FieldReference> index)
{
    return ResolveField<StaticFieldImpl>(index);
}

std::optional<Type*> ResolverImpl::TypeOf(Symlevel::Term* term)
{
    FATAL("not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

} // namespace Impl
} // namespace API
