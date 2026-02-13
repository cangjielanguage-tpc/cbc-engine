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

    static constexpr uint32_t MAGIC = 0xCBCDAFF0;

    static CbcFile Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name);

private:
    CbcFile(
        IO::FileId fileId,
        std::vector<TermValue> terms,
        std::vector<TypeDefinition> typeDefs,
        std::vector<MethodDefinition> methodDefs,
        std::vector<MethodReference> methodRefs,
        std::vector<FieldDefinition> fieldDefs,
        std::vector<Code> codes,
        std::string name
    ):
        fileId    (fileId),
        terms     (std::move(terms)),
        typeDefs  (std::move(typeDefs)),
        methodDefs(std::move(methodDefs)),
        methodRefs(std::move(methodRefs)),
        fieldDefs (std::move(fieldDefs)),
        codes     (std::move(codes)),
        name      (std::move(name))
    {}

    const IO::FileId fileId;

    const std::vector<TermValue> terms;
    const std::vector<TypeDefinition> typeDefs;
    const std::vector<MethodDefinition> methodDefs;
    const std::vector<MethodReference> methodRefs;
    const std::vector<FieldDefinition> fieldDefs;
    const std::vector<Code> codes;
    const std::string name;
};

} // namespace Symlevel
