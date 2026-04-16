#include "resolver_impl.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
#include "method_impl.h"
#include "type_impl.h"

namespace API {
namespace Impl {

//////////////////////////////////
// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Symlevel::Terms::Term> index)
{
    using namespace Symlevel;

    auto fileId   = method.GetFileId();
    auto& cbcFile = session.CbcFileOf(fileId);

    auto termOpt = cbcFile.GetRegionData().queryTerm(session, index);
    if (!termOpt) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve term");
    }

    auto term = termOpt.value();

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
        default: {
            ASSERTION(false, "Not supported yet");
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
            ASSERTION(false, "should not reach here");
            return nullptr;
        }
    }
}

InstanceField* ResolverImpl::Resolve(Symlevel::Index<InstanceField> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

StaticField* ResolverImpl::Resolve(Symlevel::Index<StaticField> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> ResolverImpl::TypeOf(Term* term)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

} // namespace Impl
} // namespace API
