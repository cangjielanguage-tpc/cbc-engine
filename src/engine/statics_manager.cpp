#include "statics_manager.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/reader.h"
#include "field_layout.h"
#include "symlevel/definitions.h"
#include "symlevel/flags.h"
#include "terms.h"
#include "typeinfo_manager.h"
#include "utils/assertion.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static constexpr size_t RECORD_ALIGNMENT = sizeof(uintptr_t);

namespace Engine {

StaticFieldsBundle::StaticFieldsBundle(
    uintptr_t refs,
    uintptr_t primitives,
    uintptr_t records,
    uint32_t* recordOffsets,
    uint32_t* referenceOffsets,
    uint32_t refCount,
    uint32_t refOffsetsCount
)
    : refs(refs),
      primitives(primitives),
      records(records),
      recordOffsets(recordOffsets),
      referenceOffsets(referenceOffsets),
      refCount(refCount),
      refOffsetsCount(refOffsetsCount)
{}

StaticFieldsBundle::~StaticFieldsBundle()
{
    // FIXME: leaks on dlclose
}

SlotKind ComputeSlotKind(Session& session, FieldLayoutManager& flm, Symlevel::FieldDefinition& definition)
{
    auto fieldType = TermManager::Resolve(session, definition.FieldType());
    if (fieldType.IsReference()) {
        return REFERENCE;
    }
    switch (fieldType.GetKind()) {
        case TermKind::OPTION:
        case TermKind::UNION_ENUM:
        case TermKind::AOT_TYPE:
        case TermKind::TYPE:     return RECORD;
        default:                 return PRIMITIVE;
    }
}

uintptr_t StaticFieldsBundle::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    auto typeDef  = Symlevel::TypeDefinition::Resolve(session, typeIdent);
    auto fieldDef = Symlevel::FieldDefinition::Resolve(session, fieldIdent);

    auto flm                 = FieldLayoutManager::New(session);
    auto targetKind          = ComputeSlotKind(session, *flm, fieldDef);

    uint32_t fieldIdx = 0;

    for (auto fieldId : typeDef.GetFields().Entries(session)) {
        auto field = Symlevel::Reader::Read(session, fieldId);
        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            continue;
        }

        auto kind = ComputeSlotKind(session, *flm, field);
        if (targetKind != kind) {
            continue;
        }

        if (fieldIdent == field.Identifier()) {
            break;
        }

        fieldIdx++;
    };

    switch (targetKind) {
        case REFERENCE: {
            return this->refs + 8 * fieldIdx;
        }
        case PRIMITIVE: {
            // each slot is 8-byte size
            // TODO: implement compact representation
            return this->primitives + 8 * fieldIdx;
        }
        case RECORD: auto offs = recordOffsets[fieldIdx]; return records + offs;
    }
}

void StaticFieldsBundle::VisitRefLocations(std::function<void(RefLocation*)> action) const
{
    RefLocation* location = reinterpret_cast<RefLocation*>(refs);
    auto refCount         = this->refCount;
    for (auto i = 0; i < refCount; i++) {
        action(location);
        location++;
    }

    for (auto i = 0; i < refOffsetsCount; i++) {
        auto loc = records + referenceOffsets[i];
        action(reinterpret_cast<RefLocation*>(loc));
    }
}

StaticFieldsBundle StaticsManager::CreateBundle(Session& session, TypeIdent typeIdent)
{
    uint32_t refFieldsNum    = 0;
    uint32_t primFieldsNum   = 0;

    RTSupport::Log::gc.Log(Logging::Level::DEBUG, [&](Stream::Output& out0) {
        Stream::ResolvingOutput out(session, out0);
        out << "Start building sfb for " << Stream::Detailed(typeIdent) << Stream::endl;
    });

    auto flm     = FieldLayoutManager::New(session);
    auto& tim    = TypeInfoManager::Of(session);
    auto typeDef = Symlevel::TypeDefinition::Resolve(session, typeIdent);

    std::vector<uint32_t> refOffsetInRecords;
    std::vector<uint32_t> recordOffsets;

    uint32_t recordsSize = 0;
    std::vector<StaticTypedSlotInfo> typedSlotsInfo;

    for (auto fieldId : typeDef.GetFields().Entries(session)) {
        auto field = Symlevel::Reader::Read(session, fieldId);

        if (field.Flags().IsNot(Symlevel::FieldFlag::STATIC)) {
            continue;
        }

        auto kind = ComputeSlotKind(session, *flm, field);
        switch (kind) {
            case REFERENCE: refFieldsNum++; break;
            case PRIMITIVE: primFieldsNum++; break;
            case RECORD:    {
                auto fterm    = TermManager::Resolve(session, field.FieldType());
                auto size     = flm->GetFlatSize(fterm);
                auto alignedSize = MathUtils::AlignUp(size.value(), RECORD_ALIGNMENT); // FIXME: size can be absent
                auto offset      = recordsSize;
                recordOffsets.push_back(offset);
                flm->FillRefOffsets(fterm, refOffsetInRecords, offset);
                recordsSize += alignedSize;

                RTSupport::Log::gc.Log(Logging::Level::DEBUG, [&](Stream::Output& out0) {
                    Stream::ResolvingOutput out(session, out0);
                    out << "record field " << fterm << ". size: " << *size << ". offset: " << offset << Stream::endl;
                });
                break;
            }
        }
    };

    size_t totalSize     = primFieldsNum * 8 + refFieldsNum * 8 + recordsSize;
    void* memory         = malloc(totalSize);
    uint32_t* recOffsets = (uint32_t*)malloc(sizeof(uint32_t) * recordOffsets.size());
    uint32_t* refOffsets = (uint32_t*)malloc(sizeof(uint32_t) * refOffsetInRecords.size());
    if (!memory || !recOffsets || !refOffsets) {
        FATAL("Out of memory (SFB)");
    }
    memset(memory, 0, totalSize);

    uintptr_t mem        = reinterpret_cast<uintptr_t>(memory);
    uintptr_t primitives = mem;
    uintptr_t refs       = primitives + primFieldsNum * 8;
    uintptr_t records    = refs + refFieldsNum * 8;

    for (int i = 0; i < recordOffsets.size(); i++) {
        recOffsets[i] = recordOffsets[i];
    }
    for (int i = 0; i < refOffsetInRecords.size(); i++) {
        refOffsets[i] = refOffsetInRecords[i];
    }

    RTSupport::Log::gc.Log(Logging::Level::DEBUG, [&](Stream::Output& out0) {
        Stream::ResolvingOutput out(session, out0);
        out << "Built sfb for " << Stream::Detailed(typeIdent) << Stream::endl;
        out << "Primitives " << primitives << ". Count = " << primFieldsNum << Stream::endl;
        out << "References " << refs << ". Count = " << refFieldsNum << Stream::endl;
        out << "Records " << records << ". Size = " << recordsSize << Stream::endl;
    });

    return StaticFieldsBundle(
        refs, primitives, records, recOffsets, refOffsets, refFieldsNum, refOffsetInRecords.size()
    );
}

uintptr_t StaticsManager::GetLocation(Session& session, TypeIdent typeIdent, FieldIdent fieldIdent)
{
    std::lock_guard guard(lock);

    auto it = bundles.find(typeIdent.Pack());
    if (it == bundles.end()) {
        // move bundle, so the underlying vector won't be copied.
        bundles.try_emplace(typeIdent.Pack(), std::move(CreateBundle(session, typeIdent)));
        it = bundles.find(typeIdent.Pack());
    }

    return it->second.GetLocation(session, typeIdent, fieldIdent);
}

void StaticsManager::VisitRefLocations(
    std::function<void(RefLocation*)> untypedSlotsVisitor,
    std::function<void(uint8_t* base, const StaticTypedSlotInfo&)> typedSlotsVisitor
) const
{
    std::lock_guard guard(lock);

    for (const auto& [key, bundle] : bundles) {
        bundle.VisitRefLocations(untypedSlotsVisitor);
    }
}

} // namespace Engine
