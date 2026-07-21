#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/reader.h"
#include "utils/iterators.h"
#include <cstdint>
#include <optional>
#include <string_view>

namespace Symlevel {

class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

struct MemberIndex {
    IO::FileId fileId;

    uint32_t bucketTableStart;
    uint32_t bucketTableSize;

    uint32_t bucketsStart;
    uint32_t bucketsSize;

    struct Generator;

    MemberIndex::Generator AllEntries(Engine::Session& session) const;
    MemberIndex::Generator FindBucket(Engine::Session& session, std::string_view name) const;
    static MemberIndex Read(IO::FileId fileId, IO::StreamFileReader& reader);
};

struct MemberIndex::Generator {
    MemberIndex index;
    IO::RandomAccessFile* raf;
    uint32_t cursor;
    uint32_t endIdx;

    std::optional<uint32_t> operator()();
};

template <typename T> class MemberIndexBase {
public:
    MemberIndex index;

    MemberIndexBase(IO::StreamFileReader& reader, IO::FileId fileId) : index(MemberIndex::Read(fileId, reader)) {}

    struct FilteredGenerator {
        MemberIndex::Generator gen;
        std::string_view name;
        Engine::Session* session;

        std::optional<Engine::Identifier<T>> operator()()
        {
            for (auto res = gen(); res; res = gen()) {
                auto offs = Offset<T>(*res);
                auto name = Reader::ReadName(*session, gen.index.fileId, offs);
                if (this->name.compare(name) == 0) {
                    return Engine::Identifier<T>(offs, gen.index.fileId);
                }
            }
            return std::nullopt;
        }
    };

    struct Generator {
        MemberIndex::Generator gen;

        std::optional<Engine::Identifier<T>> operator()()
        {
            auto res = gen();
            if (res) {
                return Engine::Identifier<T>(Offset<T>(*res), gen.index.fileId);
            }
            return std::nullopt;
        }
    };

    using FilteredRange = Iterators::SimpleRange<FilteredGenerator>;
    using Range         = Iterators::SimpleRange<Generator>;

    std::optional<Engine::Identifier<T>> Find(Engine::Session& session, std::string_view name) const
    {
        FilteredGenerator gen { .gen = index.FindBucket(session, name), .name = name, .session = &session };
        return gen();
    }

    FilteredRange FindAll(Engine::Session& session, std::string_view name) const
    {
        FilteredGenerator gen { .gen = index.FindBucket(session, name), .name = name, .session = &session };
        return Iterators::MakeRange(std::move(gen));
    }

    Range Entries(Engine::Session& sesion) const
    {
        Generator gen { index.AllEntries(sesion) };
        return Iterators::MakeRange(std::move(gen));
    }
};

class TypeIndex : public MemberIndexBase<TypeDefinition> {
public:
    TypeIndex(IO::StreamFileReader& reader, IO::FileId fileId) : MemberIndexBase(reader, fileId) {}
};

using FieldIndex  = MemberIndexBase<FieldDefinition>;
using MethodIndex = MemberIndexBase<MethodDefinition>;

} // namespace Symlevel
