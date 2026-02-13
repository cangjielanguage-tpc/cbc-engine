#include "cbc_file.h"
#include "reader.h"

#include "io/stream_file_reader.h"

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

CbcFile CbcFile::Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name)
{
    IO::StreamFileReader reader(file, sizeof(CbcFile::MAGIC));

    auto terms = ReadEntries<TermValue>(reader, fileId);
    auto typeDefs = ReadEntries<TypeDefinition>(reader, fileId);
    auto methodDefs = ReadEntries<MethodDefinition>(reader, fileId);
    auto methodRefs = ReadEntries<MethodReference>(reader, fileId);
    auto fieldDefs = ReadEntries<FieldDefinition>(reader, fileId);
    auto codes = ReadEntries<Code>(reader, fileId);

    return CbcFile(
        fileId,
        std::move(terms),
        std::move(typeDefs),
        std::move(methodDefs),
        std::move(methodRefs),
        std::move(fieldDefs),
        std::move(codes),
        std::move(std::string(name))
    );
}

} // namespace Symlevel
