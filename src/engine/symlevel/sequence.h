#pragma once

#include "engine/engine.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "utils/iterators.h"

#include <cstdint>
#include <optional>

namespace Symlevel {

class Sequence {
public:
    struct Generator;

    using Range = Iterators::SimpleRange<Generator>;

    Sequence(IO::FileId file, uint32_t startPos, uint32_t endPos);

    static Sequence Empty();

    static Sequence Parse(IO::StreamFileReader& reader, IO::FileId id);
    Generator Values(Engine::Session& session) const;
    IO::FileId FileId() const;

private:
    IO::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

struct Sequence::Generator {
    IO::StreamFileReader reader;
    Sequence seq;

    std::optional<uint32_t> operator()();
};

template <typename T> class OffsetSequence {
public:
    OffsetSequence(Sequence seq) : seq(seq) {}
    OffsetSequence() : seq(Sequence::Empty()) {}

    static OffsetSequence Parse(IO::StreamFileReader& reader, IO::FileId id)
    {
        return { Sequence::Parse(reader, id) };
    }

    static OffsetSequence Empty() { return { Sequence::Empty() }; }

    struct Generator {
        Sequence::Generator sgen;

        std::optional<Engine::Identifier<T>> operator()()
        {
            auto offset = sgen();
            if (offset.has_value()) {
                return Engine::Identifier(Offset<T>(*offset), sgen.seq.FileId());
            }
            return std::nullopt;
        }
    };

    using Range = Iterators::SimpleRange<Generator>;

    Range Values(Engine::Session& session) const
    {
        return Iterators::MakeRange(Generator {
            seq.Values(session)
        });
    }

private:
    Sequence seq;
};

template <typename T> class RefSequence {
public:
    RefSequence(Sequence seq) : seq(seq) {}

    RefSequence() : seq(Sequence::Empty()) {}

    static RefSequence Parse(IO::StreamFileReader& reader, IO::FileId id) { return { Sequence::Parse(reader, id) }; }

    struct Generator {
        Sequence::Generator sgen;

        std::optional<Engine::RefIdentifier<T>> operator()()
        {
            auto _id = sgen();
            if (_id.has_value()) {
                auto id = static_cast<uint32_t>(*_id);
                return Engine::RefIdentifier(RefId<T>(id), sgen.seq.FileId());
            }
            return std::nullopt;
        }
    };

    using Range = Iterators::SimpleRange<Generator>;

    Range Values(Engine::Session& session) const { return Iterators::MakeRange(Generator { seq.Values(session) }); }

private:
    Sequence seq;
};

} // namespace Symlevel
