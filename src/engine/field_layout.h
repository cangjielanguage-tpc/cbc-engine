#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "utils/logger.h"
#include <cstdint>
#include <memory>

namespace Engine {

/// Full layout of fields for some type.
/// The layout could be partially built for some cases.
class FieldLayout {
public:
    struct Entry {
        std::optional<Identifier<Image::FieldDefinition>> definition;
        Term fieldType;
        std::optional<uint32_t> offset;
    };

    struct SizeDesc {
        std::optional<uint32_t> size{0};
        uint8_t alignment{1}; // required for records, for references - always 8.
    };

    struct Content {
        std::vector<Entry> fields;
        SizeDesc desc{};
    };

    FieldLayout(Content&& content);
    std::shared_ptr<Content const> operator->() const;
    Content const& operator*() const;

private:
    std::shared_ptr<Content> content;
};

class FieldLayoutManager {
public:
    static std::unique_ptr<FieldLayoutManager> New(Session& session);
    static std::unique_ptr<FieldLayoutManager> New(Session& session, TypeInfoManager& typeInfoManager);

    /// Computes the layout of fields of the type provided by term.
    /// Expects only terms which are backed by TypeDefinition (classes and structs).
    ///
    /// In case of errors during the build process (e.g. resolution failure)
    /// `nullopt` is returned.
    virtual std::optional<FieldLayout> GetLayout(Term term) = 0;

    /// The size of a field of given type and its alignment.
    virtual std::optional<uint32_t> GetFlatSize(Term term) = 0;

    /// The alignment of a field of given type and its alignment.
    virtual uint8_t GetFlatAlignment(Term term) = 0;

    /// Fills out the `offsets` vector with all reference offsets of a value of type `Term`.
    virtual void FillRefOffsets(Term term, std::vector<uint32_t>& offsets, uint32_t disp) = 0;

    virtual ~FieldLayoutManager();
};

namespace Log {
/// Logger for field layout building and querying.
/// DEBUG - log field layout structure
/// INFO  - log queries of field layout
/// ERROR - log errors
extern Logging::Logger fields;
} // namespace Log

} // namespace Engine
