#pragma once

#include <vector>
#include "io/random_access_file.h"
#include "io/file_id.h"
#include "term_val.h"
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
        std::vector<TermValue> terms,
        std::vector<TypeDefinition> typeDefs,
        std::vector<MethodDefinition> methodDefs,
        std::vector<MethodReference> methodRefs,
        std::vector<FieldDefinition> fieldDefs,
        std::vector<Code> codes
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

    const std::vector<TermValue> terms;
    const std::vector<TypeDefinition> typeDefs;
    const std::vector<MethodDefinition> methodDefs;
    const std::vector<MethodReference> methodRefs;
    const std::vector<FieldDefinition> fieldDefs;
    const std::vector<Code> codes;

};

} // namespace Symlevel
