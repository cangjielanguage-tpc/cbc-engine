#include "cbc_file.h"
#include "reader.h"


inline constexpr uint32_t MAGIC = 0xCBCDAFF0;

int curFileId = 0;


namespace Symlevel {

template <typename T> void ReadEntries(IO::StreamFileReader& reader, IO::FileId fileId, std::vector<std::unique_ptr<T>>& vector) {
    auto count = reader.ReadU32();
    vector.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        T* parsed = T::Parse(fileId, reader);
        vector.push_back(std::unique_ptr<T>(parsed));
    }
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

    std::vector<std::unique_ptr<Term>> terms;
    ReadEntries(reader, fileId, terms);

    std::vector<std::unique_ptr<TypeDefinition>> typeDefs;
    ReadEntries(reader, fileId, typeDefs);

    std::vector<std::unique_ptr<MethodDefinition>> methodDefs;
    ReadEntries(reader, fileId, methodDefs);

    std::vector<std::unique_ptr<MethodReference>> methodRefs;
    ReadEntries(reader, fileId, methodRefs);

    std::vector<std::unique_ptr<FieldDefinition>> fieldDefs;
    ReadEntries(reader, fileId, fieldDefs);

    std::vector<std::unique_ptr<Code>> codes;
    ReadEntries(reader, fileId, codes);


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