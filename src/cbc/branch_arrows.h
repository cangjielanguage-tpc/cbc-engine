#pragma once

#include "utils/ostream.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace Cbc {

inline std::string const kArrowHead  = "<";
inline std::string const kHorizontal = "─"; // U+2500
inline std::string const kDownCorner = "┐"; // U+2510
inline std::string const kUpCorner   = "┘"; // U+2518
inline std::string const kVertical   = "│"; // U+2502

/// Corner column of the rightmost (outermost) arrow, counted from the start of
/// the instruction text.
inline constexpr int64_t kArrowBaseColumn = 39;

/// Buffering output: collects the printed instruction lines together with the
/// bytecode offset of every line, so that arrows for backward branches can be
/// attached to the lines once the whole method has been parsed.
struct LineBuffer : public Stream::Output {
    struct Line {
        int64_t pos; // bytecode offset of the instruction printed on this line
        std::string text;
    };

    void SetPos(int64_t position) { pendingPos = position; }

    void VPrintFmt(const char* fmt, va_list argp) override
    {
        va_list copy;
        va_copy(copy, argp);
        int required = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);
        if (required <= 0) {
            return;
        }
        auto offset = current.size();
        current.resize(offset + static_cast<size_t>(required));
        vsnprintf(current.data() + offset, static_cast<size_t>(required) + 1, fmt, argp);
    }

    void NewLine() override
    {
        lines.push_back(Line{pendingPos, current});
        current.clear();
    }

    void Flush() const override {}

    std::vector<Line> lines;

private:
    int64_t pendingPos = 0;
    std::string current;
};

/// A recorded branch edge: `delta` is relative to the offset just past the
/// branch instruction (`end`), the target is `end + delta`.
struct Branch {
    int64_t start; // bytecode offset of the branch instruction
    int64_t end;   // bytecode offset just past the branch instruction
    int64_t delta; // branch offset relative to `end`
};

/// Renders the arrow suffixes for backward branches: one suffix string per
/// line (empty when the line needs no annotation). The corner column is fixed
/// relative to the instruction start; nested (overlapping) backward edges are
/// shifted one column to the right, the innermost edge closest to the code.
inline std::vector<std::string> BranchArrowSuffixes(
    std::vector<LineBuffer::Line> const& lines, std::vector<Branch> const& branches, int64_t prefixLen
)
{
    struct Arrow {
        int64_t target;
        int64_t branch;
        size_t corner; // corner column counted within a buffered line
    };

    std::unordered_map<int64_t, bool> lineStarts;
    for (auto const& line : lines) {
        lineStarts[line.pos] = true;
    }

    std::vector<Arrow> arrows;
    for (auto const& branch : branches) {
        auto target = branch.end + branch.delta;
        if (target >= branch.start || lineStarts.find(target) == lineStarts.end()) {
            continue;
        }
        // Keep the corner column clear of the longest instruction inside
        // the arrow span.
        int64_t spanMax = 0;
        for (auto const& line : lines) {
            if (target <= line.pos && line.pos <= branch.start) {
                spanMax = std::max<int64_t>(spanMax, static_cast<int64_t>(line.text.size()) - prefixLen);
            }
        }
        arrows.push_back(
            Arrow{target, branch.start, static_cast<size_t>(prefixLen + std::max(kArrowBaseColumn, spanMax + 2))}
        );
    }

    // Assign columns: the innermost backward edges take the column closest
    // to the code, every edge overlapping an already placed (earlier
    // ending) edge is moved one column to the right.
    std::sort(arrows.begin(), arrows.end(), [](Arrow const& l, Arrow const& r) { return l.branch < r.branch; });
    for (size_t i = 0; i < arrows.size(); i++) {
        for (size_t j = 0; j < i; j++) {
            if (arrows[j].branch >= arrows[i].target) {
                arrows[i].corner = std::max(arrows[i].corner, arrows[j].corner + 1);
            }
        }
    }

    std::vector<std::string> suffixes(lines.size());
    for (size_t l = 0; l < lines.size(); l++) {
        auto const& line = lines[l];
        auto textLen     = static_cast<int64_t>(line.text.size());

        // First pass: horizontal runs, then corners and arrow heads take
        // precedence over them.
        std::map<int64_t, std::string const*> slots;
        int64_t maxCorner = -1;
        for (auto const& arrow : arrows) {
            if (arrow.target > line.pos || arrow.branch < line.pos) {
                continue;
            }
            if (arrow.target == line.pos) {
                for (auto c = textLen + 2; c < static_cast<int64_t>(arrow.corner); c++) {
                    slots[c] = &kHorizontal;
                }
            } else if (arrow.branch == line.pos) {
                for (auto c = textLen + 1; c < static_cast<int64_t>(arrow.corner); c++) {
                    slots[c] = &kHorizontal;
                }
            }
            maxCorner = std::max<int64_t>(maxCorner, static_cast<int64_t>(arrow.corner));
        }
        for (auto const& arrow : arrows) {
            if (arrow.target > line.pos || arrow.branch < line.pos) {
                continue;
            }
            if (arrow.target == line.pos) {
                slots[textLen + 1] = &kArrowHead;
                slots[static_cast<int64_t>(arrow.corner)] = &kDownCorner;
            } else if (arrow.branch == line.pos) {
                slots[static_cast<int64_t>(arrow.corner)] = &kUpCorner;
            } else {
                slots[static_cast<int64_t>(arrow.corner)] = &kVertical;
            }
        }

        if (maxCorner < 0) {
            continue;
        }
        auto& suffix = suffixes[l];
        for (auto c = textLen; c <= maxCorner; c++) {
            auto slot = slots.find(c);
            suffix += slot != slots.end() ? *slot->second : " ";
        }
    }
    return suffixes;
}

} // namespace Cbc
