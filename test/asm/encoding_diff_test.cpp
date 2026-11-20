// Encoding differential against a system assembler.
//
// Every case is rendered twice: as assembly text, which LLVM MC assembles, and
// as our encoder's word. The whole corpus goes through one `llvm-mc` and one
// `llvm-objcopy` invocation, the resulting .text section is read back as
// little-endian words, and the two word streams must agree. Nothing here needs
// an aarch64 host: the oracle is cross-target, so the test runs everywhere.
//
// Two tests:
//
//   EncodingDiff.Systematic  a bounded sweep of the whole implemented menu,
//                            runs with the rest of the suite;
//   EncodingDiff.Exhaustive  the same sweep over the full operand space, opt-in
//                            through ASM_ENCODING_DIFF_FULL=1.
//
// ASM_FUZZ_ITERS=N adds N randomly parameterized sweeps on top.
//
// A form is only admitted to the table when our encoder exposes it. Where the
// architecture has a form our API does not model, the assembler is the wrong
// place to notice; the omission is deliberate and lives in the design record.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/wait.h>

#include "asm/encoding.h"
#include "asm/insn.h"
#include "asm/labels.h"
#include "asm/operands.h"

namespace {

using namespace Asm;

// A case may not exceed these; exceeding them means the sweep stopped being a
// sweep and the caps need revisiting rather than the pools growing silently.
constexpr size_t kSystematicCap = 10000;
constexpr size_t kExhaustiveCap = 4000000;

// ---------------------------------------------------------------------------
// Shell helpers
// ---------------------------------------------------------------------------

std::string Quote(const std::string& token)
{
    std::string out = "'";
    for (char c : token) {
        out += (c == '\'') ? "'\\''" : std::string(1, c);
    }
    out += "'";
    return out;
}

int RunCapture(const std::string& command, std::string& output)
{
    std::FILE* pipe = ::popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return -1;
    }
    char buffer[512];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof buffer, pipe)) > 0) {
        output.append(buffer, count);
    }
    const int status = ::pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int Run(const std::string& command)
{
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::string ReadAll(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string Head(std::string text, size_t limit)
{
    if (text.size() > limit) {
        text.resize(limit);
        text += "\n... (truncated)";
    }
    return text;
}

// ---------------------------------------------------------------------------
// Oracle
// ---------------------------------------------------------------------------

struct Oracle {
    std::string mc;
    std::string objcopy;

    bool Available() const { return !mc.empty() && !objcopy.empty(); }
};

std::string FindTool(const char* env, std::initializer_list<const char*> names)
{
    if (const char* override = std::getenv(env); override != nullptr && *override != '\0') {
        return override;
    }
    for (const char* name : names) {
        std::string output;
        if (RunCapture(std::string { "command -v " } + name + " 2>/dev/null", output) == 0) {
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }
            if (!output.empty()) {
                return output;
            }
        }
    }
    return {};
}

Oracle FindOracle()
{
    Oracle oracle;
    oracle.mc      = FindTool("ASM_ORACLE_LLVM_MC", { "llvm-mc" });
    oracle.objcopy = FindTool("ASM_ORACLE_LLVM_OBJCOPY", { "llvm-objcopy" });
    return oracle;
}

// The claim is only "same bytes for this LLVM revision and triple", so an oracle
// without the target is no oracle at all.
bool SupportsAarch64(const Oracle& oracle, std::string& version)
{
    if (RunCapture(Quote(oracle.mc) + " --version 2>&1", version) != 0) {
        return false;
    }
    return version.find("aarch64") != std::string::npos;
}

// ---------------------------------------------------------------------------
// Corpus
// ---------------------------------------------------------------------------

struct Case {
    std::string text;
    uint32_t word;
};

class Corpus {
public:
    void Add(std::string text, Insn insn) { cases_.push_back({ std::move(text), insn.Bits() }); }

    // For cases already carrying their word (merging corpora).
    void AddWord(std::string text, uint32_t word) { cases_.push_back({ std::move(text), word }); }

    size_t Size() const { return cases_.size(); }

    const std::vector<Case>& Cases() const { return cases_; }

private:
    std::vector<Case> cases_;
};

// ---------------------------------------------------------------------------
// Operands
// ---------------------------------------------------------------------------

XReg X(int code) { return XReg { static_cast<uint8_t>(code) }; }

WReg W(int code) { return WReg { static_cast<uint8_t>(code) }; }

std::string Xs(int code) { return "x" + std::to_string(code); }

std::string Ws(int code) { return "w" + std::to_string(code); }

std::string Hex(uint64_t value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

std::string OffsetText(int64_t delta)
{
    return delta >= 0 ? ".+" + std::to_string(delta) : ".-" + std::to_string(-delta);
}

std::vector<Cc> AllConds()
{
    return { Cc::Eq, Cc::Ne, Cc::Hs, Cc::Lo, Cc::Mi, Cc::Pl, Cc::Vs, Cc::Vc,
             Cc::Hi, Cc::Ls, Cc::Ge, Cc::Lt, Cc::Gt, Cc::Le, Cc::Al, Cc::Nv };
}

const char* CondName(Cc cc)
{
    switch (cc) {
        case Cc::Eq: return "eq";
        case Cc::Ne: return "ne";
        case Cc::Hs: return "hs";
        case Cc::Lo: return "lo";
        case Cc::Mi: return "mi";
        case Cc::Pl: return "pl";
        case Cc::Vs: return "vs";
        case Cc::Vc: return "vc";
        case Cc::Hi: return "hi";
        case Cc::Ls: return "ls";
        case Cc::Ge: return "ge";
        case Cc::Lt: return "lt";
        case Cc::Gt: return "gt";
        case Cc::Le: return "le";
        case Cc::Al: return "al";
        case Cc::Nv: return "nv";
    }
    return "eq";
}

// The accepted range of a fixup delta, both ends inclusive and multiples of 4:
// ApplyFixup's assertion, so the generated corpus can never trip it.
struct DeltaRange {
    int64_t lo;
    int64_t hi;
};

DeltaRange RangeOf(FixupKind kind)
{
    switch (kind) {
        case FixupKind::Branch26:     return { -((int64_t { 1 } << 25) * 4 - 4), (int64_t { 1 } << 25) * 4 - 4 };
        case FixupKind::CondBranch19:
        case FixupKind::CmpBranch19:  return { -((int64_t { 1 } << 18) * 4 - 4), (int64_t { 1 } << 18) * 4 - 4 };
        case FixupKind::TestBranch14: return { -((int64_t { 1 } << 13) * 4 - 4), (int64_t { 1 } << 13) * 4 - 4 };
        case FixupKind::Adr21:        return { -(int64_t { 1 } << 20), (int64_t { 1 } << 20) - 4 };
    }
    return { 0, 0 };
}

bool InRange(FixupKind kind, int64_t delta)
{
    const DeltaRange range = RangeOf(kind);
    return delta >= range.lo && delta <= range.hi;
}

// ---------------------------------------------------------------------------
// Logical immediates
// ---------------------------------------------------------------------------

// The architecture's DecodeBitMasks. It is used only to enumerate *valid*
// logical immediates, so the generated corpus is always encodable; it is the
// ARM ARM's decoding pseudocode, written independently of our encoder.
bool DecodeBitMask(unsigned n, unsigned immr, unsigned imms, unsigned width, uint64_t& out)
{
    const unsigned top = (n << 6) | ((~imms) & 0x3Fu);
    unsigned len       = 0;
    bool found         = false;
    for (int bit = 6; bit >= 0; --bit) {
        if ((top >> bit) & 1u) {
            len   = static_cast<unsigned>(bit);
            found = true;
            break;
        }
    }
    if (!found || (n == 1 && width == 32)) {
        return false;
    }
    const unsigned esize = 1u << len;
    if (esize > width) {
        return false;
    }
    const unsigned levels = esize - 1u;
    const unsigned s      = imms & levels;
    const unsigned r      = immr & levels;
    if (s == levels) { // an all-ones element has no encoding
        return false;
    }
    uint64_t element = 0;
    for (unsigned i = 0; i <= s; ++i) {
        element |= uint64_t { 1 } << i;
    }
    uint64_t rotated = 0;
    for (unsigned i = 0; i < esize; ++i) {
        if ((element >> ((i + r) % esize)) & 1u) {
            rotated |= uint64_t { 1 } << i;
        }
    }
    uint64_t value = 0;
    for (unsigned i = 0; i < width; i += esize) {
        value |= rotated << i;
    }
    out = value;
    return true;
}

std::vector<uint64_t> EnumerateLogical(unsigned width)
{
    std::vector<uint64_t> values;
    const unsigned nLimit = width == 64 ? 2 : 1;
    for (unsigned n = 0; n < nLimit; ++n) {
        for (unsigned immr = 0; immr < 64; ++immr) {
            for (unsigned imms = 0; imms < 64; ++imms) {
                uint64_t value = 0;
                if (DecodeBitMask(n, immr, imms, width, value)) {
                    values.push_back(value);
                }
            }
        }
    }
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

std::vector<uint64_t> Sample(const std::vector<uint64_t>& all, size_t count)
{
    std::vector<uint64_t> out;
    if (all.empty() || count == 0) {
        return out;
    }
    const size_t step = std::max<size_t>(1, all.size() / count);
    for (size_t i = 0; i < all.size() && out.size() < count; i += step) {
        out.push_back(all[i]);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Operand pools
// ---------------------------------------------------------------------------

struct Triple {
    int d;
    int n;
    int m;
};

struct Pool {
    // Register triples swept against the full immediate/amount lists.
    std::vector<Triple> triples;
    // Triples swept against the sample lists; empty in bounded mode.
    std::vector<Triple> sampleTriples;
    std::vector<uint32_t> imms12;
    std::vector<uint32_t> sampleImms12;
    std::vector<uint32_t> amounts;
    std::vector<uint32_t> sampleAmounts;
    std::vector<uint64_t> logicalImms;
    std::vector<uint64_t> logicalImms32;
    std::vector<uint64_t> sampleLogical;
    std::vector<uint64_t> sampleLogical32;
    // (immr, imms) pairs for the bitfield forms, and (lsb, width) pairs for the
    // extract aliases of them.
    std::vector<std::pair<uint32_t, uint32_t>> immPairs;
    std::vector<std::pair<uint32_t, uint32_t>> sampleImmPairs;
    std::vector<std::pair<uint32_t, uint32_t>> extracts;
    std::vector<std::pair<uint32_t, uint32_t>> sampleExtracts;
    std::vector<int64_t> deltas;
    std::vector<Cc> conds;
    std::vector<uint32_t> bits;
    std::vector<int> branchRegs;
    // (rt, base, index) of the address forms, the byte offsets the unscaled
    // forms sweep, and the element-counted offsets of the pair forms.
    std::vector<Triple> memTriples;
    std::vector<Triple> sampleMemTriples;
    std::vector<int32_t> offsets;
    std::vector<int32_t> sampleOffsets;
    std::vector<int32_t> pairImms;
    std::vector<int32_t> samplePairImms;
};

std::vector<Triple> TriplesWithDistinctM(const std::vector<int>& regs)
{
    std::vector<Triple> triples;
    for (size_t i = 0; i < regs.size(); ++i) {
        for (size_t j = 0; j < regs.size(); ++j) {
            // m differs from d, so a swapped Rd/Rm field cannot survive.
            triples.push_back({ regs[i], regs[j], regs[(i + 1) % regs.size()] });
        }
    }
    return triples;
}

Pool BoundedPool()
{
    const std::vector<int> regs { 0, 1, 7, 15, 30 };
    Pool pool;
    pool.triples    = TriplesWithDistinctM(regs);
    pool.branchRegs = regs;
    pool.imms12     = { 0, 4095 };
    pool.amounts    = { 0, 31, 63 };
    pool.deltas     = { 0, 4, -4, 8, -8 };
    pool.conds      = AllConds();
    pool.bits       = { 0, 1, 31, 32, 62, 63 };
    pool.immPairs   = { { 0, 0 }, { 0, 63 }, { 63, 63 }, { 1, 58 }, { 8, 15 } };
    pool.extracts   = { { 0, 1 }, { 0, 64 }, { 8, 8 }, { 63, 1 }, { 31, 32 } };
    pool.memTriples = { { 0, 1, 2 }, { 30, 7, 15 } };
    pool.offsets    = { -256, -255, -1, 0, 1, 8, 255 };
    pool.pairImms   = { -64, -1, 0, 1, 63 };

    const std::vector<uint64_t> logical64 = EnumerateLogical(64);
    const std::vector<uint64_t> logical32 = EnumerateLogical(32);
    pool.logicalImms                      = Sample(logical64, 5);
    pool.logicalImms32                    = Sample(logical32, 4);
    return pool;
}

Pool ExhaustivePool()
{
    Pool pool;
    pool.triples = { { 0, 1, 7 },   { 1, 0, 15 }, { 7, 15, 30 }, { 15, 7, 0 },  { 30, 0, 1 }, { 0, 30, 7 },
                     { 1, 30, 15 }, { 30, 1, 0 }, { 15, 30, 7 }, { 30, 15, 1 }, { 7, 30, 0 }, { 1, 7, 30 } };
    std::vector<int> allRegs;
    for (int code = 0; code <= 30; ++code) {
        allRegs.push_back(code);
    }
    pool.sampleTriples = TriplesWithDistinctM(allRegs);
    pool.branchRegs    = allRegs;
    pool.imms12        = { 0, 1, 2, 255, 256, 2047, 2048, 4094, 4095 };
    pool.sampleImms12  = { 0, 4095 };
    pool.amounts       = { 0, 1, 2, 31, 32, 63 };
    pool.sampleAmounts = { 0, 63 };
    pool.deltas        = { 0, 4, -4, 8, -8, 16, -16, 32764, -32764, 1048572, -1048572, 1048572 + 4, 134217724 };
    pool.conds         = AllConds();
    pool.bits          = {};
    for (uint32_t bit = 0; bit <= 63; ++bit) {
        pool.bits.push_back(bit);
    }
    pool.logicalImms     = EnumerateLogical(64);
    pool.logicalImms32   = EnumerateLogical(32);
    pool.sampleLogical   = Sample(pool.logicalImms, 4);
    pool.sampleLogical32 = Sample(pool.logicalImms32, 3);
    for (uint32_t immr = 0; immr <= 63; ++immr) {
        for (uint32_t imms = 0; imms <= 63; ++imms) {
            pool.immPairs.push_back({ immr, imms });
        }
    }
    pool.sampleImmPairs = { { 0, 63 }, { 63, 63 }, { 1, 58 }, { 8, 15 } };
    // Every width at a handful of offsets, so each imms value is swept.
    for (uint32_t lsb : { 0u, 1u, 8u, 31u, 63u }) {
        for (uint32_t width = 1; lsb + width <= 64; ++width) {
            pool.extracts.push_back({ lsb, width });
        }
    }
    pool.sampleExtracts = { { 0, 64 }, { 8, 8 }, { 63, 1 }, { 31, 32 } };
    pool.memTriples     = { { 0, 1, 2 }, { 30, 0, 1 }, { 1, 7, 15 }, { 15, 30, 0 } };
    pool.sampleMemTriples = { { 7, 2, 30 }, { 2, 15, 7 }, { 30, 15, 1 } };
    for (int32_t offset = -256; offset <= 255; ++offset) {
        pool.offsets.push_back(offset);
    }
    pool.sampleOffsets = { -256, -1, 0, 1, 255 };
    for (int32_t imm7 = -64; imm7 <= 63; ++imm7) {
        pool.pairImms.push_back(imm7);
    }
    pool.samplePairImms = { -64, 0, 63 };
    return pool;
}

Pool RandomPool(std::mt19937& rng, const std::vector<uint64_t>& logical64, const std::vector<uint64_t>& logical32)
{
    std::uniform_int_distribution<int> regDist(0, 30);
    const int a = regDist(rng);
    int b       = regDist(rng);
    while (b == a) {
        b = regDist(rng);
    }
    Pool pool;
    pool.triples                = { { a, b, b }, { b, a, a } };
    pool.branchRegs             = { a, b };
    pool.imms12                 = { static_cast<uint32_t>(rng() % 4096) };
    pool.amounts                = { static_cast<uint32_t>(rng() % 64) };
    pool.deltas                 = { static_cast<int64_t>(rng() % 8192) * 4 - 16384 };
    const std::vector<Cc> conds = AllConds();
    pool.conds                  = { conds[rng() % conds.size()] };
    pool.bits                   = { static_cast<uint32_t>(rng() % 64) };
    pool.logicalImms            = { logical64[rng() % logical64.size()] };
    pool.logicalImms32          = { logical32[rng() % logical32.size()] };
    pool.immPairs               = { { static_cast<uint32_t>(rng() % 64), static_cast<uint32_t>(rng() % 64) } };
    const uint32_t width        = 1 + static_cast<uint32_t>(rng() % 64);
    pool.extracts               = { { static_cast<uint32_t>(rng() % (65 - width)), width } };
    pool.memTriples             = { { a, b, b } };
    pool.offsets                = { static_cast<int32_t>(rng() % 512) - 256 };
    pool.pairImms               = { static_cast<int32_t>(rng() % 128) - 64 };
    return pool;
}

// ---------------------------------------------------------------------------
// Instruction forms
// ---------------------------------------------------------------------------

void EmitAddSubImmediate(Corpus& corpus, const Triple& t, uint32_t imm)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n), wd = Ws(t.d), wn = Ws(t.n);
    const std::string s = std::to_string(imm);
    corpus.Add("add " + xd + ", " + xn + ", #" + s, Encode::Add(X(t.d), X(t.n), imm));
    corpus.Add("add " + xd + ", " + xn + ", #" + s + ", lsl #12", Encode::Add(X(t.d), X(t.n), imm, Lsl12));
    corpus.Add("sub " + xd + ", " + xn + ", #" + s, Encode::Sub(X(t.d), X(t.n), imm));
    corpus.Add("sub " + xd + ", " + xn + ", #" + s + ", lsl #12", Encode::Sub(X(t.d), X(t.n), imm, Lsl12));
    corpus.Add("add " + xd + ", sp, #" + s, Encode::Add(X(t.d), Sp, imm));
    corpus.Add("add " + xd + ", sp, #" + s + ", lsl #12", Encode::Add(X(t.d), Sp, imm, Lsl12));
    corpus.Add("sub " + xd + ", sp, #" + s, Encode::Sub(X(t.d), Sp, imm));
    corpus.Add("sub " + xd + ", sp, #" + s + ", lsl #12", Encode::Sub(X(t.d), Sp, imm, Lsl12));
    corpus.Add("add sp, " + xn + ", #" + s, Encode::Add(Sp, X(t.n), imm));
    corpus.Add("sub sp, " + xn + ", #" + s, Encode::Sub(Sp, X(t.n), imm));
    corpus.Add("add sp, sp, #" + s, Encode::Add(Sp, Sp, imm));
    corpus.Add("sub sp, sp, #" + s, Encode::Sub(Sp, Sp, imm));
    corpus.Add("add " + wd + ", " + wn + ", #" + s, Encode::Add(W(t.d), W(t.n), imm));
    corpus.Add("add " + wd + ", " + wn + ", #" + s + ", lsl #12", Encode::Add(W(t.d), W(t.n), imm, Lsl12));
    corpus.Add("sub " + wd + ", " + wn + ", #" + s, Encode::Sub(W(t.d), W(t.n), imm));
    corpus.Add("sub " + wd + ", " + wn + ", #" + s + ", lsl #12", Encode::Sub(W(t.d), W(t.n), imm, Lsl12));
    corpus.Add("adds " + xd + ", " + xn + ", #" + s, Encode::Adds(X(t.d), X(t.n), imm));
    corpus.Add("subs " + xd + ", " + xn + ", #" + s, Encode::Subs(X(t.d), X(t.n), imm));
    corpus.Add("adds " + wd + ", " + wn + ", #" + s, Encode::Adds(W(t.d), W(t.n), imm));
    corpus.Add("subs " + wd + ", " + wn + ", #" + s, Encode::Subs(W(t.d), W(t.n), imm));
    corpus.Add("adds " + xd + ", sp, #" + s, Encode::Adds(X(t.d), Sp, imm));
    corpus.Add("subs " + xd + ", sp, #" + s, Encode::Subs(X(t.d), Sp, imm));
    corpus.Add("cmn " + xn + ", #" + s, Encode::Adds(Xzr, X(t.n), imm));
    corpus.Add("cmp " + xn + ", #" + s, Encode::Subs(Xzr, X(t.n), imm));
    corpus.Add("cmn " + xn + ", #" + s + ", lsl #12", Encode::Adds(Xzr, X(t.n), imm, Lsl12));
    corpus.Add("cmp " + xn + ", #" + s + ", lsl #12", Encode::Subs(Xzr, X(t.n), imm, Lsl12));
    corpus.Add("cmn " + wn + ", #" + s, Encode::Adds(Wzr, W(t.n), imm));
    corpus.Add("cmp " + wn + ", #" + s, Encode::Subs(Wzr, W(t.n), imm));
    corpus.Add("cmn sp, #" + s, Encode::Adds(Xzr, Sp, imm));
    corpus.Add("cmp sp, #" + s, Encode::Subs(Xzr, Sp, imm));
}

void EmitAddSubShifted(Corpus& corpus, const Triple& t, Shift shift, const char* name, uint32_t amount)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n), xm = Xs(t.m);
    const std::string wd = Ws(t.d), wn = Ws(t.n), wm = Ws(t.m);
    const std::string off = std::string(", ") + name + " #" + std::to_string(amount);
    corpus.Add("add " + xd + ", " + xn + ", " + xm + off, Encode::Add(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("sub " + xd + ", " + xn + ", " + xm + off, Encode::Sub(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("adds " + xd + ", " + xn + ", " + xm + off, Encode::Adds(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("subs " + xd + ", " + xn + ", " + xm + off, Encode::Subs(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("cmp " + xn + ", " + xm + off, Encode::Subs(Xzr, X(t.n), X(t.m), shift, amount));
    corpus.Add("cmn " + xn + ", " + xm + off, Encode::Adds(Xzr, X(t.n), X(t.m), shift, amount));
    if (amount <= 31) {
        corpus.Add("add " + wd + ", " + wn + ", " + wm + off, Encode::Add(W(t.d), W(t.n), W(t.m), shift, amount));
        corpus.Add("sub " + wd + ", " + wn + ", " + wm + off, Encode::Sub(W(t.d), W(t.n), W(t.m), shift, amount));
        corpus.Add("cmp " + wn + ", " + wm + off, Encode::Subs(Wzr, W(t.n), W(t.m), shift, amount));
        corpus.Add("cmn " + wn + ", " + wm + off, Encode::Adds(Wzr, W(t.n), W(t.m), shift, amount));
    }
}

void EmitLogical64(Corpus& corpus, const Triple& t, uint64_t imm)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n), h = Hex(imm);
    corpus.Add("and " + xd + ", " + xn + ", #" + h, Encode::And(X(t.d), X(t.n), imm));
    corpus.Add("orr " + xd + ", " + xn + ", #" + h, Encode::Orr(X(t.d), X(t.n), imm));
    corpus.Add("eor " + xd + ", " + xn + ", #" + h, Encode::Eor(X(t.d), X(t.n), imm));
    corpus.Add("ands " + xd + ", " + xn + ", #" + h, Encode::Ands(X(t.d), X(t.n), imm));
    corpus.Add("tst " + xn + ", #" + h, Encode::Ands(Xzr, X(t.n), imm));
    corpus.Add("and sp, " + xn + ", #" + h, Encode::And(Sp, X(t.n), imm));
    corpus.Add("orr sp, " + xn + ", #" + h, Encode::Orr(Sp, X(t.n), imm));
    corpus.Add("eor sp, " + xn + ", #" + h, Encode::Eor(Sp, X(t.n), imm));
}

void EmitLogical32(Corpus& corpus, const Triple& t, uint32_t imm)
{
    const std::string wd = Ws(t.d), wn = Ws(t.n), h = Hex(imm);
    corpus.Add("and " + wd + ", " + wn + ", #" + h, Encode::And(W(t.d), W(t.n), imm));
    corpus.Add("orr " + wd + ", " + wn + ", #" + h, Encode::Orr(W(t.d), W(t.n), imm));
    corpus.Add("eor " + wd + ", " + wn + ", #" + h, Encode::Eor(W(t.d), W(t.n), imm));
    corpus.Add("ands " + wd + ", " + wn + ", #" + h, Encode::Ands(W(t.d), W(t.n), imm));
    corpus.Add("tst " + wn + ", #" + h, Encode::Ands(Wzr, W(t.n), imm));
}

void EmitLogicalShifted(Corpus& corpus, const Triple& t, Shift shift, const char* name, uint32_t amount)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n), xm = Xs(t.m);
    const std::string wd = Ws(t.d), wn = Ws(t.n), wm = Ws(t.m);
    const std::string off = std::string(", ") + name + " #" + std::to_string(amount);
    corpus.Add("and " + xd + ", " + xn + ", " + xm + off, Encode::And(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("orr " + xd + ", " + xn + ", " + xm + off, Encode::Orr(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("eor " + xd + ", " + xn + ", " + xm + off, Encode::Eor(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("ands " + xd + ", " + xn + ", " + xm + off, Encode::Ands(X(t.d), X(t.n), X(t.m), shift, amount));
    corpus.Add("tst " + xn + ", " + xm + off, Encode::Ands(Xzr, X(t.n), X(t.m), shift, amount));
    if (amount <= 31) {
        corpus.Add("and " + wd + ", " + wn + ", " + wm + off, Encode::And(W(t.d), W(t.n), W(t.m), shift, amount));
        corpus.Add("orr " + wd + ", " + wn + ", " + wm + off, Encode::Orr(W(t.d), W(t.n), W(t.m), shift, amount));
        corpus.Add("eor " + wd + ", " + wn + ", " + wm + off, Encode::Eor(W(t.d), W(t.n), W(t.m), shift, amount));
        corpus.Add("ands " + wd + ", " + wn + ", " + wm + off, Encode::Ands(W(t.d), W(t.n), W(t.m), shift, amount));
        corpus.Add("tst " + wn + ", " + wm + off, Encode::Ands(Wzr, W(t.n), W(t.m), shift, amount));
    }
}

// LSLV/LSRV/ASRV: the register form of the shifts, at both widths. The Triple
// already guarantees m differs from d, so a swapped Rd/Rm field cannot survive.
void EmitShiftRegister(Corpus& corpus, const Triple& t)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n), xm = Xs(t.m);
    const std::string wd = Ws(t.d), wn = Ws(t.n), wm = Ws(t.m);
    corpus.Add("lsl " + xd + ", " + xn + ", " + xm, Encode::Lsl(X(t.d), X(t.n), X(t.m)));
    corpus.Add("lsr " + xd + ", " + xn + ", " + xm, Encode::Lsr(X(t.d), X(t.n), X(t.m)));
    corpus.Add("asr " + xd + ", " + xn + ", " + xm, Encode::Asr(X(t.d), X(t.n), X(t.m)));
    corpus.Add("lsl " + wd + ", " + wn + ", " + wm, Encode::Lsl(W(t.d), W(t.n), W(t.m)));
    corpus.Add("lsr " + wd + ", " + wn + ", " + wm, Encode::Lsr(W(t.d), W(t.n), W(t.m)));
    corpus.Add("asr " + wd + ", " + wn + ", " + wm, Encode::Asr(W(t.d), W(t.n), W(t.m)));
}

// LSL/LSR/ASR (immediate): the aliases of UBFM/SBFM. A 32-bit form only has the
// shifts its width holds, so the wider amounts are skipped there rather than
// clamped.
void EmitShiftImmediate(Corpus& corpus, const Triple& t, uint32_t shift)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n);
    const std::string wd = Ws(t.d), wn = Ws(t.n);
    const std::string s = std::to_string(shift);
    corpus.Add("lsl " + xd + ", " + xn + ", #" + s, Encode::Lsl(X(t.d), X(t.n), shift));
    corpus.Add("lsr " + xd + ", " + xn + ", #" + s, Encode::Lsr(X(t.d), X(t.n), shift));
    corpus.Add("asr " + xd + ", " + xn + ", #" + s, Encode::Asr(X(t.d), X(t.n), shift));
    if (shift <= 31) {
        corpus.Add("lsl " + wd + ", " + wn + ", #" + s, Encode::Lsl(W(t.d), W(t.n), shift));
        corpus.Add("lsr " + wd + ", " + wn + ", #" + s, Encode::Lsr(W(t.d), W(t.n), shift));
        corpus.Add("asr " + wd + ", " + wn + ", #" + s, Encode::Asr(W(t.d), W(t.n), shift));
    }
}

// SBFM/UBFM, spelled with the immediates rather than through an alias, so the
// immr/imms fields themselves are what the oracle checks. Again a 32-bit form
// only takes the pairs its width holds.
void EmitBitfield(Corpus& corpus, const Triple& t, uint32_t immr, uint32_t imms)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n);
    const std::string wd = Ws(t.d), wn = Ws(t.n);
    const std::string i = std::to_string(immr), j = std::to_string(imms);
    corpus.Add("ubfm " + xd + ", " + xn + ", #" + i + ", #" + j, Encode::Ubfm(X(t.d), X(t.n), immr, imms));
    corpus.Add("sbfm " + xd + ", " + xn + ", #" + i + ", #" + j, Encode::Sbfm(X(t.d), X(t.n), immr, imms));
    if (immr <= 31 && imms <= 31) {
        corpus.Add("ubfm " + wd + ", " + wn + ", #" + i + ", #" + j, Encode::Ubfm(W(t.d), W(t.n), immr, imms));
        corpus.Add("sbfm " + wd + ", " + wn + ", #" + i + ", #" + j, Encode::Sbfm(W(t.d), W(t.n), immr, imms));
    }
}

// UBFX/SBFX: the same fields as UBFM/SBFM with imms = lsb + width - 1, written
// as the extraction they mean. An extraction that does not fit the width of the
// 32-bit form is skipped there.
void EmitBitExtract(Corpus& corpus, const Triple& t, uint32_t lsb, uint32_t width)
{
    const std::string xd = Xs(t.d), xn = Xs(t.n);
    const std::string wd = Ws(t.d), wn = Ws(t.n);
    const std::string l = std::to_string(lsb), w = std::to_string(width);
    corpus.Add("ubfx " + xd + ", " + xn + ", #" + l + ", #" + w, Encode::Ubfx(X(t.d), X(t.n), lsb, width));
    corpus.Add("sbfx " + xd + ", " + xn + ", #" + l + ", #" + w, Encode::Sbfx(X(t.d), X(t.n), lsb, width));
    if (lsb + width <= 32) {
        corpus.Add("ubfx " + wd + ", " + wn + ", #" + l + ", #" + w, Encode::Ubfx(W(t.d), W(t.n), lsb, width));
        corpus.Add("sbfx " + wd + ", " + wn + ", #" + l + ", #" + w, Encode::Sbfx(W(t.d), W(t.n), lsb, width));
    }
}

// SXTB/SXTH/SXTW (a 64-bit destination from a 32-bit source) and UXTB/UXTH (a
// 32-bit pair).
void EmitExtend(Corpus& corpus, const Triple& t)
{
    const std::string xd = Xs(t.d), wn = Ws(t.n), wd = Ws(t.d);
    corpus.Add("sxtb " + xd + ", " + wn, Encode::Sxtb(X(t.d), W(t.n)));
    corpus.Add("sxth " + xd + ", " + wn, Encode::Sxth(X(t.d), W(t.n)));
    corpus.Add("sxtw " + xd + ", " + wn, Encode::Sxtw(X(t.d), W(t.n)));
    corpus.Add("uxtb " + wd + ", " + wn, Encode::Uxtb(W(t.d), W(t.n)));
    corpus.Add("uxth " + wd + ", " + wn, Encode::Uxth(W(t.d), W(t.n)));
}

// The register fields of every form above accept the zero register, so each
// position gets a case spelled with it: the pool of real registers cannot reach
// code 31, whose meaning differs between these forms and SP.
void EmitShiftBitfieldZeroRegisters(Corpus& corpus)
{
    corpus.Add("lsl xzr, x1, x2", Encode::Lsl(Xzr, X1, X2));
    corpus.Add("lsl x0, xzr, x2", Encode::Lsl(X0, Xzr, X2));
    corpus.Add("lsl x0, x1, xzr", Encode::Lsl(X0, X1, Xzr));
    corpus.Add("lsr wzr, w1, w2", Encode::Lsr(Wzr, W1, W2));
    corpus.Add("asr w0, wzr, w2", Encode::Asr(W0, Wzr, W2));
    corpus.Add("asr w0, w1, wzr", Encode::Asr(W0, W1, Wzr));
    corpus.Add("lsl xzr, x1, #4", Encode::Lsl(Xzr, X1, 4));
    corpus.Add("lsr x0, xzr, #4", Encode::Lsr(X0, Xzr, 4));
    corpus.Add("lsr w0, wzr, #31", Encode::Lsr(W0, Wzr, 31));
    corpus.Add("ubfm xzr, x1, #0, #7", Encode::Ubfm(Xzr, X1, 0, 7));
    corpus.Add("ubfm x0, xzr, #8, #15", Encode::Ubfm(X0, Xzr, 8, 15));
    corpus.Add("sbfm xzr, xzr, #0, #0", Encode::Sbfm(Xzr, Xzr, 0, 0));
    corpus.Add("ubfm wzr, w1, #0, #7", Encode::Ubfm(Wzr, W1, 0, 7));
    corpus.Add("sbfm w0, wzr, #0, #31", Encode::Sbfm(W0, Wzr, 0, 31));
    corpus.Add("ubfx xzr, x1, #0, #8", Encode::Ubfx(Xzr, X1, 0, 8));
    corpus.Add("ubfx x0, xzr, #8, #8", Encode::Ubfx(X0, Xzr, 8, 8));
    corpus.Add("sbfx w0, wzr, #4, #4", Encode::Sbfx(W0, Wzr, 4, 4));
    corpus.Add("sxtb xzr, w1", Encode::Sxtb(Xzr, W1));
    corpus.Add("sxth x0, wzr", Encode::Sxth(X0, Wzr));
    corpus.Add("sxtw xzr, wzr", Encode::Sxtw(Xzr, Wzr));
    corpus.Add("uxtb w0, wzr", Encode::Uxtb(W0, Wzr));
    corpus.Add("uxth wzr, w1", Encode::Uxth(Wzr, W1));
}

// ---------------------------------------------------------------------------
// Loads and stores
// ---------------------------------------------------------------------------

// The immediate forms name the width of the register they move (a 32-bit form
// reads and writes W registers) and their encoder. The scaled group scales its
// offset field by the size of the accessed element; the unscaled group (LDUR)
// takes the byte offset as it is, so it carries no element size.
struct ScaledForm {
    const char* mnemonic;
    bool word;
    uint32_t elementSize;
    Insn (*encode)(int rt, int base, int32_t offset);
};

struct UnscaledForm {
    const char* mnemonic;
    bool word;
    Insn (*encode)(int rt, int base, int32_t offset);
};

const ScaledForm kScaledForms[] = {
    { "ldr", false, 8, [](int rt, int base, int32_t off) { return Encode::Ldr(X(rt), Mem(X(base), off)); } },
    { "ldr", true, 4, [](int rt, int base, int32_t off) { return Encode::Ldr(W(rt), Mem(X(base), off)); } },
    { "str", false, 8, [](int rt, int base, int32_t off) { return Encode::Str(X(rt), Mem(X(base), off)); } },
    { "str", true, 4, [](int rt, int base, int32_t off) { return Encode::Str(W(rt), Mem(X(base), off)); } },
    { "ldrb", true, 1, [](int rt, int base, int32_t off) { return Encode::Ldrb(W(rt), Mem(X(base), off)); } },
    { "strb", true, 1, [](int rt, int base, int32_t off) { return Encode::Strb(W(rt), Mem(X(base), off)); } },
    { "ldrh", true, 2, [](int rt, int base, int32_t off) { return Encode::Ldrh(W(rt), Mem(X(base), off)); } },
    { "strh", true, 2, [](int rt, int base, int32_t off) { return Encode::Strh(W(rt), Mem(X(base), off)); } },
    { "ldrsb", false, 1, [](int rt, int base, int32_t off) { return Encode::Ldrsb(X(rt), Mem(X(base), off)); } },
    { "ldrsb", true, 1, [](int rt, int base, int32_t off) { return Encode::Ldrsb(W(rt), Mem(X(base), off)); } },
    { "ldrsh", false, 2, [](int rt, int base, int32_t off) { return Encode::Ldrsh(X(rt), Mem(X(base), off)); } },
    { "ldrsh", true, 2, [](int rt, int base, int32_t off) { return Encode::Ldrsh(W(rt), Mem(X(base), off)); } },
    { "ldrsw", false, 4, [](int rt, int base, int32_t off) { return Encode::Ldrsw(X(rt), Mem(X(base), off)); } },
};

const UnscaledForm kUnscaledForms[] = {
    { "ldur", false, [](int rt, int base, int32_t off) { return Encode::Ldur(X(rt), Mem(X(base), off)); } },
    { "ldur", true, [](int rt, int base, int32_t off) { return Encode::Ldur(W(rt), Mem(X(base), off)); } },
    { "stur", false, [](int rt, int base, int32_t off) { return Encode::Stur(X(rt), Mem(X(base), off)); } },
    { "stur", true, [](int rt, int base, int32_t off) { return Encode::Stur(W(rt), Mem(X(base), off)); } },
    { "ldurb", true, [](int rt, int base, int32_t off) { return Encode::Ldurb(W(rt), Mem(X(base), off)); } },
    { "sturb", true, [](int rt, int base, int32_t off) { return Encode::Sturb(W(rt), Mem(X(base), off)); } },
    { "ldurh", true, [](int rt, int base, int32_t off) { return Encode::Ldurh(W(rt), Mem(X(base), off)); } },
    { "sturh", true, [](int rt, int base, int32_t off) { return Encode::Sturh(W(rt), Mem(X(base), off)); } },
    { "ldursb", false, [](int rt, int base, int32_t off) { return Encode::Ldursb(X(rt), Mem(X(base), off)); } },
    { "ldursb", true, [](int rt, int base, int32_t off) { return Encode::Ldursb(W(rt), Mem(X(base), off)); } },
    { "ldursh", false, [](int rt, int base, int32_t off) { return Encode::Ldursh(X(rt), Mem(X(base), off)); } },
    { "ldursh", true, [](int rt, int base, int32_t off) { return Encode::Ldursh(W(rt), Mem(X(base), off)); } },
    { "ldursw", false, [](int rt, int base, int32_t off) { return Encode::Ldursw(X(rt), Mem(X(base), off)); } },
};

std::string LoadStoreText(const char* mnemonic, bool word, int rt, int base, int32_t offset)
{
    return std::string { mnemonic } + " " + (word ? Ws(rt) : Xs(rt)) + ", [" + Xs(base) + ", #" +
           std::to_string(offset) + "]";
}

void EmitLoadStoreScaled(Corpus& corpus, const Triple& t, uint32_t imm12)
{
    for (const ScaledForm& form : kScaledForms) {
        const int32_t offset = static_cast<int32_t>(imm12 * form.elementSize);
        corpus.Add(LoadStoreText(form.mnemonic, form.word, t.d, t.n, offset), form.encode(t.d, t.n, offset));
    }
}

void EmitLoadStoreUnscaled(Corpus& corpus, const Triple& t, int32_t offset)
{
    for (const UnscaledForm& form : kUnscaledForms) {
        corpus.Add(LoadStoreText(form.mnemonic, form.word, t.d, t.n, offset), form.encode(t.d, t.n, offset));
    }
}

// The pair forms read or write both registers from one address, so the second
// register of the pair is the third register of the triple.
struct PairForm {
    const char* mnemonic;
    bool word;
    uint32_t elementSize;
    Insn (*encode)(int rt, int rt2, int base, int32_t offset);
};

const PairForm kPairForms[] = {
    { "ldp", false, 8,
      [](int rt, int rt2, int base, int32_t off) { return Encode::Ldp(X(rt), X(rt2), Mem(X(base), off)); } },
    { "ldp", true, 4,
      [](int rt, int rt2, int base, int32_t off) { return Encode::Ldp(W(rt), W(rt2), Mem(X(base), off)); } },
    { "stp", false, 8,
      [](int rt, int rt2, int base, int32_t off) { return Encode::Stp(X(rt), X(rt2), Mem(X(base), off)); } },
    { "stp", true, 4,
      [](int rt, int rt2, int base, int32_t off) { return Encode::Stp(W(rt), W(rt2), Mem(X(base), off)); } },
    { "ldpsw", false, 4,
      [](int rt, int rt2, int base, int32_t off) { return Encode::Ldpsw(X(rt), X(rt2), Mem(X(base), off)); } },
};

void EmitLoadStorePair(Corpus& corpus, const Triple& t, int32_t imm7)
{
    for (const PairForm& form : kPairForms) {
        const int32_t offset = static_cast<int32_t>(imm7 * form.elementSize);
        corpus.Add(
            std::string { form.mnemonic } + " " + (form.word ? Ws(t.d) : Xs(t.d)) + ", " +
                (form.word ? Ws(t.m) : Xs(t.m)) + ", [" + Xs(t.n) + ", #" + std::to_string(offset) + "]",
            form.encode(t.d, t.m, t.n, offset)
        );
    }
}

// The register-offset forms: the eight ways a 64-bit or a 32-bit index register
// can be read, each in its unscaled and its scaled spelling.
struct IndexShape {
    IndexExtend extend;
    bool scaled;
};

const IndexShape kIndexShapes[] = {
    { IndexExtend::Lsl, false },  { IndexExtend::Lsl, true },  { IndexExtend::Sxtx, false }, { IndexExtend::Sxtx, true },
    { IndexExtend::Uxtw, false }, { IndexExtend::Uxtw, true }, { IndexExtend::Sxtw, false }, { IndexExtend::Sxtw, true },
};

struct IndexForm {
    const char* mnemonic;
    bool word;
    uint32_t elementSize;
    Insn (*encode)(int rt, const MemIndex& mem);
};

const IndexForm kIndexForms[] = {
    { "ldr", false, 8, [](int rt, const MemIndex& mem) { return Encode::Ldr(X(rt), mem); } },
    { "ldr", true, 4, [](int rt, const MemIndex& mem) { return Encode::Ldr(W(rt), mem); } },
    { "str", false, 8, [](int rt, const MemIndex& mem) { return Encode::Str(X(rt), mem); } },
    { "str", true, 4, [](int rt, const MemIndex& mem) { return Encode::Str(W(rt), mem); } },
    { "ldrb", true, 1, [](int rt, const MemIndex& mem) { return Encode::Ldrb(W(rt), mem); } },
    { "strb", true, 1, [](int rt, const MemIndex& mem) { return Encode::Strb(W(rt), mem); } },
    { "ldrh", true, 2, [](int rt, const MemIndex& mem) { return Encode::Ldrh(W(rt), mem); } },
    { "strh", true, 2, [](int rt, const MemIndex& mem) { return Encode::Strh(W(rt), mem); } },
    { "ldrsb", false, 1, [](int rt, const MemIndex& mem) { return Encode::Ldrsb(X(rt), mem); } },
    { "ldrsb", true, 1, [](int rt, const MemIndex& mem) { return Encode::Ldrsb(W(rt), mem); } },
    { "ldrsh", false, 2, [](int rt, const MemIndex& mem) { return Encode::Ldrsh(X(rt), mem); } },
    { "ldrsh", true, 2, [](int rt, const MemIndex& mem) { return Encode::Ldrsh(W(rt), mem); } },
    { "ldrsw", false, 4, [](int rt, const MemIndex& mem) { return Encode::Ldrsw(X(rt), mem); } },
};

// A scaled index is spelled with the shift its access size implies, which is 0
// for a byte access and so is still written out.
uint32_t AccessShift(uint32_t elementSize)
{
    uint32_t shift = 0;
    while ((uint32_t { 1 } << shift) < elementSize) {
        ++shift;
    }
    return shift;
}

std::string IndexText(IndexExtend extend, bool scaled, uint32_t elementSize)
{
    if (extend == IndexExtend::Lsl && !scaled) {
        return {};
    }
    const char* name = extend == IndexExtend::Sxtx ? "sxtx" : (extend == IndexExtend::Lsl ? "lsl" : (extend == IndexExtend::Uxtw ? "uxtw" : "sxtw"));
    return std::string { ", " } + name + (scaled ? " #" + std::to_string(AccessShift(elementSize)) : "");
}

bool IsWordIndex(IndexExtend extend)
{
    return extend == IndexExtend::Uxtw || extend == IndexExtend::Sxtw;
}

template <typename Base, typename Index> MemIndex MemFor(Base base, Index index, IndexExtend extend, bool scaled)
{
    return scaled ? Mem(base, index, extend, Scaled) : Mem(base, index, extend);
}

void EmitLoadStoreIndex(Corpus& corpus, const IndexForm& form, int rt, int base, int index)
{
    for (const IndexShape& shape : kIndexShapes) {
        const bool wordIndex      = IsWordIndex(shape.extend);
        const std::string address = "[" + Xs(base) + ", " + (wordIndex ? Ws(index) : Xs(index)) +
                                    IndexText(shape.extend, shape.scaled, form.elementSize) + "]";
        const MemIndex mem = wordIndex ? MemFor(X(base), W(index), shape.extend, shape.scaled)
                                       : MemFor(X(base), X(index), shape.extend, shape.scaled);
        corpus.Add(
            std::string { form.mnemonic } + " " + (form.word ? Ws(rt) : Xs(rt)) + ", " + address, form.encode(rt, mem)
        );
    }
}

void EmitLoadStoreRegister(Corpus& corpus, const Triple& t)
{
    for (const IndexForm& form : kIndexForms) {
        EmitLoadStoreIndex(corpus, form, t.d, t.n, t.m);
    }
}

// SP as a base, the zero register where a form reads one, and the store-only
// case of a pair naming the same register twice.
void EmitLoadStoreEdgeCases(Corpus& corpus)
{
    corpus.Add("ldr x0, [sp, #8]", Encode::Ldr(X0, Mem(Sp, 8)));
    corpus.Add("ldr x0, [sp, x1, lsl #3]", Encode::Ldr(X0, Mem(Sp, X1, Scaled)));
    corpus.Add("ldr x0, [sp, x1]", Encode::Ldr(X0, Mem(Sp, X1)));
    corpus.Add("ldrsw x0, [sp, xzr, lsl #2]", Encode::Ldrsw(X0, Mem(Sp, Xzr, Scaled)));
    corpus.Add("str w0, [sp, w1, uxtw #2]", Encode::Str(W0, Mem(Sp, W1, IndexExtend::Uxtw, Scaled)));
    corpus.Add("ldur x0, [sp, #-8]", Encode::Ldur(X0, Mem(Sp, -8)));
    corpus.Add("ldp x0, x1, [sp, #-16]", Encode::Ldp(X0, X1, Mem(Sp, -16)));
    corpus.Add("stp xzr, xzr, [sp, #-16]", Encode::Stp(Xzr, Xzr, Mem(Sp, -16)));
    corpus.Add("stp x1, x1, [x2, #16]", Encode::Stp(X1, X1, Mem(X2, 16)));
    corpus.Add("ldp xzr, x0, [sp, #-16]", Encode::Ldp(Xzr, X0, Mem(Sp, -16)));
    corpus.Add("ldp x0, xzr, [sp]", Encode::Ldp(X0, Xzr, Mem(Sp, 0)));
    corpus.Add("ldp w0, wzr, [x1, #8]", Encode::Ldp(W0, Wzr, Mem(X1, 8)));
    corpus.Add("stp w0, wzr, [x1, #8]", Encode::Stp(W0, Wzr, Mem(X1, 8)));
    corpus.Add("ldpsw xzr, x0, [x1, #4]", Encode::Ldpsw(Xzr, X0, Mem(X1, 4)));
    corpus.Add("ldrb wzr, [sp, #3]", Encode::Ldrb(Wzr, Mem(Sp, 3)));
    corpus.Add("strb wzr, [sp, #3]", Encode::Strb(Wzr, Mem(Sp, 3)));
    corpus.Add("ldrh wzr, [sp, #2]", Encode::Ldrh(Wzr, Mem(Sp, 2)));
    corpus.Add("ldrsb xzr, [sp, #1]", Encode::Ldrsb(Xzr, Mem(Sp, 1)));
    corpus.Add("ldrsh wzr, [sp, #2]", Encode::Ldrsh(Wzr, Mem(Sp, 2)));
    corpus.Add("ldrsw xzr, [sp, #4]", Encode::Ldrsw(Xzr, Mem(Sp, 4)));
    corpus.Add("ldurb wzr, [sp, #-1]", Encode::Ldurb(Wzr, Mem(Sp, -1)));
    corpus.Add("sturh wzr, [sp, #-2]", Encode::Sturh(Wzr, Mem(Sp, -2)));
    corpus.Add("ldursb xzr, [sp, #-1]", Encode::Ldursb(Xzr, Mem(Sp, -1)));
    corpus.Add("ldursh xzr, [sp, #-2]", Encode::Ldursh(Xzr, Mem(Sp, -2)));
    corpus.Add("ldursw xzr, [sp, #-4]", Encode::Ldursw(Xzr, Mem(Sp, -4)));
    // The zero register is a legal index register.
    corpus.Add("ldr x0, [x1, xzr]", Encode::Ldr(X0, Mem(X1, Xzr)));
    corpus.Add("ldr x0, [x1, xzr, lsl #3]", Encode::Ldr(X0, Mem(X1, Xzr, Scaled)));
    corpus.Add("ldr x0, [x1, xzr, sxtx]", Encode::Ldr(X0, Mem(X1, Xzr, IndexExtend::Sxtx)));
    corpus.Add("ldr x0, [x1, wzr, uxtw]", Encode::Ldr(X0, Mem(X1, Wzr, IndexExtend::Uxtw)));
    corpus.Add("ldr x0, [x1, wzr, uxtw #3]", Encode::Ldr(X0, Mem(X1, Wzr, IndexExtend::Uxtw, Scaled)));
    corpus.Add("str wzr, [x1, wzr, sxtw]", Encode::Str(Wzr, Mem(X1, Wzr, IndexExtend::Sxtw)));
    corpus.Add("ldrsb x0, [x1, wzr, sxtw #0]", Encode::Ldrsb(X0, Mem(X1, Wzr, IndexExtend::Sxtw, Scaled)));
    corpus.Add("ldr xzr, [x1, x2, sxtx #3]", Encode::Ldr(Xzr, Mem(X1, X2, IndexExtend::Sxtx, Scaled)));
}

void EmitLoadStore(Corpus& corpus, const Pool& pool)
{
    for (const Triple& t : pool.memTriples) {
        for (uint32_t imm12 : pool.imms12) {
            EmitLoadStoreScaled(corpus, t, imm12);
        }
        for (int32_t offset : pool.offsets) {
            EmitLoadStoreUnscaled(corpus, t, offset);
        }
        for (int32_t imm7 : pool.pairImms) {
            EmitLoadStorePair(corpus, t, imm7);
        }
        EmitLoadStoreRegister(corpus, t);
    }
    for (const Triple& t : pool.sampleMemTriples) {
        for (uint32_t imm12 : pool.sampleImms12) {
            EmitLoadStoreScaled(corpus, t, imm12);
        }
        for (int32_t offset : pool.sampleOffsets) {
            EmitLoadStoreUnscaled(corpus, t, offset);
        }
        for (int32_t imm7 : pool.samplePairImms) {
            EmitLoadStorePair(corpus, t, imm7);
        }
        EmitLoadStoreRegister(corpus, t);
    }
    EmitLoadStoreEdgeCases(corpus);
}

void EmitBranches(Corpus& corpus, const Pool& pool)
{
    corpus.Add("ret", Encode::Ret());
    for (int reg : pool.branchRegs) {
        corpus.Add("br " + Xs(reg), Encode::Br(X(reg)));
        corpus.Add("blr " + Xs(reg), Encode::Blr(X(reg)));
        corpus.Add("ret " + Xs(reg), Encode::Ret(X(reg)));
        for (int64_t delta : pool.deltas) {
            const std::string off = OffsetText(delta);
            if (InRange(FixupKind::Branch26, delta)) {
                corpus.Add("b " + off, Encode::ApplyFixup(Encode::B(), FixupKind::Branch26, delta));
            }
            if (InRange(FixupKind::CmpBranch19, delta)) {
                corpus.Add(
                    "cbz " + Xs(reg) + ", " + off,
                    Encode::ApplyFixup(Encode::Cbz(X(reg)), FixupKind::CmpBranch19, delta)
                );
                corpus.Add(
                    "cbnz " + Xs(reg) + ", " + off,
                    Encode::ApplyFixup(Encode::Cbnz(X(reg)), FixupKind::CmpBranch19, delta)
                );
                corpus.Add(
                    "cbz " + Ws(reg) + ", " + off,
                    Encode::ApplyFixup(Encode::Cbz(W(reg)), FixupKind::CmpBranch19, delta)
                );
                corpus.Add(
                    "cbnz " + Ws(reg) + ", " + off,
                    Encode::ApplyFixup(Encode::Cbnz(W(reg)), FixupKind::CmpBranch19, delta)
                );
            }
            if (InRange(FixupKind::Adr21, delta)) {
                corpus.Add(
                    "adr " + Xs(reg) + ", " + off, Encode::ApplyFixup(Encode::Adr(X(reg)), FixupKind::Adr21, delta)
                );
            }
        }
        for (uint32_t bit : pool.bits) {
            corpus.Add(
                "tbz x" + std::to_string(reg) + ", #" + std::to_string(bit) + ", .+4",
                Encode::ApplyFixup(Encode::Tbz(X(reg), bit), FixupKind::TestBranch14, 4)
            );
            corpus.Add(
                "tbnz x" + std::to_string(reg) + ", #" + std::to_string(bit) + ", .+4",
                Encode::ApplyFixup(Encode::Tbnz(X(reg), bit), FixupKind::TestBranch14, 4)
            );
            if (bit <= 31) {
                corpus.Add(
                    "tbz w" + std::to_string(reg) + ", #" + std::to_string(bit) + ", .+4",
                    Encode::ApplyFixup(Encode::Tbz(W(reg), bit), FixupKind::TestBranch14, 4)
                );
                corpus.Add(
                    "tbnz w" + std::to_string(reg) + ", #" + std::to_string(bit) + ", .+4",
                    Encode::ApplyFixup(Encode::Tbnz(W(reg), bit), FixupKind::TestBranch14, 4)
                );
            }
        }
    }
    for (Cc cc : pool.conds) {
        for (int64_t delta : pool.deltas) {
            if (InRange(FixupKind::CondBranch19, delta)) {
                corpus.Add(
                    "b." + std::string(CondName(cc)) + " " + OffsetText(delta),
                    Encode::ApplyFixup(Encode::B(cc), FixupKind::CondBranch19, delta)
                );
            }
        }
    }
}

void EmitAddSub(Corpus& corpus, const Pool& pool)
{
    static const std::pair<const char*, Shift> shifts[] = { { "lsl", Shift::Lsl },
                                                            { "lsr", Shift::Lsr },
                                                            { "asr", Shift::Asr } };
    for (const Triple& t : pool.triples) {
        for (uint32_t imm : pool.imms12) {
            EmitAddSubImmediate(corpus, t, imm);
        }
        for (auto [name, shift] : shifts) {
            for (uint32_t amount : pool.amounts) {
                EmitAddSubShifted(corpus, t, shift, name, amount);
            }
        }
    }
    for (const Triple& t : pool.sampleTriples) {
        for (uint32_t imm : pool.sampleImms12) {
            EmitAddSubImmediate(corpus, t, imm);
        }
        for (auto [name, shift] : shifts) {
            for (uint32_t amount : pool.sampleAmounts) {
                EmitAddSubShifted(corpus, t, shift, name, amount);
            }
        }
    }
}

void EmitLogical(Corpus& corpus, const Pool& pool)
{
    static const std::pair<const char*, Shift> shifts[] = { { "lsl", Shift::Lsl },
                                                            { "lsr", Shift::Lsr },
                                                            { "asr", Shift::Asr } };
    for (const Triple& t : pool.triples) {
        for (uint64_t imm : pool.logicalImms) {
            EmitLogical64(corpus, t, imm);
        }
        for (uint64_t imm : pool.logicalImms32) {
            EmitLogical32(corpus, t, static_cast<uint32_t>(imm));
        }
        for (auto [name, shift] : shifts) {
            for (uint32_t amount : pool.amounts) {
                EmitLogicalShifted(corpus, t, shift, name, amount);
            }
        }
    }
    for (const Triple& t : pool.sampleTriples) {
        for (uint64_t imm : pool.sampleLogical) {
            EmitLogical64(corpus, t, imm);
        }
        for (uint64_t imm : pool.sampleLogical32) {
            EmitLogical32(corpus, t, static_cast<uint32_t>(imm));
        }
        for (auto [name, shift] : shifts) {
            for (uint32_t amount : pool.sampleAmounts) {
                EmitLogicalShifted(corpus, t, shift, name, amount);
            }
        }
    }
}

void EmitShiftBitfield(Corpus& corpus, const Pool& pool)
{
    for (const Triple& t : pool.triples) {
        EmitShiftRegister(corpus, t);
        EmitExtend(corpus, t);
        for (uint32_t shift : pool.amounts) {
            EmitShiftImmediate(corpus, t, shift);
        }
        for (auto [immr, imms] : pool.immPairs) {
            EmitBitfield(corpus, t, immr, imms);
        }
        for (auto [lsb, width] : pool.extracts) {
            EmitBitExtract(corpus, t, lsb, width);
        }
    }
    for (const Triple& t : pool.sampleTriples) {
        EmitShiftRegister(corpus, t);
        EmitExtend(corpus, t);
        for (uint32_t shift : pool.sampleAmounts) {
            EmitShiftImmediate(corpus, t, shift);
        }
        for (auto [immr, imms] : pool.sampleImmPairs) {
            EmitBitfield(corpus, t, immr, imms);
        }
        for (auto [lsb, width] : pool.sampleExtracts) {
            EmitBitExtract(corpus, t, lsb, width);
        }
    }
    EmitShiftBitfieldZeroRegisters(corpus);
}

Corpus BuildCorpus(const std::vector<Pool>& pools)
{
    Corpus corpus;
    for (const Pool& pool : pools) {
        EmitAddSub(corpus, pool);
        EmitLogical(corpus, pool);
        EmitShiftBitfield(corpus, pool);
        EmitLoadStore(corpus, pool);
        EmitBranches(corpus, pool);
    }
    return corpus;
}

// ---------------------------------------------------------------------------
// Differential driver
// ---------------------------------------------------------------------------

class TempDir {
public:
    TempDir()
    {
        std::string templ = (std::filesystem::temp_directory_path() / "asm-encoding-diff.XXXXXX").string();
        std::vector<char> buffer(templ.begin(), templ.end());
        buffer.push_back('\0');
        const char* made = ::mkdtemp(buffer.data());
        path_            = made != nullptr ? made : std::string {};
    }

    ~TempDir()
    {
        if (!preserve_ && !path_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }
    }

    const std::string& Path() const { return path_; }

    void Preserve() { preserve_ = true; }

private:
    std::string path_;
    bool preserve_ = false;
};

std::vector<uint32_t> ReadWords(const std::string& path, bool& ok)
{
    std::ifstream in(path, std::ios::binary);
    ok = static_cast<bool>(in);
    std::vector<uint32_t> words;
    if (!ok) {
        return words;
    }
    unsigned char bytes[4];
    while (in.read(reinterpret_cast<char*>(bytes), 4)) {
        words.push_back(
            static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
            (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24)
        );
    }
    return words;
}

void RunDifferential(const Corpus& corpus, const Oracle& oracle, size_t reportLimit)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty()) << "could not create a temporary directory";
    const std::string sourcePath = dir.Path() + "/corpus.s";
    const std::string objectPath = dir.Path() + "/corpus.o";
    const std::string binaryPath = dir.Path() + "/corpus.bin";
    const std::string logPath    = dir.Path() + "/log";

    {
        std::ofstream source(sourcePath, std::ios::binary);
        ASSERT_TRUE(source) << "could not write " << sourcePath;
        for (const Case& item : corpus.Cases()) {
            source << item.text << '\n';
        }
    }

    const std::string logRedirect = " > " + Quote(logPath) + " 2>&1";
    const int assembleStatus =
        Run(Quote(oracle.mc) + " -triple=aarch64 -filetype=obj " + Quote(sourcePath) + " -o " + Quote(objectPath) +
            logRedirect);
    ASSERT_EQ(assembleStatus, 0) << "llvm-mc rejected the corpus:\n" << Head(ReadAll(logPath), 4000);
    const int extractStatus =
        Run(Quote(oracle.objcopy) + " -O binary --only-section=.text " + Quote(objectPath) + " " + Quote(binaryPath) +
            logRedirect);
    ASSERT_EQ(extractStatus, 0) << "llvm-objcopy failed:\n" << Head(ReadAll(logPath), 4000);

    bool readOk                             = false;
    const std::vector<uint32_t> oracleWords = ReadWords(binaryPath, readOk);
    ASSERT_TRUE(readOk) << "could not read " << binaryPath;
    ASSERT_EQ(oracleWords.size(), corpus.Size())
        << "the oracle emitted " << oracleWords.size() << " words for " << corpus.Size()
        << " cases: .text holds data this harness did not emit";

    size_t mismatches = 0;
    for (size_t i = 0; i < oracleWords.size(); ++i) {
        const Case& item = corpus.Cases()[i];
        if (item.word == oracleWords[i]) {
            continue;
        }
        ++mismatches;
        if (mismatches <= reportLimit) {
            ADD_FAILURE() << "encoding mismatch at case " << i << ": " << item.text << "\n  ours:   " << Hex(item.word)
                          << "\n  oracle: " << Hex(oracleWords[i]) << "\n  xor:    " << Hex(item.word ^ oracleWords[i]);
        }
    }
    if (mismatches != 0) {
        dir.Preserve();
        ADD_FAILURE() << mismatches << " of " << corpus.Size() << " cases mismatch; corpus kept at " << dir.Path();
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

Oracle ReadyOracle()
{
    Oracle oracle = FindOracle();
    if (!oracle.Available()) {
        return Oracle {};
    }
    std::string version;
    if (!SupportsAarch64(oracle, version)) {
        return Oracle {};
    }
    return oracle;
}

// A randomly parameterized sweep, `iterations` times, drawn from the same form
// table. Kept separate from the deterministic corpora so a failure there is
// always reproducible.
Corpus BuildRandomCorpus(
    size_t iterations, const std::vector<uint64_t>& logical64, const std::vector<uint64_t>& logical32
)
{
    std::mt19937 rng(0x5eed1234u);
    Corpus corpus;
    for (size_t i = 0; i < iterations; ++i) {
        Corpus slice = BuildCorpus({ RandomPool(rng, logical64, logical32) });
        for (const Case& item : slice.Cases()) {
            corpus.AddWord(item.text, item.word);
        }
    }
    return corpus;
}

TEST(EncodingDiff, Systematic)
{
    const Oracle oracle = ReadyOracle();
    if (!oracle.Available()) {
        GTEST_SKIP() << "no aarch64-capable llvm-mc/llvm-objcopy (set ASM_ORACLE_LLVM_MC / "
                        "ASM_ORACLE_LLVM_OBJCOPY, or run in the devshell)";
    }
    const Corpus corpus = BuildCorpus({ BoundedPool() });
    RecordProperty("cases", static_cast<int>(corpus.Size()));
    ASSERT_LE(corpus.Size(), kSystematicCap) << "the bounded sweep outgrew its cap";
    RunDifferential(corpus, oracle, 20);
}

TEST(EncodingDiff, Exhaustive)
{
    if (std::getenv("ASM_ENCODING_DIFF_FULL") == nullptr) {
        GTEST_SKIP() << "set ASM_ENCODING_DIFF_FULL=1 to sweep the full operand space";
    }
    const Oracle oracle = ReadyOracle();
    if (!oracle.Available()) {
        GTEST_SKIP() << "no aarch64-capable llvm-mc/llvm-objcopy (set ASM_ORACLE_LLVM_MC / "
                        "ASM_ORACLE_LLVM_OBJCOPY, or run in the devshell)";
    }
    const Pool pool         = ExhaustivePool();
    Corpus corpus           = BuildCorpus({ pool });
    size_t randomIterations = 0;
    if (const char* iters = std::getenv("ASM_FUZZ_ITERS"); iters != nullptr && *iters != '\0') {
        randomIterations = static_cast<size_t>(std::strtoul(iters, nullptr, 10));
    }
    if (randomIterations != 0) {
        Corpus random = BuildRandomCorpus(randomIterations, pool.logicalImms, pool.logicalImms32);
        for (const Case& item : random.Cases()) {
            corpus.AddWord(item.text, item.word);
        }
    }
    RecordProperty("cases", static_cast<int>(corpus.Size()));
    ASSERT_LE(corpus.Size(), kExhaustiveCap) << "the exhaustive sweep outgrew its cap";
    RunDifferential(corpus, oracle, 20);
}

} // namespace
