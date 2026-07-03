#include "exception_handling.h"

#include <algorithm>

#include "engine/symlevel/code.h"
#include "engine/symlevel/definitions.h"
#include "engine/terms.h"
#include "interpreter/function_handle.h"

namespace EHSupport {

uint8_t engine_get_exception_handler(Interpretation::DynamicFunctionHandle* handle, Decoder::ByteReader& reader)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto bytecode     = handle->bytecode.load();
    auto offsetsIndex = bytecode->offsetsIndex;

    auto methodDef = Symlevel::MethodDefinition::Resolve(session, handle->methodDef);
    if (!methodDef.MethodCode().has_value()) {
        return false;
    }

    uint8_t* bcStart = bytecode->code.bytecode;
    uint8_t* bcEnd   = bcStart + bytecode->code.bytecodeSize;
    ASSERT(bcStart <= reader.Cursor() && reader.Cursor() <= bcEnd);

    auto bcPos      = reader.Cursor() - bcStart;
    auto exPos      = bcPos - 1;
    auto methodCode = Symlevel::Code::Resolve(session, methodDef.MethodCode().value());
    auto regions    = methodCode.GetExceptionRegions(session);
    auto it         = std::find_if(regions.begin(), regions.end(), [&](const Symlevel::ExceptionRegion& region) {
        auto regStart = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, region.start);
        auto regEnd   = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, region.end);

        if (!regStart.has_value() || !regEnd.has_value()) {
            FATAL("Couldn't translate exception region offsets to rt bytecode offsets");
            return false;
        }

        return regStart.value() <= exPos && exPos <= regEnd.value();
    });

    if (it == regions.end()) {
        return false; // no suitable handler found
    }

    auto target = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, it->target);
    if (!target.has_value()) {
        FATAL("Couldn't translate exception region target offset to rt bytecode offset");
        return false;
    }

    auto delta = static_cast<int64_t>(target.value()) - static_cast<int64_t>(bcPos);
    reader.Advance(delta);
    return true;
}

void FrameInfoProvider(DYN_InstructionPointer ip, DYN_FramePointer fp, INT_InterpretedFrameInfo* info)
{
    auto fuh      = *reinterpret_cast<Interpretation::DynamicFunctionHandle**>((uint8_t*)fp - FUH_SLOT_OFFSET);
    auto reader   = reinterpret_cast<Decoder::ByteReader*>((uint8_t*)fp - READER_SLOT_OFFSET);
    auto bytecode = fuh->bytecode.load();

    uint8_t* bcStart = bytecode->code.bytecode;
    uint8_t* bcEnd   = bcStart + bytecode->code.bytecodeSize;
    ASSERT(bcStart <= reader->Cursor() && reader->Cursor() <= bcEnd);

    info->bcPos = reinterpret_cast<INT_BytecodePos>(reader->Cursor() - bcStart);
    info->fuh   = fuh;
}

static char* AllocateString(std::string_view sv)
{
    char* buffer = new char[sv.size() + 1];
    std::memcpy(buffer, sv.data(), sv.size());
    buffer[sv.size()] = 0;
    return buffer;
}

static void FreeStrings(char* methodName, char* className, char* fileName)
{
    ASSERT(methodName != nullptr && className != nullptr && fileName != nullptr);
    delete[] methodName;
    delete[] className;
    delete[] fileName;
}

// TODO support getting src line by bytecode posiiton
void FrameDescProvider(INT_FunctionHandle fuh, INT_BytecodePos pos, INT_InterpretedFrameDesc* frameDesc)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto dynFuh = static_cast<const Interpretation::DynamicFunctionHandle*>(fuh);

    auto methodDef = Symlevel::MethodDefinition::Resolve(session, dynFuh->methodDef);
    if (!methodDef.MethodCode().has_value()) {
        FATAL("Couldn't get method definition for stacktrace from FuH: %p", dynFuh);
        return;
    }

    // Line number
    {
        frameDesc->lineNumber = 0; // TODO: implement
    }

    // Method name
    {
        std::string methodNameWithArgs;
        auto methodName = Symlevel::String::Parse(session, methodDef.Name());
        methodNameWithArgs.append(methodName).append("(");

        auto sig      = Engine::TermManager::Resolve(session, methodDef.Signature());
        int retSigIdx = sig.GetLength() - 1;
        for (int i = 0; i < retSigIdx; i++) {
            auto argTerm = sig.Subterm(i);
            methodNameWithArgs.append(argTerm.GetName(session));
            if (i != retSigIdx - 1) {
                methodNameWithArgs.append(", ");
            }
        }
        methodNameWithArgs.append(")");

        frameDesc->methodName = AllocateString(methodNameWithArgs);
    }

    // Type name
    {
        auto typeNameView = Symlevel::String::Parse(session, methodDef.TypeName());
        size_t size       = typeNameView.size();

        size_t delimPos = typeNameView.find(':');
        if (delimPos == std::string_view::npos) {
            // method is in a package
            frameDesc->className = AllocateString(typeNameView);
        } else {
            // method in a class
            std::string typeName;
            typeName.reserve(typeNameView.size() + 1);

            typeName.append(typeNameView.substr(0, delimPos));
            typeName.append("::");
            typeName.append(typeNameView.substr(delimPos + 1));

            frameDesc->className = AllocateString(typeName);
        }
    }

    // File name
    if (methodDef.SourceFile().has_value()) {
        auto fileName       = Symlevel::String::Parse(session, methodDef.SourceFullName().value());
        frameDesc->fileName = AllocateString(fileName);
    } else {
        frameDesc->fileName = AllocateString("unknown"); // should it be possible?
    }

    frameDesc->freeResources = FreeStrings;
}

} // namespace EHSupport
