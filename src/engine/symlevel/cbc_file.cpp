#include "cbc_file.h"

#include "io/stream_file_reader.h"

namespace Symlevel {

struct CbcFile::Impl {
    uint32_t stringOffs;
    uint32_t fieldRefsOffs;
    uint32_t signatureOffs;
    uint32_t codeOffs;
    uint32_t methodsOffs;
    uint32_t fieldOffs;
    uint32_t typesOffs;
    uint32_t typesTableOffs;
    uint32_t fieldRefsTableOffs;
    uint32_t methodRefsTableOffs;
    uint32_t termTableOffs;
    IO::FileId id;
    std::string name;
};

CbcFile::CbcFile(std::unique_ptr<CbcFile::Impl> impl) : impl(std::move(impl)) {}

CbcFile::CbcFile(CbcFile&& other) = default;
CbcFile::~CbcFile()               = default;

static uint32_t ReadU32AndAdvance(IO::StreamFileReader& reader)
{
    auto length = reader.ReadU32();
    reader.Advance(length);
    return length;
}

CbcFile CbcFile::Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name)
{
    IO::StreamFileReader reader(file, sizeof(CbcFile::MAGIC));

    // FIXME: offset from file start
    uint32_t addend = 12 * sizeof(uint32_t);

    auto stringOffs          = addend;
    auto fieldRefsOffs       = addend + reader.ReadU32();
    auto methodRefsOffs      = addend + reader.ReadU32();
    auto signatureOffs       = addend + reader.ReadU32();
    auto codeOffs            = addend + reader.ReadU32();
    auto methodsOffs         = addend + reader.ReadU32();
    auto fieldOffs           = addend + reader.ReadU32();
    auto typesOffs           = addend + reader.ReadU32();
    auto typesTableOffs      = addend + reader.ReadU32();
    auto fieldRefsTableOffs  = addend + reader.ReadU32();
    auto methodRefsTableOffs = addend + reader.ReadU32();
    auto termTableOffs       = addend + reader.ReadU32();

    CbcFile::Impl impl {
        .stringOffs          = stringOffs,
        .fieldRefsOffs       = fieldRefsOffs,
        .signatureOffs       = signatureOffs,
        .codeOffs            = codeOffs,
        .methodsOffs         = methodsOffs,
        .fieldOffs           = fieldOffs,
        .typesOffs           = typesOffs,
        .typesTableOffs      = typesTableOffs,
        .fieldRefsTableOffs  = fieldRefsTableOffs,
        .methodRefsTableOffs = methodRefsTableOffs,
        .termTableOffs       = termTableOffs,
        .id                  = fileId,
        .name                = std::string(name),
    };

    return CbcFile(std::move(std::make_unique<CbcFile::Impl>(impl)));
}

IO::FileId CbcFile::Id() const { return impl->id; }

uint32_t CbcFile::GetCodeOffs(Offset<Code> offs) const { return offs + impl->codeOffs; }

uint32_t CbcFile::GetStringOffs(Offset<String> offs) const { return offs + impl->stringOffs; }

uint32_t CbcFile::GetTypeDefOffs(Offset<TypeDefinition> offs) const { return offs + impl->typesOffs; }

uint32_t CbcFile::GetMethodDefOffs(Offset<MethodDefinition> offs) const { return offs + impl->methodsOffs; }

uint32_t CbcFile::GetFieldDefOffs(Offset<FieldDefinition> offs) const { return offs + impl->fieldOffs; }

uint32_t CbcFile::GetTermOffs(Offset<TermVal> offs) const { return offs + impl->termTableOffs; }

uint32_t CbcFile::GetTypesTableOffs() const { return impl->typesTableOffs; }

String CbcFile::GetName() const { return String(impl->name); }

} // namespace Symlevel
