#include "cbc_file.h"

#include "io/stream_file_reader.h"

namespace Symlevel {

struct CbcFile::Impl {
    uint32_t typeDefSectionOffs;
    uint32_t methodDefSectionOffs;
    uint32_t fieldDefSectionOffs;
    uint32_t codeSectionOffs;
    uint32_t termSectionOffs;
    IO::FileId id;
};

CbcFile::CbcFile(std::unique_ptr<CbcFile::Impl> impl) : impl(std::move(impl)) {}
CbcFile::CbcFile(CbcFile&& other) = default;
CbcFile::~CbcFile() = default;

static uint32_t ReadU32AndAdvance(IO::StreamFileReader& reader)
{
    auto length = reader.ReadU32();
    reader.Advance(length);
    return length;
}

CbcFile CbcFile::Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name)
{
    IO::StreamFileReader reader(file, sizeof(CbcFile::MAGIC));
    auto termsOffs = reader.Position();
    ReadU32AndAdvance(reader);

    auto typeDefOffs = reader.Position();
    ReadU32AndAdvance(reader);

    auto methodDefOffs = reader.Position();
    ReadU32AndAdvance(reader);

    auto methodRefOffs = reader.Position();
    ReadU32AndAdvance(reader);

    auto fieldDefOffs = reader.Position();
    ReadU32AndAdvance(reader);

    auto codeOffs = reader.Position();
    ReadU32AndAdvance(reader);

    CbcFile::Impl impl {
        .typeDefSectionOffs = typeDefOffs,
        .methodDefSectionOffs = methodDefOffs,
        .fieldDefSectionOffs = fieldDefOffs,
        .codeSectionOffs = codeOffs,
        .termSectionOffs = termsOffs,
        .id = fileId,
    };
    return CbcFile(std::move(std::make_unique<CbcFile::Impl>(impl)));
}

IO::FileId CbcFile::Id() const { return impl->id; }
Offset<Code> CbcFile::GetCodeSectionOffs() const { return impl->codeSectionOffs; }
Offset<TypeDefinition> CbcFile::GetTypeDefSectionOffs() const { return impl->typeDefSectionOffs; }
Offset<MethodDefinition> CbcFile::GetMethodDefSectionOffs() const { return impl->methodDefSectionOffs; }
Offset<FieldDefinition> CbcFile::GetFieldDefSectionOffs() const { return impl->fieldDefSectionOffs; }
Offset<TermVal> CbcFile::GetTermSectionOffs() const { return impl->termSectionOffs; }
} // namespace Symlevel
