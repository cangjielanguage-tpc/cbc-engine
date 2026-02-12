#pragma once

#include <vector>
#include "io/random_access_file.h"
#include "io/stream_file_reader.h"
#include "io/file_id.h"
#include "term.h"
#include "type_definition.h"
#include "method_definition.h"
#include "field_definition.h"
#include "method_reference.h"
#include "code.h"


namespace Symlevel {

class CbcFile {
public:

    static CbcFile* Create(IO::RandomAccessFile* file);

private:
    CbcFile(
        IO::FileId fileId,
        std::vector<std::unique_ptr<Term>> terms,
        std::vector<std::unique_ptr<TypeDefinition>> typeDefs,
        std::vector<std::unique_ptr<MethodDefinition>> methodDefs,
        std::vector<std::unique_ptr<MethodReference>> methodRefs,
        std::vector<std::unique_ptr<FieldDefinition>> fieldDefs,
        std::vector<std::unique_ptr<Code>> codes
    ):
        fileId    (fileId),
        terms     (std::move(terms)),
        typeDefs  (std::move(typeDefs)),
        methodDefs(std::move(methodDefs)),
        methodRefs(std::move(methodRefs)),
        fieldDefs (std::move(fieldDefs)),
        codes     (std::move(codes))
    {}

    const IO::FileId fileId;

    const std::vector<std::unique_ptr<Term>> terms;
    const std::vector<std::unique_ptr<TypeDefinition>> typeDefs;
    const std::vector<std::unique_ptr<MethodDefinition>> methodDefs;
    const std::vector<std::unique_ptr<MethodReference>> methodRefs;
    const std::vector<std::unique_ptr<FieldDefinition>> fieldDefs;
    const std::vector<std::unique_ptr<Code>> codes;

};

} // namespace Symlevel
