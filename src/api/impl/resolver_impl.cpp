#include "resolver_impl.h"
#include "engine/identifiers.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
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

    return Resolve(termOpt.value());
}

Type* ResolverImpl::Resolve(Symlevel::Term term)
{
    using namespace Symlevel;

    auto termIdent = term.GetIdentifier();
    switch (termIdent.GetKind()) {
        case TemplateKind::TYPE: {
            // TODO: provide type info
            return session.Allocator().New<TypeImpl>(term);
        }

        case TemplateKind::AOT_TYPE: {
            auto aotIdent       = termIdent.AsAotIdent();
            auto nameFileId     = aotIdent.GetFile();
            auto typeNameOffset = aotIdent.GetOffset();
            auto typeName       = Reader::Read(session, nameFileId, Offset<String>(typeNameOffset));

            auto typeInfo = RTSupport::RuntimeInterface<RTSupport::Impl>::GetTypeInfo(std::string(typeName).c_str());

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

DirectMethod* ResolverImpl::ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto methodRefOpt = cbcFile.GetRegionData().queryMethod(session, index);
    if (!methodRefOpt.has_value()) {
        FATAL("cannot resolve method ref");
    }

    auto methodRef = methodRefOpt.value();
    ASSERTION(methodRef.AccessKind() == Symlevel::MethodAccessKind::DIRECT, "resolving direct method is not static");

    auto* refType = Resolve(methodRef.RefType());
    auto name     = methodRef.Name();

    auto refTypeId = methodRef.RefType().GetIdentifier();
    switch (refTypeId.GetKind()) {
        case Symlevel::TemplateKind::TYPE: {
            auto ident = refTypeId.AsTypeIdent();
            Engine::Identifier<Symlevel::TypeDefinition> typeId(ident.GetOffset(), ident.GetFile());
            auto refTypeDef = Symlevel::TypeDefinition::Resolve(session, typeId);

            auto methodDefs = refTypeDef.GetMethodIndex().FindMethods(session, name);
            ASSERTION(methodDefs.size() == 1, "not implemented yet");
            auto methodDef = methodDefs[0];

            auto& fuhManager = Interpretation::FunctionHandleManager::Of(session);
            auto* fuh        = fuhManager.Acquire(session, methodDef.GetIdentifier());

            return session.Allocator().New<DirectMethodCbc>(refType, fuh, name);
        }

        case Symlevel::TemplateKind::AOT_TYPE: {
            auto data = cbcFile.GetDirectCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                FATAL("aot method ref has no aot data");
            }

            auto linkageName   = data->GetLinkageName();
            auto targetAddress = cbcFile.GetDependencies().FindTarget(linkageName);

            return session.Allocator().New<DirectMethodAot>(refType, targetAddress, name);
        }

        default: {
            FATAL("should not reach here");
            return nullptr;
        }
    }
}

VirtualMethod* ResolverImpl::ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto methodRefOpt = cbcFile.GetRegionData().queryMethod(session, index);
    if (!methodRefOpt.has_value()) {
        FATAL("cannot resolve method ref");
    }

    auto methodRef = methodRefOpt.value();
    ASSERTION(methodRef.AccessKind() == Symlevel::MethodAccessKind::VIRTUAL, "resolving virtual method is not virtual");

    auto* refType = Resolve(methodRef.RefType());
    auto name     = methodRef.Name();

    auto refTypeId = methodRef.RefType().GetIdentifier();
    switch (refTypeId.GetKind()) {
        case Symlevel::TemplateKind::TYPE: {
            auto& mtManager  = Symlevel::MethodTableManager::Of(session);
            auto methodTable = mtManager.GetMethodTable(session, methodRef.RefType());

            std::vector<Symlevel::MethodTableEntry> entries;
            methodTable.Find(session, name, entries);
            if (entries.size() != 1) {
                // TODO: - implement signature comparison
                //       - proper error handling
                ASSERTION(false, "failed to resolve virtual method");
            }
            auto methodInfo = entries.at(0);

            auto vnum      = methodInfo.methodNum;
            auto extDefNum = methodInfo.subTableNum;

            return session.Allocator().New<VirtualMethodImpl>(refType, vnum, extDefNum, name);
        }

        case Symlevel::TemplateKind::AOT_TYPE: {
            auto data = cbcFile.GetVirtualCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                FATAL("aot method ref has no aot data");
            }

            auto vnum      = data->GetVNum();
            auto extDefNum = data->GetExtDefNum();

            return session.Allocator().New<VirtualMethodImpl>(refType, vnum, extDefNum, name);
        }

        default: {
            FATAL("should not reach here");
            return nullptr;
        }
    }
}

InterfaceMethod* ResolverImpl::ResolveInterfaceMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto methodRefOpt = cbcFile.GetRegionData().queryMethod(session, index);
    if (!methodRefOpt.has_value()) {
        FATAL("cannot resolve method ref");
    }

    auto methodRef = methodRefOpt.value();
    ASSERTION(
        methodRef.AccessKind() == Symlevel::MethodAccessKind::INTERFACE, "resolving interface method is not interface"
    );

    auto* refType = Resolve(methodRef.RefType());
    auto name     = methodRef.Name();

    auto refTypeId = methodRef.RefType().GetIdentifier();

    switch (refTypeId.GetKind()) {
        case Symlevel::TemplateKind::TYPE: {
            FATAL("not implemented yet");
            return nullptr;
        }

        case Symlevel::TemplateKind::AOT_TYPE: {
            auto data = cbcFile.GetInterfaceCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                FATAL("aot method ref has no aot data");
            }

            auto inum = data->GetINum();

            return session.Allocator().New<InterfaceMethodImpl>(refType, inum, name);
        }

        default: {
            ASSERTION(false, "should not reach here");
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

ResolverImpl::~ResolverImpl() = default;

} // namespace Impl
} // namespace API
