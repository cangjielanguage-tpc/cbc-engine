#include "resolver_impl.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
#include "field_impl.h"
#include "method_impl.h"
#include "type_impl.h"

namespace API {
namespace Impl {

//////////////////////////////////
// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Symlevel::Terms::Term> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto termOpt = cbcFile.GetRegionData().queryTerm(session, index);
    if (!termOpt) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve term");
    }

    return Resolve(termOpt.value());
}

Type* ResolverImpl::Resolve(Symlevel::Terms::Term term)
{
    using namespace Symlevel;

    auto termKind = term.GetIdentifier().GetKind();
    switch (termKind) {
        case Terms::TemplateKind::AOT_TYPE: {
            auto nameFileId     = term.GetIdentifier().GetFileId();
            auto typeNameOffset = term.GetIdentifier().GetOffset();
            auto typeName       = Reader::Read(session, nameFileId, Offset<String>(typeNameOffset));
            auto typeInfo = RTSupport::RuntimeInterface<RTSupport::Impl>::GetTypeInfo(std::string(typeName).c_str());

            ASSERTION(typeInfo != nullptr, "Couldn't resolve AOT type");

            return session.Allocator().New<TypeImpl>(term, typeInfo);
        }
        case Terms::TemplateKind::I64: { // TODO support other built-in types
            auto typeInfo = RTSupport::RuntimeInterface<RTSupport::Impl>::GetTypeInfo(std::string("Int64").c_str());
            ASSERTION(typeInfo != nullptr, "Couldn't resolve AOT type");

            return session.Allocator().New<TypeImpl>(term, typeInfo);
        }
        default: {
            FATAL("Not supported yet");
            break;
        }
    }

    return nullptr;
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

    auto refTypeId = ref.RefType().GetIdentifier();
    switch (refTypeId.GetKind()) {
        case Symlevel::Terms::TemplateKind::TYPE: {
            ASSERTION(false, "cbc virtual calls are not supported yet");
            return nullptr;
        }

        case Symlevel::Terms::TemplateKind::AOT_TYPE: {
            auto data = session.CbcFileOf(method.GetFileId()).GetVirtualCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                // TODO: handle this case
                throw std::runtime_error("aot method ref has no aot data");
            }

            return session.Allocator().New<VirtualMethodAot>(session, ref, data.value());
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
        case Symlevel::Terms::TemplateKind::TYPE: {
            Engine::Identifier<Symlevel::TypeDefinition> typeId(refTypeId.GetNum());
            auto refTypeDef = Symlevel::TypeDefinition::Resolve(session, typeId);

            auto candidates = refTypeDef.GetMethodIndex().FindMethods(session, ref.Name());

            ASSERTION(candidates.size() == 1, "not implemented yet");
            auto target = candidates[0];

            return session.Allocator().New<DirectMethodCbc>(session, target);
        }

        case Symlevel::Terms::TemplateKind::AOT_TYPE: {
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

template <typename T>
T* ResolverImpl::ResolveField(Symlevel::Index<Symlevel::FieldReference> index)
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
    Type* refType    = Resolve(fieldRef.RefType());
    FieldFlags flags = fieldRef.IsRecord() ? FieldFlags(FieldFlag::Shift::RECORD) : FieldFlags();

    switch (fieldRef.RefType().GetIdentifier().GetKind()) {
        case Terms::TemplateKind::AOT_TYPE: {
            ASSERTION(
                fieldRef.FieldType().GetIdentifier().GetKind() != Terms::TemplateKind::TYPE,
                "aot types cannot have fields of cbc type"
            );

            if constexpr (std::is_same_v<T, InstanceFieldImpl>) {
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
                    reinterpret_cast<uintptr_t>(location), fieldRef.Name(), flags, fieldType, refType
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

std::optional<Type*> ResolverImpl::TypeOf(Symlevel::Terms::Term* term)
{
    FATAL("not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

} // namespace Impl
} // namespace API
