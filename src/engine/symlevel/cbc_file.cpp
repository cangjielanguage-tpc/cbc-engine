#include "cbc_file.h"
#include "reader.h"

#include "io/stream_file_reader.h"


inline constexpr uint32_t MAGIC = 0xCBCDAFF0;

int curFileId = 0;


namespace Symlevel {

template <typename T> std::vector<T> ReadEntries(
        IO::StreamFileReader& reader,
        IO::FileId fileId)
{
    std::vector<T> vector;
    auto count = reader.ReadU32();
    vector.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        vector.push_back(T::Parse(fileId, reader));
    }
    return vector;
}

CbcFile* CbcFile::Create(IO::RandomAccessFile* file)
{
    IO::StreamFileReader reader(file, 0);

    IO::FileId fileId(curFileId);
    curFileId += 1;

    uint32_t magic = reader.ReadU32();
    if (magic != MAGIC) {
        std::filesystem::path path = file->Path();
        std::stringstream msg;
        msg << "Invalid CBC file @ " << path.c_str() << ": wrong magic";
        throw std::runtime_error(msg.str());
    }

    auto terms = ReadEntries<TermValue>(reader, fileId);
    auto typeDefs = ReadEntries<TypeDefinition>(reader, fileId);
    auto methodDefs = ReadEntries<MethodDefinition>(reader, fileId);
    auto methodRefs = ReadEntries<MethodReference>(reader, fileId);
    auto fieldDefs = ReadEntries<FieldDefinition>(reader, fileId);
    auto codes = ReadEntries<Code>(reader, fileId);

    return new CbcFile(
        fileId,
        std::move(terms),
        std::move(typeDefs),
        std::move(methodDefs),
        std::move(methodRefs),
        std::move(fieldDefs),
        std::move(codes)
    );
}

} // namespace Symlevel
