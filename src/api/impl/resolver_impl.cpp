#include "resolver_impl.h"
#include "engine/identifiers.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
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
        case TemplateKind::AOT_TYPE: {
            auto nameFileId     = term.GetIdentifier().AsAotIdent().GetFile();
            auto typeNameOffset = term.GetIdentifier().AsAotIdent().GetOffset();
            auto typeName       = Reader::Read(session, nameFileId, Offset<String>(typeNameOffset));
            auto typeInfo       = RTSupport::Runtime::GetTypeInfo(std::string(typeName).c_str());

            ASSERTION(typeInfo.Raw() != nullptr, "Couldn't resolve AOT type");

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

InstanceField* ResolverImpl::Resolve(Symlevel::Index<InstanceField> index)
{
    FATAL("not implemented yet");
    return nullptr;
}

StaticField* ResolverImpl::Resolve(Symlevel::Index<StaticField> index)
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Type*> ResolverImpl::TypeOf(Term* term)
{
    FATAL("not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

} // namespace Impl
} // namespace API
