#!/usr/bin/env python3
"""Generates .asm test resources for the cbc-engine tests.

Usage:
    python3 asm-generator.py <generator-name>

Each named generator overwrites resource files in test/resources/generated-asm/<testsuite-name>/. 
"""

import argparse
import sys
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path

ENGINE_ROOT = Path(__file__).resolve().parent.parent.parent
GENERATED_ASM = ENGINE_ROOT / "test" / "resources" / "generated-asm"


def write_suite(suite: str, files: dict[str, str]) -> list[Path]:
    """Writes the generated files of a test suite into
    test/resources/generated-asm/<suite>/ and returns the written paths."""
    directory = GENERATED_ASM / suite
    directory.mkdir(parents=True, exist_ok=True)
    written = []
    for file_name, source in files.items():
        path = directory / file_name
        path.write_text(source)
        written.append(path)
    return written


# ---------------------------------------------------------------------------
# asm DSL: the only place that knows the indentation, method wrappers and literal formats
# ---------------------------------------------------------------------------

class AsmType(StrEnum):
    """A type-tag operand of a typed op (F64 in `i2f U32, F64, IR1, FR0`)."""

    F32 = "F32"
    F64 = "F64"
    I8 = "I8"
    I16 = "I16"
    I32 = "I32"
    I64 = "I64"
    U8 = "U8"
    U16 = "U16"
    U32 = "U32"
    U64 = "U64"

    @property
    def is_float(self) -> bool:
        return self.startswith("F")

    @property
    def kind(self) -> str:
        if self.is_float:
            return "f"
        return "i"


# The members, unpacked so the case tables below can name types directly.
F32, F64, I8, I16, I32, I64, U8, U16, U32, U64 = AsmType

# The registers used by the generated tests.
IR1 = "IR1"
FR0 = "FR0"
FR1 = "FR1"
IRZ = "IRZ"


def reg_for(t: AsmType) -> str:
    if t.is_float:
        return FR0
    return IR1


def imm(value: int) -> str:
    return format(value, "#x")


def live(*regs: str) -> str:
    return "@live.prim " + ", ".join(regs)


def live_ref(*regs: str) -> str:
    return "@live.ref " + ", ".join(regs)


def instr(mnemonic: str, *operands: object, w: int | None = None,
          suffixes: tuple[object, ...] = ()) -> str:
    """Renders one instruction line: `mnemonic[.s1…] op, op, …`.

    `w` is the common single width suffix; `suffixes` covers the rest
    (e.g. bfxs.{dst}.{src}). Operands are registers ("IR1"), imm(…)
    literals or AsmType tags.
    """
    assert w is None or not suffixes
    if w is not None:
        tail = f".{w}"
    else:
        tail = "".join(f".{s}" for s in suffixes)
    ops = ", ".join(str(o) for o in operands)
    if ops:
        return f"{mnemonic}{tail} {ops}"
    return f"{mnemonic}{tail}"


@dataclass(frozen=True)
class Method:
    name: str
    ret: AsmType
    body: tuple[str, ...]
    params: tuple[AsmType, ...] = ()
    directives: tuple[str, ...] = ()

    def render(self) -> str:
        lines = "\n".join(f"      {line}" for line in self.body)
        params = ", ".join(self.params)
        directives = "".join(f"    {d}\n" for d in self.directives)
        return (f"  @method {self.name}({params}){self.ret}\n"
                f"{directives}"
                f"    @code\n{lines}\n"
                f"    @end\n  @end")


ASM_FILE_HEADER = '@main_type "default"\n\n@type default\n'


def asm_file(methods: list[Method]) -> str:    
    return ASM_FILE_HEADER + "\n\n".join(m.render() for m in methods) + "\n@end\n"


def asm_file_with_preamble(preamble: str, methods: list[Method]) -> str:
    """A file whose header carries extra top-level definitions (field refs,
    record type declarations) before the `@type default` method block."""
    return '@main_type "default"\n' + preamble + '\n@type default\n\n' + "\n\n".join(
        m.render() for m in methods
    ) + "\n@end\n"


class MethodBuilder:

    def __init__(self, name: str, ret: AsmType, params: tuple[AsmType, ...] = (),
                 default_w: int | None = None):
        self._name = name
        self._ret = ret
        self._params = params
        self._lines: list[str] = []
        self._directives: list[str] = []
        self._default_w = default_w

    def directive(self, text: str) -> "MethodBuilder":
        """Adds a method-level directive line rendered between the
        `@method` header and `@code` (e.g. `@typed_slots ...`)."""
        self._directives.append(text)
        return self

    def live(self, *regs: str) -> "MethodBuilder":
        self._lines.append(live(*regs))
        return self

    def live_ref(self, *regs: str) -> "MethodBuilder":
        self._lines.append(live_ref(*regs))
        return self

    def dead(self, *regs: str) -> "MethodBuilder":
        self._lines.append("@dead " + ", ".join(regs))
        return self

    def label(self, name: str) -> "MethodBuilder":
        self._lines.append(f"{name}:")
        return self

    def nops(self, count: int) -> "MethodBuilder":
        self._lines.extend(["nop"] * count)
        return self

    def raw(self, line: str) -> "MethodBuilder":
        self._lines.append(line)
        return self

    def instr(self, mnemonic: str, *operands: object,
              suffixes: tuple[object, ...] = (), w: int | None = None) -> "MethodBuilder":
        if w is None:
            w = self._default_w
        if suffixes:
            w = None
        self._lines.append(instr(mnemonic, *operands, w=w, suffixes=suffixes))
        return self

    def plain(self, mnemonic: str, *operands: object) -> "MethodBuilder":
        """Appends a line without the default width suffix: mnemonics that
        encode the width in the register kind (ld.typed/st.typed.f) and
        analyzer directives (@dead)."""
        self._lines.append(instr(mnemonic, *operands, w=None))
        return self

    def _close(self, mnemonic: str, reg: str) -> Method:
        w = int(self._ret[1:])
        return Method(self._name, self._ret, tuple(self._lines) + (instr(mnemonic, reg, w=w),),
                      self._params, tuple(self._directives))

    def ret(self, reg: str = IR1) -> Method:
        return self._close("ret", reg)

    def fret(self, reg: str = FR0) -> Method:
        return self._close("fret", reg)

    def build(self) -> Method:
        """Closes the method as-is, for bodies with several rets or none."""
        return Method(self._name, self._ret, tuple(self._lines), self._params)


def method(name: str, ret: AsmType, params: tuple[AsmType, ...] = (),
           default_w: int | None = None) -> MethodBuilder:
    return MethodBuilder(name, ret, params, default_w)


def integer_method(name: str, width: int = 64) -> MethodBuilder:
    ret = AsmType(f"I{width}")
    return (
        MethodBuilder(name, ret, params=(ret,), default_w=width)
        .live(IR1)
    )


# ---------------------------------------------------------------------------
# arith: CbcSpecializedArith suite
# ---------------------------------------------------------------------------

# The image set must stay in sync with rhsValues in test/cbc_test.cpp: the
# test builds the file name with its hex() helper, which formats exactly
# like imm() below.
ARITH_RHS_VALUES = [
    0x1,
    0x10,
    0x100,
    0x1020,
    0x10000,
    0x100200,
    0x1000020,
    0x7000000000000000,
    0x7000000010000001,
    0xF000100000000001,
    0xF000000100000001,
]

ARITH_OPERATIONS = [
    "Add",
    "Sub",
    "Mul",
    "And",
    "Or",
    "Xor",
    "UDiv",
    "Div",
    "Rem",
    "URem",
    "LSL",
    "LSR",
    "ASR",
]


def arith_method(op: str, rhs: int) -> Method:
    """Builds one arith case: `<op>i IR1, IR1, <imm(rhs)>` on the live IR1."""
    return (
        integer_method(f"test_{imm(rhs)}")
        .instr(op.lower() + "i", IR1, IR1, imm(rhs))
        .ret()
    )


def generate_arith() -> list[Path]:
    written = []
    for op in ARITH_OPERATIONS:
        written += write_suite(
            f"arith/{op}",
            {
                f"{imm(v)}.asm": asm_file([arith_method(op, v)]) for v in ARITH_RHS_VALUES
            },
        )
    return written


# ---------------------------------------------------------------------------
# float-arith: CbcSpecializedFloatOps/CbcSpecializedDoubleOps and SimpleArithFloatMov32/Mov64
# ---------------------------------------------------------------------------

FLOAT_UNARY_OPS = ["NEG", "SQRT", "ABS"]
FLOAT_BINARY_OPS = ["ADD", "SUB", "MUL", "DIV"]


def float_ops_method(name: str, width: int) -> Method:
    float_type = AsmType(f"F{width}")
    if name in FLOAT_UNARY_OPS:
        params = (float_type,)
        operands = (FR0,)
    else:
        params = (float_type, float_type)
        operands = (FR0, FR1)
    return (
        method(f"test_{name}", float_type, params=params, default_w=width)
        .instr(f"f{name.lower()}", FR0, *operands)
        .fret()
    )


def float_mov_methods(width: int) -> list[Method]:
    """mov (FR1 -> FR0), i2f (IR1 -> FR0) and f2i (FR0 -> IR1)"""
    float_type = AsmType(f"F{width}")
    int_type = AsmType(f"I{width}")
    return [
        method("test_MOV", float_type, params=(float_type, float_type), default_w=width)
          .instr("fmov", FR0, FR1)
          .fret(),

        method("test_I2F", float_type, params=(int_type,), default_w=width)
          .live(IR1)
          .instr("movi2f", FR0, IR1)
          .fret(),

        method("test_F2I", int_type, params=(float_type,), default_w=width)
          .instr("movf2i", IR1, FR0)
          .ret(),
    ]


def generate_float_arith() -> list[Path]:
    written = []
    for width in (32, 64):
        mov, i2f, f2i = float_mov_methods(width)
        files = {
            "MOV.asm": asm_file([mov]),
            "I2F.asm": asm_file([i2f]),
            "F2I.asm": asm_file([f2i]),
        }
        for name in FLOAT_UNARY_OPS + FLOAT_BINARY_OPS:
            files[f"{name}.asm"] = asm_file([float_ops_method(name, width)])
        written += write_suite(f"float-arith/{width}", files)
    return written


# ---------------------------------------------------------------------------
# cast: CbcSpecializedCast
# ---------------------------------------------------------------------------

# Each case becomes one {first}_{second}.asm test
CAST_TO_FLOAT_CASES = [
    (F32, F64),
    (F32, I32),
    (F32, I64),
    (F32, U32),
    (F32, U64),
    (F64, F32),
    (F64, I32),
    (F64, I64),
    (F64, U32),
    (F64, U64),
]

CAST_TO_INTEGER_CASES = [
    (U8, F32),
    (U8, F64),
    (U16, F32),
    (U16, F64),
    (I8, F32),
    (I8, F64),
    (I16, F32),
    (I16, F64),
    (I16, I32),
    (I16, U32),
    (I32, F32),
    (I32, F64),
    (I32, I64),
    (I32, U64),
    (I64, F32),
    (I64, F64),
    (I64, I32),
    (I64, U32),
    (I8, I32),
    (I8, U32),
    (U16, I32),
    (U16, U32),
    (U32, F32),
    (U32, F64),
    (U32, U64),
    (U64, F32),
    (U64, F64),
    (U8, I32),
    (U8, U32),
]

CAST_OPS = {
  ("f", "f"): "f2f", 
  ("f", "i"): "i2f", 
  ("i", "f"): "f2i", 
  ("i", "i"): "i2i"
}

def cast_method(case: tuple[AsmType, AsmType], *, to_float: bool) -> Method:
    """`to_float` tests always keep IR1 live; integer ones mark IR1
    live only for int-to-int cases."""
    first, second = case
    op = CAST_OPS[(first.kind, second.kind)]
    ret = first if to_float else I64
    built = method(f"test_{first}_{second}", ret, params=(second,))
    if to_float or op == "i2i":
        built = built.live(IR1)
    built = built.instr(op, second, first, reg_for(second), reg_for(first))
    if to_float:
        return built.fret()
    return built.ret()


def generate_cast() -> list[Path]:
    written = write_suite(
        "cast/to_float",
        {
           f"{first}_{second}.asm": asm_file([cast_method((first, second), to_float=True)])
                                    for first, second in CAST_TO_FLOAT_CASES
        },
    )
    written += write_suite(
        "cast/to_integer",
        {
           f"{first}_{second}.asm": asm_file([cast_method((first, second), to_float=False)])
                                    for first, second in CAST_TO_INTEGER_CASES
        },
    )
    return written


# ---------------------------------------------------------------------------
# bfx
# ---------------------------------------------------------------------------

BFX_WIDTHS = [32, 64]


def bfx_method(sign: str, dst: int, src: int) -> Method:
    if sign == "signed":
        mnemonic = "bfxs"
    else:
        mnemonic = "bfxz"
    return (
        integer_method(f"test_{sign}_{dst}_{src}")
        .instr(mnemonic, IR1, IR1, 0, 32, suffixes=(dst, src))
        .ret()
    )


def generate_bfx() -> list[Path]:
    return write_suite(
        "bfx",
        {
            f"{sign}_{dst}_{src}.asm": asm_file([bfx_method(sign, dst, src)])
                                       for sign in ("signed", "zeroed")
                                       for dst in BFX_WIDTHS
                                       for src in BFX_WIDTHS
        },
    )


# ---------------------------------------------------------------------------
# arith-specialized: ArithSpecialized1/ArithSpecialized2
# ---------------------------------------------------------------------------

ARITH_CHAINS = {
    "1": [
        ("addi", 0x30000),
        ("addi", -0x30001),
        ("muli", 0x30000),
        ("divi", 0x30000),
        ("muli", -0x40000),
        ("divi", -0x40000),
        ("muli", 0x4000000),
        ("udivi", 0x4000000),
    ],
    "2": [
        ("xori", 0xFFFFFF),
        ("xori", 0xFF),
        ("xori", 0xFF0000),
        ("xori", 0x7000000000000000),
    ],
}


def arith_chain_method(case: str, ops: list[tuple[str, int]]) -> Method:
    builder = integer_method(f"test_{case}")
    for mnemonic, value in ops:
        builder.instr(mnemonic, IR1, IR1, imm(value))
    return builder.ret()


def generate_arith_specialized() -> list[Path]:
    return write_suite(
        "arith-specialized",
        {
            f"{case}.asm": asm_file([arith_chain_method(case, ops)]) for case, ops in ARITH_CHAINS.items()
        },
    )


# frame-slots: typed frame slots (ld.typed/st.typed(.f), ld.stack.rec)
# ---------------------------------------------------------------------------

# One record type serves all the cases: `x` is its 8-byte field at offset 0,
# `f` is the 8-byte float field at offset 8.
FRAME_SLOTS_PREAMBLE = """
@field_ref rec_x = default:Rec@rec x I64
@field_ref rec_f = default:Rec@rec f F64

@type default:Rec
  @flags PUBLIC RECORD
  @field x I64
    @flags PUBLIC
  @end
  @field f F64
    @flags PUBLIC
  @end
@end
"""

REC_TYPE = "default:Rec@rec"


def typed_i64_method() -> Method:
    """Round-trips the I64 param through field `x` of the typed slot 0."""
    return (
        method("test_typed_i64", I64, params=(I64,), default_w=64)
        .directive(f"@typed_slots {REC_TYPE}")
        .live(IR1)
        .plain("st.typed", IR1, "$0", "#rec_x")
        .plain("@dead", IR1)
        .plain("ld.typed", IR1, "$0", "#rec_x")
        .ret()
    )


def typed_f64_method() -> Method:
    """Round-trips the F64 param through field `f` of the typed slot 0.
    Float registers need no live/dead markers (see the float-arith suite)."""
    return (
        method("test_typed_f64", F64, params=(F64,), default_w=64)
        .directive(f"@typed_slots {REC_TYPE}")
        .plain("st.typed.f", FR0, "$0", "#rec_f")
        .plain("ld.typed.f", FR0, "$0", "#rec_f")
        .fret()
    )


def typed_two_slots_method() -> Method:
    """Round-trips both I64 params through field `x` of typed slots 0 and 1."""
    return (
        method("test_typed_two", I64, params=(I64, I64), default_w=64)
        .directive(f"@typed_slots {REC_TYPE}, {REC_TYPE}")
        .live(IR1, "IR2")
        .plain("st.typed", IR1, "$0", "#rec_x")
        .plain("st.typed", "IR2", "$1", "#rec_x")
        .plain("@dead", f"{IR1}, IR2")
        .plain("ld.typed", IR1, "$0", "#rec_x")
        .plain("ld.typed", "IR2", "$1", "#rec_x")
        .instr("add", IR1, IR1, "IR2")
        .ret()
    )


def stack_rec_method() -> Method:
    """Loads the address of typed slot 0 with ld.stack.rec and accesses the
    record through it via plain field ops."""
    return (
        method("test_stack_rec", I64, params=(I64,), default_w=64)
        .directive(f"@typed_slots {REC_TYPE}")
        .live(IR1)
        .plain("ld.stack.rec", "IR2", "$0")
        .plain("st.field", IR1, "IR2", "#rec_x")
        .plain("@dead", IR1)
        .plain("ld.field", IR1, "IR2", "#rec_x")
        .ret()
    )


def generate_frame_slots() -> list[Path]:
    return write_suite(
        "frame-slots",
        {
            "typed_i64.asm": asm_file_with_preamble(FRAME_SLOTS_PREAMBLE, [typed_i64_method()]),
            "typed_f64.asm": asm_file_with_preamble(FRAME_SLOTS_PREAMBLE, [typed_f64_method()]),
            "typed_two.asm": asm_file_with_preamble(FRAME_SLOTS_PREAMBLE, [typed_two_slots_method()]),
            "stack_rec.asm": asm_file_with_preamble(FRAME_SLOTS_PREAMBLE, [stack_rec_method()]),
        },
    )


# ---------------------------------------------------------------------------
# sat-arith: saturating SLSL with negative bases
# ---------------------------------------------------------------------------

# Negative bases exercise the unsigned-wide shift in SatArith (a signed left
# shift of a negative value is UB); the shift of width - 1 also drives the
# makeS clamping. Shifts must stay in sync with the `shifts` array in
# CbcTest.SatSlslNegativeBase (test/cbc_test.cpp).
SAT_SLSL_WIDTHS  = [8, 16, 32, 64]
SAT_SLSL_SHIFTS  = [1, 2, 7, 15, 31, 63]


def slsl_neg_method(width: int, shift: int) -> Method:
    """Builds one slsl case: `slsli.<w> IR1, IR1, <shift>` on the incoming
    IR1 param (the base). Narrow widths have no ret.8/ret.16, so only the
    w64 case returns I64."""
    ret = I64 if width == 64 else I32
    return (
        method(f"test_slsl_{width}_{shift}", ret, params=(I64,), default_w=width)
        .live(IR1)
        .instr("slsli", IR1, IR1, shift)
        .ret()
    )


def generate_sat_arith() -> list[Path]:
    return write_suite(
        "sat-arith",
        {
            f"slsl_{width}_{shift}.asm": asm_file([slsl_neg_method(width, shift)])
                                          for width in SAT_SLSL_WIDTHS
                                          for shift in SAT_SLSL_SHIFTS
                                          if shift <= width
        },
    )


# ---------------------------------------------------------------------------
# arr-ic: array index check (arr.ic) lowered through the rewriter
# ---------------------------------------------------------------------------

# IR1 carries a reference to a fake array with the layout
# [TI*][u64 length][elements...] (see CbcTest.ArrIndexCheck); IR2 carries the
# index. The method loads the length with arr.len, checks the index with
# arr.ic (mnemonic order: index first, length second) and returns the index
# when the check passes. The same bytecode serves the throwing cases; the
# index is returned straight from IR2 because IR1 stays a reference (the
# liveness analyzer forbids redefining it as prim).
def arr_ic_method() -> Method:
    return (
        method("test_arr_ic", I64, params=(I64, I64), default_w=64)
        .live_ref(IR1)
        .live("IR2")
        .plain("arr.len", "IR3", IR1)
        .plain("arr.ic", "IR2", "IR3")
        .ret("IR2")
    )


def generate_arr_ic() -> list[Path]:
    return write_suite("arr-ic", {"arr_ic.asm": asm_file([arr_ic_method()])})


# ---------------------------------------------------------------------------
# checked-arith: CbcSpecializedCheckedArith suite
# ---------------------------------------------------------------------------

# Width boundary helpers; values are kept as unsigned bit patterns.
def smax(w: int) -> int:
    return (1 << (w - 1)) - 1


def smin(w: int) -> int:
    return 1 << (w - 1)


def umax(w: int) -> int:
    return (1 << w) - 1


# Immediates outside the signed 12-bit range select the wide CBINI*W encoding.
# The engine emitter keeps W8 immediates within [-128, 255] (all fit into 12
# bits), so there is no wide-imm case for that width.
def wide_neg(w: int) -> int | None:
    return {8: None, 16: 0x8000, 32: 0x80000000, 64: 0xFFFFFFFFFFFFFFFF}[w]


CHECKED_RR_OPS = [
    "cadd",
    "csub",
    "cmul",
    "cdiv",
    "clsl",
    "clsr",
    "casr",
    "cpow",
    "cuadd",
    "cusub",
    "cumul",
]

# The rhs file sets must stay in sync with CheckedArithParamsList in
# test/cbc_test.cpp: the test builds the file name with its hex() helper,
# which formats exactly like imm() below. cpow.32/cpowi.32 exist in the
# assembler DSL since upstream 854529c2 (probed: rr assembles, and cpowi.32
# selects the 12-bit CBINI32I encoding for small immediates and the wide
# CBINI32W one for values beyond the signed 12-bit range — unlike W8, whose
# emitter keeps immediates within [-128, 255], making CBINI8W unreachable).
# The imm operand is written as the unsigned bit pattern of the value.
CHECKED_RR_WIDTHS = {op: [8, 16, 32, 64] for op in CHECKED_RR_OPS}

CHECKED_IMM_WIDTHS = {op: [8, 16, 32, 64] for op in [
    "caddi",
    "csubi",
    "cmuli",
    "cdivi",
    "cuaddi",
    "cusubi",
    "cumuli",
    "clsli",
    "clsri",
    "casri",
    "cpowi",
    # The saturating imm forms (upstream 854529c2, probed: all four mnemonics
    # assemble at widths 8/16/32/64 with the `op.<w> IRd, IRl, imm` operand
    # order and negative spellings). They share the SBINIMM{8,16,32,64}
    # opcode families with the other saturating imm ops (short 12-bit sleb
    # encoding, wide SBINIMM*W re-encoding by the engine rewriter for values
    # beyond the signed 12-bit range); the interpreter delivers the immediate
    # as a plain u64 primitive, so the semantics match the rr forms exactly.
    # Saturating ops clamp instead of raising, so only the ok/ flavor exists.
    "sdivi",
    "smodi",
    "spowi",
    "sulsri",
]}

# cpowi overflow exponents per width: the width itself (base 2/3 overflow the
# width's signed type) and the positive boundary exponent (= smax, whose
# squaring overflows immediately; operations.h CPOW squares base between
# result multiplications).
CPOWI_OF_RHS = {
    8 : [0x8, 0x7F],
    16: [0x10, 0x7FFF],
    32: [0x20, 0x7FFFFFFF],
    64: [0x40, 0x7FFFFFFFFFFFFFFF],
}


def checked_imm_rhs(op: str, w: int) -> dict[str, list[int]]:
    hi = smax(w)
    unsigned = umax(w)
    neg = wide_neg(w)
    shifts = [w] + ([neg] if neg is not None else [])
    # cdivi: only divisors reaching the CDIV boundaries matter (the ok/of file
    # split below mirrors the test's lhs tables in cbc_test.cpp):
    #   ok  — 0x1/0x2 basic division and truncation toward zero, unsigned-1
    #         (i.e. -2) negative-divisor truncation, hi (= smax) positive
    #         boundary divisor, unsigned (= -1) negation divisor;
    #   of  — 0x0 → ArithmeticException for any lhs, unsigned (= -1) with
    #         lhs = smin → DivOverflowException (INT_MIN / -1).
    # unsigned appears in both flavors: same divisor value, files live in the
    # ok/ and of/ dirs side by side (the test picks the dir per lhs). All
    # values stay inside the per-width imm ranges, so at W8 everything fits
    # the [-128, 255] emitter bound; the W16/32/64 entries beyond the signed
    # 12-bit range select the wide CBINI*W encodings (W8 has no wide-imm case).
    #
    # sdivi/smodi/spowi/sulsri (saturating) never raise, so they carry only
    # the "ok" key — the mirroring lhs tables live in CheckedArithParamsList
    # (test/cbc_test.cpp). Div/Mod immediates exclude 0x0 (the interpreter
    # asserts on a zero divisor, operations.h:142/151). The W16/32/64 entries
    # beyond the signed 12-bit range select the wide SBINIMM*W encodings
    # (0x...ff spellings are negative values, delivered as u64 bit patterns
    # and truncated/sign-extended by the width's sget/uget).
    table = {
        "caddi" : lambda: {"ok": [0x1, 0x2, 0x10], "of": [hi]},
        "csubi" : lambda: {"ok": [0x1, 0x2, 0x10], "of": [0x1, 0x10, hi]},
        "cmuli" : lambda: {"ok": [0x2, 0x3], "of": [hi]},
        "cdivi" : lambda: {"ok": [0x1, 0x2, unsigned - 1, hi, unsigned], "of": [0x0, unsigned]},
        "cuaddi": lambda: {"ok": [0x1, 0x2, 0x10], "of": [unsigned]},
        "cusubi": lambda: {"ok": [0x1, 0x2], "of": [0x1, 0x2]},
        "cumuli": lambda: {"ok": [0x2, 0x3], "of": [unsigned]},
        "clsli" : lambda: {"ok": [0x0, 0x1, 0x3], "of": shifts},
        "clsri" : lambda: {"ok": [0x0, 0x1, 0x3], "of": shifts},
        "casri" : lambda: {"ok": [0x0, 0x1, 0x3], "of": shifts},
        "cpowi" : lambda: {"ok": [0x2, 0x3], "of": CPOWI_OF_RHS[w]},
        # sdivi/smodi: 0x1 identity and x % 1 = 0, 0x2 positive divisor,
        # unsigned-1 (= -2) negative divisor, hi (= smax) positive boundary
        # divisor (also the wide encoding), unsigned (= -1) negation divisor
        # (smin/-1 saturates to smax; smin % -1 → 0).
        # spowi: the exponent is read unsigned (uget), so every imm bit
        # pattern is a valid exponent — 0x0 (0^0 = 1), 0x1 (identity), 0x2/0x3
        # (squaring/cubing with negative bases, both saturation signs), the
        # width itself (2^w saturates), unsigned (= -1, a huge odd exponent;
        # also the wide encoding at W16/32/64).
        # sulsri: the shift is read unsigned and clamped to [0, w-1] — 0x0
        # identity, 0x1/0x3 basic shifts, w-1 (last meaningful shift), w
        # (first clamped shift), unsigned (= -1: a huge shift that clamps to
        # w-1; at W64 the u64 bit pattern casts to a negative int64 and
        # clamps to 0).
        "sdivi" : lambda: {"ok": [0x1, 0x2, unsigned - 1, hi, unsigned]},
        "smodi" : lambda: {"ok": [0x1, 0x2, unsigned - 1, hi, unsigned]},
        "spowi" : lambda: {"ok": [0x0, 0x1, 0x2, 0x3, w, unsigned]},
        "sulsri": lambda: {"ok": [0x0, 0x1, 0x3, w - 1, w, unsigned]},
    }
    return table[op]()


def checked_imm_method(op: str, w: int, rhs: int) -> Method:
    return (
        method(f"test_{imm(rhs)}", I64, params=(I64,), default_w=w)
        .live(IR1)
        .instr(op, IR1, IR1, imm(rhs))
        .ret()
    )


def checked_rr_method(op: str, w: int) -> Method:
    return (
        method(f"test_{op}_{w}", I64, params=(I64, I64), default_w=w)
        .live(IR1, IR2)
        .instr(op, IR1, IR1, IR2)
        .ret()
    )


def generate_checked_arith() -> list[Path]:
    written = []
    for op, widths in CHECKED_IMM_WIDTHS.items():
        for w in widths:
            for flavor, values in checked_imm_rhs(op, w).items():
                written += write_suite(
                    f"checked-arith/{op}-{w}-{flavor}",
                    {f"{imm(v)}.asm": asm_file([checked_imm_method(op, w, v)]) for v in values},
                )
    for op in CHECKED_RR_OPS:
        for w in CHECKED_RR_WIDTHS[op]:
            written += write_suite(
                f"checked-arith/{op}-{w}-rr",
                {"rr.asm": asm_file([checked_rr_method(op, w)])},
            )
    return written


# ---------------------------------------------------------------------------
# sat-arith: CbcSpecializedSatArith suite
# ---------------------------------------------------------------------------

# The imm rhs file sets must stay in sync with SatImmRhs in test/cbc_test.cpp:
# the test builds the file name with its hex() helper, which formats exactly
# like imm() below. Values are unsigned bit patterns; the signed ops truncate
# them to the width (0xFF == -1 for W8, 0xFFFF == -1 for W16, ...).
# Immediates outside the signed 12-bit range select the wide SBINIMM*W encoding;
# the engine emitter keeps W8 immediates within [-128, 255] (all inside 12 bits),
# so SBINIMM8W has no test case.
SAT_IMM_RHS = {
    8:  [0x0, 0x1, 0x2, 0x7F, 0x80, 0xFF],
    16: [0x0, 0x1, 0x2, 0x7FF, 0x1000, 0x7FFF, 0x8000, 0xFFFF],
    32: [0x0, 0x1, 0x2, 0x7FF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF],
    64: [0x0, 0x1, 0x2, 0x7FF, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0xFFFFFFFFFFFFFFFF],
}

SAT_IMM_OPS = ["saddi", "ssubi", "smuli", "slsli", "sasri", "suaddi", "susubi", "sumuli"]

# The DSL exposes no imm mnemonics for sdiv/smod/spow, and the unsigned shifts
# sulsl/sulsr exist only in the register form (all widths, upstream
# NewAsmParser.scala:1091-1100); sneg/suneg are unary (ssub/susub against IRZ)
# with no imm form.
SAT_RR_WIDTHS = {op: [8, 16, 32, 64] for op in [
    "sadd", "ssub", "smul", "sdiv", "smod", "spow", "slsl", "sasr", "sneg",
    "suadd", "susub", "sumul", "sudiv", "sumod", "suneg", "sulsl", "sulsr",
]}

SAT_UNARY_OPS = ["sneg", "suneg"]


def sat_imm_method(op: str, w: int, rhs: int) -> Method:
    return (
        method(f"test_{imm(rhs)}", I64, params=(I64,), default_w=w)
        .live(IR1)
        .instr(op, IR1, IR1, imm(rhs))
        .ret()
    )


def sat_rr_method(op: str, w: int) -> Method:
    if op in SAT_UNARY_OPS:
        return (
            method(f"test_{op}_{w}", I64, params=(I64,), default_w=w)
            .live(IR1)
            .instr(op, IR1, IR1)
            .ret()
        )
    return (
        method(f"test_{op}_{w}", I64, params=(I64, I64), default_w=w)
        .live(IR1, IR2)
        .instr(op, IR1, IR1, IR2)
        .ret()
    )


def generate_sat_arith() -> list[Path]:
    written = []
    for op in SAT_IMM_OPS:
        for w in (8, 16, 32, 64):
            written += write_suite(
                f"sat-arith/{op}-{w}",
                {f"{imm(v)}.asm": asm_file([sat_imm_method(op, w, v)]) for v in SAT_IMM_RHS[w]},
            )
    for op, widths in SAT_RR_WIDTHS.items():
        for w in widths:
            written += write_suite(
                f"sat-arith/{op}-{w}-rr",
                {"rr.asm": asm_file([sat_rr_method(op, w)])},
            )
    return written


# ---------------------------------------------------------------------------
# convert-ext: CbcSpecializedConvertExt suite
# ---------------------------------------------------------------------------

INT_TYPES = [I8, U8, I16, U16, I32, U32, I64, U64]

# i2i.u emits the same CONVERT as i2i: the destination type tag carries the
# zero-extension. The interpreter's CastPrim supports only I32/U32/I64/U64
# sources, and zero-extension is observable only for unsigned destinations,
# which leaves these combos.
I2I_U_CASES = [
    (U8, I32),
    (U8, U32),
    (U16, I32),
    (U16, U32),
    (U32, U64),
]

# ci2i/ci2i.u accept the full integer matrix, cf2i adds the float sources.
# Checked converts into float destinations (ci2f/cf2f) are rejected: the
# interpreter asserts an integral destination (casts.h CheckedCastPrim).
CHECKED_CONVERT_CASES = [(dst, src) for dst in INT_TYPES for src in INT_TYPES] + [
    (dst, src) for dst in INT_TYPES for src in (F32, F64)
]


def i2i_u_method(case: tuple[AsmType, AsmType]) -> Method:
    dst, src = case
    return (
        method(f"test_{dst}_{src}", I64, params=(src,))
        .live(IR1)
        .instr("i2i.u", src, dst, IR1, IR1)
        .ret()
    )


def checked_convert_method(case: tuple[AsmType, AsmType]) -> Method:
    dst, src = case
    if src.is_float:
        return (
            method(f"test_{dst}_{src}", I64, params=(src,))
            .instr("cf2i", src, dst, FR0, IR1)
            .ret()
        )
    mnemonic = "ci2i.u" if dst.startswith("U") or src.startswith("U") else "ci2i"
    return (
        method(f"test_{dst}_{src}", I64, params=(src,))
        .live(IR1)
        .instr(mnemonic, src, dst, IR1, IR1)
        .ret()
    )


def generate_convert_ext() -> list[Path]:
    written = write_suite(
        "convert-ext/i2i-u",
        {f"{dst}_{src}.asm": asm_file([i2i_u_method((dst, src))]) for dst, src in I2I_U_CASES},
    )
    written += write_suite(
        "convert-ext/checked",
        {f"{dst}_{src}.asm": asm_file([checked_convert_method((dst, src))]) for dst, src in CHECKED_CONVERT_CASES},
    )
    return written


# ---------------------------------------------------------------------------
# cmp-branch: CbcSpecializedCmpBranch/CbcSpecializedFloatCmpBranch suites
# ---------------------------------------------------------------------------

# The CC spellings double as file names and method-name parts; they must stay
# in sync with CmpCCName/FloatCmpCCName in test/cbc_test.cpp. The float CCs
# apply to fscc/fbcc at both widths. REQ/RNE read raw 64-bit register cells
# (Compare<CC::REQ/RNE> is specialized for Value::Reference at W64 only,
# src/interpreter/internal/operations.h:699-707; the generic Value::Reference
# form FATALs, operations.h:693-697), so the rr scc/bcc cases are generated at
# W64 only. The imm forms (scci/bcci) cannot carry REQ/RNE at any width: the
# interpreter's SCCImm/BccImm dispatch through CmpPrim, whose CC switch has no
# REQ/RNE cases and FATALs "Unreachable" (src/interpreter/interpreter.h:477-490,
# 560-565, probed).
CMP_INT_CCS = ["EQ", "NE", "LT", "GE", "ULT", "UGE", "TESTZ", "TESTNZ"]
CMP_REF_CCS = ["REQ", "RNE"]
CMP_FLOAT_CCS = ["FEQ", "FNE", "FGE", "FNGE", "FLT", "FNLT"]

# The imm file sets must stay in sync with CmpImmPatterns/CmpBccImmPatterns in
# test/cbc_test.cpp: file names carry the unsigned bit pattern of the
# immediate, the instruction spells it signed (0xffffffff at W32 is written
# -0x1). Negative immediates are generated for both scci and bcci: the
# isa_parser SccImm entry point widens its immediate argument through an
# explicit cast since 8049baee, while the bcci entry decodes it through the
# uint64_t Decoder::ReadSLEB (src/cbc/isa_parser.cpp), so the UBSan build
# accepts negative values for both. scci/bcci exist at W32/W64 only (the DSL
# has no .8/.16 spellings, NewAsmParser.scala, and ForkedAssembler.sccCommon
# asserts W32/W64; probed). A bcci with imm 0 lowers to the register-vs-IRZ
# rr form (ForkedAssembler.doBccImm), so the 0x0 files keep testing the rr
# path. Immediates outside the signed 12-bit range select the literal-encoded
# SCCI*L/BCCL*I RT forms (Emitter::SCCImm/BccImmFixup); they are generated for
# the EQ CC only, positive and negative. The negative literal patterns are
# width-natural (the W32 spellings are the low 32 bits of the W64 patterns):
# smin has a matching lhs bound in test/cbc_test.cpp, so the EQ-taken path is
# covered too.
CMP_IMM_PATTERNS = {
    32: [0x0, 0x1, 0x2, 0x7ff, 0xFFFFFFFF, 0xFFFFF800],
    64: [0x0, 0x1, 0x2, 0x7ff, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFF800],
}
CMP_BCC_IMM_PATTERNS = {
    32: [0x0, 0x1, 0x2, 0x7ff, 0xFFFFFFFF, 0xFFFFF800],
    64: [0x0, 0x1, 0x2, 0x7ff, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFF800],
}
CMP_LITERAL_IMMS = [0x800, 0x10000000]
CMP_NEG_LITERAL_IMMS = {
    32: [0x80000000, 0xF0000000],
    64: [0x8000000000000000, 0xFFFFFFFFF0000000],
}
# The far (2050-nop padded) bcci form: the branch delta moves into the literal
# table (BCCI*L) while the immediate stays in-instruction. Generated for the
# negative short immediates only (the near forms above cover the positive
# ones); -1 has a matching lhs bound, so both the taken and the skipped paths
# are exercised.
CMP_FAR_BCC_IMM_PATTERNS = {
    32: [0xFFFFFFFF],
    64: [0xFFFFFFFFFFFFFFFF],
}

# Branch deltas beyond 12 bits move the offset into the literal table
# (BCC*L/BCCI*L); the nop pad forces such a delta.
CMP_FAR_PAD_NOPS = 2050


def cmp_signed_imm(pattern: int, w: int) -> int:
    return pattern - (1 << w) if pattern >> (w - 1) else pattern


def cmp_scc_method(cc: str, w: int) -> Method:
    t = AsmType(f"I{w}")
    return (
        method(f"test_{cc}", I64, params=(t, t), default_w=w)
        .live(IR1, IR2)
        .instr("scc", cc, IR1, IR1, IR2)
        .ret()
    )


def cmp_fscc_method(cc: str, w: int) -> Method:
    t = AsmType(f"F{w}")
    return (
        method(f"test_{cc}", I64, params=(t, t), default_w=w)
        .live(IR1)
        .instr("fscc", cc, IR1, FR0, FR1)
        .ret()
    )


def cmp_scc_imm_method(cc: str, w: int, pattern: int) -> Method:
    t = AsmType(f"I{w}")
    return (
        method(f"test_{imm(pattern)}", I64, params=(t,), default_w=w)
        .live(IR1)
        .instr("scci", cc, IR1, IR1, imm(cmp_signed_imm(pattern, w)))
        .ret()
    )


def cmp_bcc_method(cc: str, w: int, ref: bool) -> Method:
    t = AsmType(f"I{w}")
    builder = method(f"test_{cc}", I64, params=(t, t), default_w=w)
    builder = builder.live_ref(IR1, IR2) if ref else builder.live(IR1, IR2)
    return (
        builder.instr("movi", IR3, imm(1))
        .instr("bcc", cc, IR1, IR2, "skip")
        .instr("subi", IR3, IR3, imm(1))
        .label("skip")
        .instr("ret", IR3, w=64)
        .build()
    )


def cmp_bcc_imm_method(cc: str, w: int, pattern: int) -> Method:
    t = AsmType(f"I{w}")
    return (
        method(f"test_{imm(pattern)}", I64, params=(t,), default_w=w)
        .live(IR1)
        .instr("movi", IR3, imm(1))
        .instr("bcci", cc, IR1, imm(cmp_signed_imm(pattern, w)), "skip")
        .instr("subi", IR3, IR3, imm(1))
        .label("skip")
        .instr("ret", IR3, w=64)
        .build()
    )


def cmp_bcc_imm_far_method(cc: str, w: int, pattern: int) -> Method:
    # The 2050-nop pad pushes the branch delta past the signed 12-bit field,
    # so the offset moves into the literal table (BCCI*L) while the immediate
    # stays in-instruction. Same ok/fail contract as cmp_bcc_imm_method (the
    # parametrized test body compares against 1/0): IR3 reads 1 when the
    # branch is taken, 0 when skipped.
    t = AsmType(f"I{w}")
    return (
        method(f"test_{imm(pattern)}", I64, params=(t,), default_w=w)
        .live(IR1)
        .instr("movi", IR3, imm(1))
        .instr("bcci", cc, IR1, imm(cmp_signed_imm(pattern, w)), "far")
        .instr("subi", IR3, IR3, imm(1))
        .instr("ret", IR3, w=64)
        .dead(IR3)
        .nops(CMP_FAR_PAD_NOPS)
        .label("far")
        .instr("movi", IR3, imm(1))
        .instr("ret", IR3, w=64)
        .build()
    )


def cmp_fbcc_method(cc: str, w: int) -> Method:
    t = AsmType(f"F{w}")
    return (
        method(f"test_{cc}", I64, params=(t, t), default_w=w)
        .instr("movi", IR1, imm(1))
        .instr("fbcc", cc, FR0, FR1, "skip")
        .instr("subi", IR1, IR1, imm(1))
        .label("skip")
        .instr("ret", IR1, w=64)
        .build()
    )


def cmp_jmp_methods() -> dict[str, Method]:
    forward = (
        method("test_forward", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("movi", IR2, imm(1))
        .raw("jmp skip")
        .instr("movi", IR3, imm(7))
        .instr("ret", IR3, w=64)
        .label("skip")
        .instr("ret", IR2, w=64)
        .build()
    )
    loop = (
        method("test_loop", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("movi", IR2, imm(0))
        .label("loop")
        .instr("add", IR2, IR2, IR1)
        .instr("subi", IR1, IR1, imm(1))
        .instr("bcc", "NE", IR1, IRZ, "loop")
        .dead(IR1)
        .instr("ret", IR2, w=64)
        .build()
    )
    loop_jmp = (
        method("test_loop_jmp", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("movi", IR2, imm(0))
        .label("loop")
        .instr("add", IR2, IR2, IR1)
        .instr("subi", IR1, IR1, imm(1))
        .instr("bcc", "EQ", IR1, IRZ, "done")
        .raw("jmp loop")
        .label("done")
        .instr("ret", IR2, w=64)
        .build()
    )
    return {"forward.asm": forward, "loop.asm": loop, "loop-jmp.asm": loop_jmp}


def cmp_wide_methods() -> dict[str, Method]:
    def wbcc(w: int, cc: str) -> Method:
        t = AsmType(f"I{w}")
        return (
            method(f"test_wbcc{w}", I64, params=(t, t), default_w=w)
            .live(IR1, IR2)
            .instr("movi", IR3, imm(1))
            .instr("w.bcc", cc, IR1, IR2, "skip")
            .instr("subi", IR3, IR3, imm(1))
            .label("skip")
            .instr("ret", IR3, w=64)
            .build()
        )

    def wbcci(w: int, cc: str) -> Method:
        t = AsmType(f"I{w}")
        return (
            method(f"test_wbcci{w}", I64, params=(t,), default_w=w)
            .live(IR1)
            .instr("movi", IR3, imm(1))
            .instr("w.bcci", cc, IR1, imm(0x7FF), "skip")
            .instr("subi", IR3, IR3, imm(1))
            .label("skip")
            .instr("ret", IR3, w=64)
            .build()
        )

    def wfbcc(w: int, cc: str) -> Method:
        t = AsmType(f"F{w}")
        return (
            method(f"test_wfbcc{w}", I64, params=(t, t), default_w=w)
            .instr("movi", IR1, imm(1))
            .instr("w.fbcc", cc, FR0, FR1, "skip")
            .instr("subi", IR1, IR1, imm(1))
            .label("skip")
            .instr("ret", IR1, w=64)
            .build()
        )

    wjmp = (
        method("test_wjmp", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("movi", IR2, imm(1))
        .raw("w.jmp skip")
        .instr("movi", IR3, imm(7))
        .instr("ret", IR3, w=64)
        .label("skip")
        .instr("ret", IR2, w=64)
        .build()
    )
    return {
        "w-bcc-32.asm": wbcc(32, "EQ"),
        "w-bcc-64.asm": wbcc(64, "GE"),
        "w-bcci-64.asm": wbcci(64, "LT"),
        "w-fbcc-32.asm": wfbcc(32, "FEQ"),
        "w-fbcc-64.asm": wfbcc(64, "FNGE"),
        "w-jmp.asm": wjmp,
    }


def cmp_far_methods() -> dict[str, Method]:
    def far_bcc(w: int) -> Method:
        t = AsmType(f"I{w}")
        return (
            method(f"test_bcc_far_{w}", I64, params=(t, t), default_w=w)
            .live(IR1, IR2)
            .instr("movi", IR3, imm(1))
            .instr("bcc", "EQ", IR1, IR2, "far")
            .instr("subi", IR3, IR3, imm(1))
            .instr("ret", IR3, w=64)
            .dead(IR3)
            .nops(CMP_FAR_PAD_NOPS)
            .label("far")
            .instr("movi", IR3, imm(2))
            .instr("ret", IR3, w=64)
            .build()
        )

    def far_bcci(name: str, w: int, pattern: int = 1) -> Method:
        return (
            method(name, I64, params=(AsmType(f"I{w}"),), default_w=w)
            .live(IR1)
            .instr("movi", IR3, imm(1))
            .instr("bcci", "EQ", IR1, imm(cmp_signed_imm(pattern, w)), "far")
            .instr("subi", IR3, IR3, imm(1))
            .instr("ret", IR3, w=64)
            .dead(IR3)
            .nops(CMP_FAR_PAD_NOPS)
            .label("far")
            .instr("movi", IR3, imm(2))
            .instr("ret", IR3, w=64)
            .build()
        )

    return {
        "bcc-32.asm": far_bcc(32),
        "bcc-64.asm": far_bcc(64),
        "bcci-32.asm": far_bcci("test_bcci_far_32", 32),
        "bcci-64.asm": far_bcci("test_bcci_far", 64),
        # Literal negative immediate + literal offset: the BCCL*L RT form.
        "bcci-neg-lit-32.asm": far_bcci("test_bcci_neg_lit_far_32", 32, CMP_NEG_LITERAL_IMMS[32][1]),
        "bcci-neg-lit-64.asm": far_bcci("test_bcci_neg_lit_far", 64, CMP_NEG_LITERAL_IMMS[64][1]),
    }


def generate_cmp_branch() -> list[Path]:
    written = []
    for w in (32, 64):
        written += write_suite(
            f"cmp-branch/scc-{w}",
            {f"{cc}.asm": asm_file([cmp_scc_method(cc, w)]) for cc in CMP_INT_CCS},
        )
        written += write_suite(
            f"cmp-branch/bcc-{w}",
            {f"{cc}.asm": asm_file([cmp_bcc_method(cc, w, ref=False)]) for cc in CMP_INT_CCS},
        )
        for cc in CMP_INT_CCS:
            patterns = list(CMP_IMM_PATTERNS[w])
            if cc == "EQ":
                patterns += CMP_LITERAL_IMMS + CMP_NEG_LITERAL_IMMS[w]
            written += write_suite(
                f"cmp-branch/scci-{w}/{cc}",
                {f"{imm(p)}.asm": asm_file([cmp_scc_imm_method(cc, w, p)]) for p in patterns},
            )
            bcc_patterns = list(CMP_BCC_IMM_PATTERNS[w])
            if cc == "EQ":
                bcc_patterns += CMP_LITERAL_IMMS + CMP_NEG_LITERAL_IMMS[w]
            written += write_suite(
                f"cmp-branch/bcci-{w}/{cc}",
                {f"{imm(p)}.asm": asm_file([cmp_bcc_imm_method(cc, w, p)]) for p in bcc_patterns},
            )
            written += write_suite(
                f"cmp-branch/far-bcci-{w}/{cc}",
                {f"{imm(p)}.asm": asm_file([cmp_bcc_imm_far_method(cc, w, p)]) for p in CMP_FAR_BCC_IMM_PATTERNS[w]},
            )
        written += write_suite(
            f"cmp-branch/fscc-{w}",
            {f"{cc}.asm": asm_file([cmp_fscc_method(cc, w)]) for cc in CMP_FLOAT_CCS},
        )
        written += write_suite(
            f"cmp-branch/fbcc-{w}",
            {f"{cc}.asm": asm_file([cmp_fbcc_method(cc, w)]) for cc in CMP_FLOAT_CCS},
        )
    for cc in CMP_REF_CCS:
        written += write_suite("cmp-branch/scc-64", {f"{cc}.asm": asm_file([cmp_scc_method(cc, 64)])})
        written += write_suite("cmp-branch/bcc-64", {f"{cc}.asm": asm_file([cmp_bcc_method(cc, 64, ref=True)])})
    written += write_suite("cmp-branch/jmp", {name: asm_file([m]) for name, m in cmp_jmp_methods().items()})
    written += write_suite("cmp-branch/wide", {name: asm_file([m]) for name, m in cmp_wide_methods().items()})
    written += write_suite("cmp-branch/far", {name: asm_file([m]) for name, m in cmp_far_methods().items()})
    return written


# ---------------------------------------------------------------------------

GENERATORS = {
    "arith"            : generate_arith,
    "float-arith"      : generate_float_arith,
    "cast"             : generate_cast,
    "bfx"              : generate_bfx,
    "arith-specialized": generate_arith_specialized,
    "frame-slots"      : generate_frame_slots,
    "sat-arith"        : generate_sat_arith,
    "arr-ic"           : generate_arr_ic,
    "checked-arith"    : generate_checked_arith,
    "sat-arith"        : generate_sat_arith,
    "convert-ext"      : generate_convert_ext,
    "cmp-branch"       : generate_cmp_branch,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("names", nargs="+", choices=sorted(GENERATORS), metavar="generator")
    args = parser.parse_args()

    for name in args.names:
        print(f"Generating {name}:")
        for path in GENERATORS[name]():
            print(f"  {path.relative_to(ENGINE_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
