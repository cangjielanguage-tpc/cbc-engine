#!/usr/bin/env python3
"""Generates .asm test resources for the cbc-engine tests.

Usage:
    python3 asm-generator.py <generator-name>

Each named generator overwrites resource files in test/resources/generated-asm/<testsuite-name>/. 
"""

import argparse
import sys
from dataclasses import dataclass
from decimal import Decimal
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
IR2 = "IR2"
IR3 = "IR3"
IR4 = "IR4"
IR5 = "IR5"
IR6 = "IR6"
IR7 = "IR7"
IR8 = "IR8"
IR9 = "IR9"
IR10 = "IR10"
FR0 = "FR0"
FR1 = "FR1"
IRZ = "IRZ"

FR2 = "FR2"
FR3 = "FR3"
FR4 = "FR4"
FR5 = "FR5"
FR6 = "FR6"
FR7 = "FR7"
FR8 = "FR8"
FR9 = "FR9"


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
    untyped_count: int = 0
    saved_iregs: tuple[str, ...] = ()

    def render(self) -> str:
        lines = "\n".join(f"      {line}" for line in self.body)
        params = ", ".join(self.params)
        directives = "".join(f"    {d}\n" for d in self.directives)
        untyped = f"    @untyped_count {imm(self.untyped_count)}\n" if self.untyped_count else ""
        saved = f"    @saved_iregs {', '.join(self.saved_iregs)}\n" if self.saved_iregs else ""
        return (f"  @method {self.name}({params}){self.ret}\n"
                f"{directives}"
                f"{untyped}"
                f"{saved}"
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
        self._untyped_count = 0
        self._saved_iregs: tuple[str, ...] = ()

    def slots(self, count: int) -> "MethodBuilder":
        """Declares `count` untyped frame slots (@untyped_count)."""
        self._untyped_count = count
        return self

    def directive(self, text: str) -> "MethodBuilder":
        """Adds a method-level directive line rendered between the
        `@method` header and `@code` (e.g. `@typed_slots ...`)."""
        self._directives.append(text)
        return self

    def saved_iregs(self, *regs: str) -> "MethodBuilder":
        """Declares caller-saved-on-entry non-volatile registers (@saved_iregs)."""
        self._saved_iregs = regs
        return self

    def live(self, *regs: str) -> "MethodBuilder":
        self._lines.append(live(*regs))
        return self

    def live_ref(self, *regs: str) -> "MethodBuilder":
        self._lines.append(live_ref(*regs))
        return self

    def live_rec(self, *regs: str) -> "MethodBuilder":
        self._lines.append("@live.rec " + ", ".join(regs))
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
                      self._params, tuple(self._directives), self._untyped_count, self._saved_iregs)

    def ret(self, reg: str = IR1) -> Method:
        return self._close("ret", reg)

    def fret(self, reg: str = FR0) -> Method:
        return self._close("fret", reg)

    def build(self) -> Method:
        """Closes the method as-is, for bodies with several rets or none."""
        return Method(self._name, self._ret, tuple(self._lines), self._params, tuple(self._directives), self._untyped_count, self._saved_iregs)


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
# alu-rr: CbcSpecializedAluRr suite
# ---------------------------------------------------------------------------

# The neg methods are unary (neg.32 IR1, IR1); the assembler lowers neg to
# sub against the IRZ zero register, so the interpreter sees Common::SUB.
ALU_RR_OPS = [
    "add", "sub", "mul", "div", "rem", "udiv", "urem",
    "and", "or", "xor", "lsl", "lsr", "asr", "pow", "neg",
]

ALU_UNARY_OPS = ["neg"]

# 0x7ff is the last immediate fitting the signed 12-bit BINI*I encoding;
# 0x800 selects the literal BINI*L form.
ALU_POWI_IMMS = [0x0, 0x1, 0x2, 0x3, 0x7FF, 0x800]


def alu_rr_method(op: str, w: int) -> Method:
    if op in ALU_UNARY_OPS:
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


def alu_powi_method(w: int, rhs: int) -> Method:
    return (
        method(f"test_{imm(rhs)}", I64, params=(I64,), default_w=w)
        .live(IR1)
        .instr("powi", IR1, IR1, imm(rhs))
        .ret()
    )


def alu_movi_methods() -> dict[str, Method]:
    """W32 cases driving BIN32 from a MOVI.32-loaded register."""
    add = (
        method("test_add", I64, params=(I64,), default_w=32)
        .live(IR1)
        .instr("movi", IR2, imm(0x7))
        .instr("add", IR1, IR1, IR2)
        .ret()
    )
    neg = (
        method("test_neg", I64, params=(I64,), default_w=32)
        .live(IR1)
        .instr("movi", IR2, imm(0x0))
        .instr("sub", IR1, IR2, IR1)
        .ret()
    )
    return {"add.asm": add, "neg.asm": neg}


def generate_alu_rr() -> list[Path]:
    written = []
    for w in (32, 64):
        for op in ALU_RR_OPS:
            written += write_suite(
                f"alu-rr/{op}-{w}-rr",
                {"rr.asm": asm_file([alu_rr_method(op, w)])},
            )
        written += write_suite(
            f"alu-rr/powi-{w}",
            {f"{imm(v)}.asm": asm_file([alu_powi_method(w, v)]) for v in ALU_POWI_IMMS},
        )
    written += write_suite("alu-rr/movi-32", {name: asm_file([m]) for name, m in alu_movi_methods().items()})
    return written


# ---------------------------------------------------------------------------
# frame-slots: CbcSpecializedFrameSlots suite (rt round-trips, uslot
# immediate/byte stores, long-form offsets)
# ---------------------------------------------------------------------------

# Each case stores its argument into untyped slot $0 and loads it back with
# the kind pair (st_kind, ld_kind): the narrow loads read a full
# st.uslot.64 cell, st16/st32 exercise the narrow STORE_FRAME kinds, and
# ref/f32/f64 round-trip through the same-width store/load pair. The
# file/method names must stay in sync with FrameSlotCases in test/cbc_test.cpp.
FRAME_SLOT_RT_CASES = [
    ("u8", "64", "u8"),
    ("s8", "64", "s8"),
    ("u16", "64", "u16"),
    ("s16", "64", "s16"),
    ("32", "64", "32"),
    ("64", "64", "64"),
    ("st16", "16", "u16"),
    ("st32", "32", "32"),
    ("ref", "ref", "ref"),
]

FRAME_SLOT_FLOAT_KINDS = ["f32", "f64"]

# The long-form methods use 260 slots: slot $0 lands 2080 bytes below the
# frame start, outside the signed 12-bit offset window (so the access takes
# LOAD_LONG_FRAME/STORE_LONG_FRAME), while slot $255 (-40) stays inside it.
FRAME_SLOT_LONG_SLOT_COUNT = 260

# Probed and rejected DSL forms (assembler crashes / interpreter FATALs, so
# their RT opcodes stay uncovered): ld.uslot.u32 assembles but the interpreter
# LoadPrim FATALs on LD_U32; ld.stack.rec assembles and rewrites (its
# @typed_slots rec type needs the @flags RECORD modifier), but TypedSlotOffset
# (isa_rewriter.cpp) computes `(int32_t)u32 - uint32` in unsigned arithmetic,
# and UBSan with halt_on_error aborts on the wrapping conversion for every
# negative typed offset (LOAD_LONG_FRAME LD_LEA stays uncovered; the mem-obj
# suite re-probed the same abort for ld.typed/st.typed); ld.stack.rec lowers to
# LoadFrame, so LOAD_LONG_REC/STORE_LONG_REC are only reachable via big-offset
# ld.raw/st.raw (covered by the mem-obj suite); lea.stack.param does not exist.

# st.uslot.imm (fixed upstream by 5081927e) and st.uslot.8 (fixed by f8b1ae17)
# are covered by the cases below. Probe notes:
# - st.uslot.imm takes a 64-bit signed integer literal (decimal or 0x hex,
#   negative too; the lexer folds '-' into the IntegerLit), encodes it as
#   sleb128, and performs NO liveness-analyzer calls. The slot therefore must
#   be brought into the analyzer state with `@live.prim $N` before the first
#   load from it: the load's trans() reads the slot's current mark and aborts
#   with NoSuchElementException on an unregistered slot (probed).
# - st.uslot.8 takes an ireg like every other st.uslot kind (f8b1ae17 fixed
#   the freg spelling); the store itself registers the slot, so plain
#   `@live.prim IR1` is enough there.
# The rewriter lowers st.uslot.imm through ST_64 unconditionally
# (isa_rewriter.cpp StoreUntypedImm), and StoreFrameImm (emitter.cpp) picks the
# RT opcode purely by the immediate magnitude: [-2^7, 2^7-1] -> STORE_FRAME_IMM_1,
# then IMM_2 at 16 bits, IMM_4 at 32 bits, IMM_8 otherwise. The test values
# below pin the class boundaries; a wrong opcode choice would truncate the
# immediate at encode/decode and fail the round-trip.
# (file, value name, imm literal, stored 64-bit pattern, ld.uslot read-back kinds)
IMM_SLOT_CASES = [
    ("imm1", "m1", "-1", 0xFFFFFFFFFFFFFFFF, ("64", "32", "s8", "u8")),
    ("imm1", "p127", "127", 0x7F, ("64",)),
    ("imm2", "p128", "128", 0x80, ("64",)),
    ("imm2", "m4660", "-0x1234", 0xFFFFFFFFFFFFEDCC, ("64", "32", "s16", "u16", "s8", "u8")),
    ("imm4", "p32768", "32768", 0x8000, ("64",)),
    ("imm4", "m305419896", "-0x12345678", 0xFFFFFFFFEDCBA988, ("64", "32", "s16", "u16", "s8", "u8")),
    ("imm8", "p2147483648", "2147483648", 0x80000000, ("64",)),
    ("imm8", "pattern", "0x123456789ABCDEF0", 0x123456789ABCDEF0, ("64", "32", "s8", "u8")),
    ("imm8", "smin", "-9223372036854775808", 0x8000000000000000, ("64",)),
]

# st.uslot.8 truncates the source register to the low byte (ST_8); the
# read-back kinds distinguish the load extensions: ld.uslot.s8 sign-extends,
# ld.uslot.u8 zero-extends, ld.uslot.64 shows the untouched upper slot bytes.
FRAME_SLOT_ST8_READS = ["64", "s8", "u8"]


def frame_imm_method(width_file: str, value_name: str, imm: str, read: str) -> Method:
    return (
        method(f"test_{width_file}_{value_name}_{read}", I64).slots(1)
        .live("$0")
        .instr("st.uslot.imm", imm, "$0")
        .instr(f"ld.uslot.{read}", IR1, "$0")
        .dead("$0")
        .ret(IR1)
    )


def frame_st8_methods() -> list[Method]:
    return [
        (
            method(f"test_st8_{read}", I64, params=(I64,)).slots(1)
            .live(IR1)
            .instr("st.uslot.8", IR1, "$0")
            .instr(f"ld.uslot.{read}", IR2, "$0")
            .dead(IR1, "$0")
            .ret(IR2)
        )
        for read in FRAME_SLOT_ST8_READS
    ]


# ---------------------------------------------------------------------------
# frame-slots: typed narrow fields (I8/U8/I16/U16) and F32 (st.typed/ld.typed(.f))
# ---------------------------------------------------------------------------

# The rewriter lowers ld.typed/st.typed through LOAD_FRAME/STORE_FRAME with
# the field-kind access kinds (isa_rewriter.cpp:913 LdTyped / :1002 StTyped ->
# isa_rewriter.cpp:54 Ldk / :79 Stk): I8 -> LD_S8 (sign-extending), U8 ->
# LD_U8 (zero-extending), I16 -> LD_S16, U16 -> LD_U16, F32 -> LD_F32; the
# store kinds collapse the signedness (I8/U8 -> ST_8, I16/U16 -> ST_16, F32 ->
# ST_F32). The interpreter truncates on store (operations.h:827
# MemoryLocation::Store<P>: static_cast<P>(GetPrimitive(src.IR()).u64)) and
# sign/zero-extends on load (operations.h:837 Load<P>: Put(IR, {u64 =
# static_cast<uint64_t>(P value)}); F32 via operations.h:787 Store<float> /
# operations.h:802 Load<float>) - unlike the uslot loads (operations.h:867
# LoadPrim), no separate s8/u8 spellings exist here: the field's declared
# kind carries the signedness.
#
# One RecN record hosts all the narrow fields packed at their natural
# alignment (field_layout.cpp:82/86/92 flat sizes 1/2/4, field_layout.cpp:302
# SizeAlignmentAccumulator::AddField packing): i8@0, u8@1, i16@2, u16@4,
# f32@8, flat size 12. The typed slot occupies AlignUp(12, STACK_SLOT_SIZE) =
# 16 bytes (isa_rewriter.cpp:2326), so staticFrameSize is 16; RecF { f32 }
# fills 4 bytes -> an 8-byte slot. Field offsets are NOT spelled in the DSL:
# they come from the resolver's layout of the declared @type fields.
FRAME_SLOTS_NARROW_PREAMBLE = """
@field_ref recn_i8 = default:RecN@rec i8 I8
@field_ref recn_u8 = default:RecN@rec u8 U8
@field_ref recn_i16 = default:RecN@rec i16 I16
@field_ref recn_u16 = default:RecN@rec u16 U16
@field_ref recn_f32 = default:RecN@rec f32 F32

@type default:RecN
  @flags PUBLIC RECORD
  @field i8 I8
    @flags PUBLIC
  @end
  @field u8 U8
    @flags PUBLIC
  @end
  @field i16 I16
    @flags PUBLIC
  @end
  @field u16 U16
    @flags PUBLIC
  @end
  @field f32 F32
    @flags PUBLIC
  @end
@end
"""

FRAME_SLOTS_F32_PREAMBLE = """
@field_ref recf_f32 = default:RecF@rec f32 F32

@type default:RecF
  @flags PUBLIC RECORD
  @field f32 F32
    @flags PUBLIC
  @end
@end
"""

REC_NARROW_TYPE = "default:RecN@rec"
REC_F32_TYPE = "default:RecF@rec"

# The narrow kinds and their C++ reference extensions (mirror
# operations.h:837 Load<P> verbatim; see TypedSlotNarrowRoundTrip).
TYPED_NARROW_KINDS = ["i8", "u8", "i16", "u16"]


def typed_narrow_method(kind: str) -> Method:
    """Round-trips the I64 param through the narrow field `kind`: st.typed
    truncates to the field width and ld.typed extends it back."""
    return (
        method(f"test_typed_narrow_{kind}", I64, params=(I64,), default_w=64)
        .directive(f"@typed_slots {REC_NARROW_TYPE}")
        .live(IR1)
        .plain("st.typed", IR1, "$0", f"#recn_{kind}")
        .plain("@dead", IR1)
        .plain("ld.typed", IR1, "$0", f"#recn_{kind}")
        .ret()
    )


def typed_narrow_load_method(kind: str) -> Method:
    """Loads the narrow field `kind` of a slot pre-set by the test (no
    store): pins the load extension against known slot bytes."""
    return (
        method(f"test_typed_narrow_load_{kind}", I64, default_w=64)
        .directive(f"@typed_slots {REC_NARROW_TYPE}")
        .plain("ld.typed", IR1, "$0", f"#recn_{kind}")
        .ret()
    )


def typed_narrow_all_store_method() -> Method:
    """Stores a distinct pattern into every RecN field, then returns the
    i16 read-back (unused by the test, which inspects the raw slot bytes).
    The fresh registers need no @live markers (movi defines them)."""
    return (
        method("test_typed_narrow_all_store", I64, default_w=64)
        .directive(f"@typed_slots {REC_NARROW_TYPE}")
        .instr("movi", IR1, imm(0xA5), w=64)
        .instr("movi", IR2, imm(0x5A1C), w=64)
        .instr("movi", IR3, imm(0x8163), w=64)
        .instr("movi", IR4, imm(0x7211), w=64)
        .plain("st.typed", IR1, "$0", "#recn_i8")
        .plain("st.typed", IR2, "$0", "#recn_u8")
        .plain("st.typed", IR3, "$0", "#recn_i16")
        .plain("st.typed", IR4, "$0", "#recn_u16")
        .plain("@dead", f"{IR1}, {IR2}, {IR3}, {IR4}")
        .raw("fmovi.32 FR0, 1.5")
        .plain("st.typed.f", FR0, "$0", "#recn_f32")
        .plain("ld.typed", IR1, "$0", "#recn_i16")
        .ret()
    )


def typed_f32_method() -> Method:
    """Round-trips the F32 param through field `f32` of the typed slot 0."""
    return (
        method("test_typed_f32", F32, params=(F32,), default_w=32)
        .directive(f"@typed_slots {REC_F32_TYPE}")
        .plain("st.typed.f", FR0, "$0", "#recf_f32")
        .plain("ld.typed.f", FR0, "$0", "#recf_f32")
        .fret()
    )


def typed_f32_load_method() -> Method:
    """Loads field `f32` of a slot pre-set by the test (no store)."""
    return (
        method("test_typed_f32_load", F32, default_w=32)
        .directive(f"@typed_slots {REC_F32_TYPE}")
        .plain("ld.typed.f", FR0, "$0", "#recf_f32")
        .fret()
    )


def frame_rt_method(name: str, st_kind: str, ld_kind: str) -> Method:
    ref = st_kind == "ref"
    builder = method(f"test_{name}", I64, params=(I64,)).slots(1)
    builder = builder.live_ref(IR1) if ref else builder.live(IR1)
    builder = (
        builder.instr(f"st.uslot.{st_kind}", IR1, "$0")
        .instr(f"ld.uslot.{ld_kind}", IR2, "$0")
        .dead(IR1, "$0")
    )
    if ref:
        # The load result carries the REF liveness mark, which ret.64 rejects.
        return builder.instr("ret.ref", IR2).build()
    return builder.ret(IR2)


def frame_rt_float_method(kind: str) -> Method:
    float_type = AsmType(f"F{kind[1:]}")
    return (
        method(f"test_{kind}", float_type, params=(float_type,)).slots(1)
        .instr(f"st.uslot.{kind}", FR0, "$0")
        .instr(f"ld.uslot.{kind}", FR0, "$0")
        .fret()
    )


def frame_long_methods() -> list[Method]:
    long_int = (
        method("test_long", I64, params=(I64,)).slots(FRAME_SLOT_LONG_SLOT_COUNT)
        .live(IR1)
        .instr("st.uslot.64", IR1, "$0")
        .instr("ld.uslot.64", IR2, "$0")
        .dead(IR1)
        .instr("st.uslot.64", IR2, "$255")
        .instr("ld.uslot.64", IR3, "$255")
        .instr("add", IR2, IR2, IR3, w=64)
        .dead(IR3, "$255", "$0")
        .ret(IR2)
    )
    long_float = (
        method("test_long_f", F64, params=(F64,)).slots(FRAME_SLOT_LONG_SLOT_COUNT)
        .instr("st.uslot.f64", FR0, "$0")
        .instr("ld.uslot.f64", FR0, "$0")
        .fret()
    )
    return [long_int, long_float]


def generate_frame_slots() -> list[Path]:
    rt_files = {
        f"{name}.asm": asm_file([frame_rt_method(name, st_kind, ld_kind)])
                       for name, st_kind, ld_kind in FRAME_SLOT_RT_CASES
    }
    rt_files.update({
        f"{kind}.asm": asm_file([frame_rt_float_method(kind)]) for kind in FRAME_SLOT_FLOAT_KINDS
    })
    rt_files["typed_narrow.asm"] = asm_file_with_preamble(
        FRAME_SLOTS_NARROW_PREAMBLE,
        [typed_narrow_method(kind) for kind in TYPED_NARROW_KINDS]
        + [typed_narrow_all_store_method()]
        + [typed_narrow_load_method(kind) for kind in TYPED_NARROW_KINDS],
    )
    rt_files["typed_f32.asm"] = asm_file_with_preamble(
        FRAME_SLOTS_F32_PREAMBLE, [typed_f32_method(), typed_f32_load_method()]
    )
    imm_files: dict[str, list[Method]] = {}
    for width_file, value_name, imm, _, reads in IMM_SLOT_CASES:
        imm_files.setdefault(width_file, []).extend(
            frame_imm_method(width_file, value_name, imm, read) for read in reads
        )
    rt_files.update({
        f"{file}.asm": asm_file(methods) for file, methods in imm_files.items()
    })
    rt_files["st8.asm"] = asm_file(frame_st8_methods())
    return (
        write_suite("frame-slots/rt", rt_files)
        + write_suite("frame-slots", {"long.asm": asm_file(frame_long_methods())})
    )


# ---------------------------------------------------------------------------
# mem-obj: CbcSpecializedRawMem/CbcSpecializedFields/CbcSpecializedArrays suites
# ---------------------------------------------------------------------------

# Typed slots (ld.typed/st.typed(.f)) are not generated: they assemble and
# rewrite, but TypedSlotOffset (isa_rewriter.cpp, shared with ld.stack.rec)
# wraps every negative typed offset through an implicit uint32->int32
# conversion that the UBSan build (halt_on_error) rejects.
# lea.g/payload.g are not generated here either: they run over the mocked
# GetFieldOffset/base-pointer queries (todo-mock 3.3) and live in the mock-lea
# suite instead.

# Probed raw-memory spellings: ld.raw has only {u8,s8,u16,s16,32,u32,s32,64,ref,f32,f64}
# (no plain 8/16 forms); st.raw has {8,16,32,64,ref,f32,f64}. Both lower to
# LOAD_REC/STORE_REC (LOAD_LONG_REC/STORE_LONG_REC for offsets past the 12-bit
# window). ld.raw.u32 assembles but the interpreter LoadPrim FATALs on the
# LD_U32TO64 access kind, so it is not generated.
# Method names double as file names and must not contain '-' (the tokenizer
# reads it as a negative-number sign), hence the underscore spellings.
RAW_LOAD_CASES = [
    ("st8_ldu8", "8", "u8", 0x8),
    ("st8_lds8", "8", "s8", 0x8),
    ("st16_ldu16", "16", "u16", 0x8),
    ("st16_lds16", "16", "s16", 0x8),
    ("st32_ld32", "32", "32", 0x8),
    ("st32_lds32", "32", "s32", 0x8),
    ("st64_ld64", "64", "64", 0x8),
    # The 0x1000 pair leaves the 12-bit offset window (LOAD_LONG_REC/STORE_LONG_REC);
    # 0x18 exercises the split low4/sleb encoding.
    ("st64_ld64_multi", "64", "64", None),
]

# Field declarations shared by every fields-suite image. Layouts resolved from
# the image at rewrite (Cangjie layout rules): the record holds f0 I64@0,
# f1 I32@8, f2 I16@12, f3 I8@14, f4 U8@15, f5 U16@16, f6 F32@20, f7 F64@24,
# f8 REF@32; the class holds x I64@8, y I32@16, r0 REF@24 (ObjectHeaderSize
# added to class-owner offsets). The names must stay in sync with
# CbcSpecializedFields in test/cbc_test.cpp.
MEM_OBJ_FIELD_REFS = """
@field_ref rec_f0 = mem:Rec@rec f0 I64
@field_ref rec_f1 = mem:Rec@rec f1 I32
@field_ref rec_f2 = mem:Rec@rec f2 I16
@field_ref rec_f3 = mem:Rec@rec f3 I8
@field_ref rec_f4 = mem:Rec@rec f4 U8
@field_ref rec_f5 = mem:Rec@rec f5 U16
@field_ref rec_f6 = mem:Rec@rec f6 F32
@field_ref rec_f7 = mem:Rec@rec f7 F64
@field_ref rec_f8 = mem:Rec@rec f8 mem:Obj@ref
@field_ref obj_x = mem:Obj@ref x I64
@field_ref obj_y = mem:Obj@ref y I32
@field_ref obj_r0 = mem:Obj@ref r0 mem:Obj@ref
"""

MEM_OBJ_TYPES = """
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
  @field f1 I32
    @flags PUBLIC
  @end
  @field f2 I16
    @flags PUBLIC
  @end
  @field f3 I8
    @flags PUBLIC
  @end
  @field f4 U8
    @flags PUBLIC
  @end
  @field f5 U16
    @flags PUBLIC
  @end
  @field f6 F32
    @flags PUBLIC
  @end
  @field f7 F64
    @flags PUBLIC
  @end
  @field f8 mem:Obj@ref
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
  @field y I32
    @flags PUBLIC
  @end
  @field r0 mem:Obj@ref
    @flags PUBLIC
  @end
@end

"""


def mem_obj_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MEM_OBJ_FIELD_REFS.lstrip("\n") + MEM_OBJ_TYPES.lstrip(
        "\n"
    ) + "@type default\n" + body + "\n@end\n"


def raw_method(name: str, st_kind: str, ld_kind: str, offsets: tuple[int, ...]) -> Method:
    builder = method(f"test_{name}", I64, params=(I64, I64)).live(IR1, IR2)
    for off in offsets[:-1]:
        builder = (
            builder
            .instr(f"st.raw.{st_kind}", IR2, IR1, imm(off))
            .instr(f"ld.raw.{ld_kind}", IR3, IR1, imm(off))
            .dead(IR3)
        )
    return (
        builder
        .instr(f"st.raw.{st_kind}", IR2, IR1, imm(offsets[-1]))
        .instr(f"ld.raw.{ld_kind}", IR3, IR1, imm(offsets[-1]))
        .dead(IR1, IR2)
        .ret(IR3)
    )


def raw_ref_method(name: str) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64, I64))
        .live(IR1)
        .live_ref(IR2)
        .instr("st.raw.ref", IR2, IR1, imm(0x8))
        .instr("ld.raw.ref", IR3, IR1, imm(0x8))
        .dead(IR1, IR2)
        .instr("ret.ref", IR3)
        .build()
    )


def raw_float_method(name: str, kind: str) -> Method:
    float_type = AsmType(f"F{kind[1:]}")
    return (
        method(f"test_{name}", float_type, params=(I64, float_type))
        .live(IR1)
        .instr(f"st.raw.{kind}", FR0, IR1, imm(0x8))
        .instr(f"ld.raw.{kind}", FR1, IR1, imm(0x8))
        .dead(IR1)
        .fret(FR1)
    )


def field_rec_method(name: str, field: str, ref: bool) -> Method:
    builder = method(f"test_{name}", I64, params=(I64, I64)).live_rec(IR1)
    builder = builder.live_ref(IR2) if ref else builder.live(IR2)
    builder = (
        builder
        .instr("st.field", IR2, IR1, f"#rec_{field}")
        .instr("ld.field", IR3, IR1, f"#rec_{field}")
        .dead(IR1, IR2)
    )
    if ref:
        # The loaded value carries the REF liveness mark, which ret.64 rejects.
        return builder.instr("ret.ref", IR3).build()
    return builder.ret(IR3)


def field_obj_method(name: str, field: str, ref: bool) -> Method:
    builder = method(f"test_{name}", I64, params=(I64, I64)).live_ref(IR1)
    builder = builder.live_ref(IR2) if ref else builder.live(IR2)
    builder = (
        builder
        .instr("st.field", IR2, IR1, f"#obj_{field}")
        .instr("ld.field", IR3, IR1, f"#obj_{field}")
        .dead(IR1, IR2)
    )
    if ref:
        return builder.instr("ret.ref", IR3).build()
    return builder.ret(IR3)


def field_float_method(name: str, field: str, width: int) -> Method:
    float_type = AsmType(f"F{width}")
    return (
        method(f"test_{name}", float_type, params=(I64, float_type))
        .live_rec(IR1)
        .instr("st.field.f", FR0, IR1, f"#rec_{field}")
        .instr("ld.field.f", FR1, IR1, f"#rec_{field}")
        .dead(IR1)
        .fret(FR1)
    )


def derived_method(name: str, field_ref: str, owner_ref: bool) -> Method:
    # st.derived/ld.derived always emit STORE_LONG_DERIVED/LOAD_LONG_DERIVED;
    # base IR1 switches the interpreter between the LOCAL (base == 0) and HEAP
    # paths of LoadDerived/StoreDerived.
    builder = method(f"test_{name}", I64, params=(I64, I64, I64))
    builder = builder.live_ref(IR1) if owner_ref else builder.live_rec(IR1)
    builder = (
        builder.live(IR2, IR3)
        .instr("st.derived", IR3, IR1, IR2, f"#{field_ref}")
        .instr("ld.derived", IR4, IR1, IR2, f"#{field_ref}")
        .dead(IR1, IR2, IR3)
    )
    return builder.ret(IR4)


def derived_ref_method(name: str) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64, I64, I64))
        .live_ref(IR1, IR3)
        .live(IR2)
        .instr("st.derived", IR3, IR1, IR2, "#obj_r0")
        .instr("ld.derived", IR4, IR1, IR2, "#obj_r0")
        .dead(IR1, IR2, IR3)
        .instr("ret.ref", IR4)
        .build()
    )


def derived_float_method(name: str, width: int) -> Method:
    float_type = AsmType(f"F{width}")
    return (
        method(f"test_{name}", float_type, params=(I64, I64, float_type))
        .live_rec(IR1)
        .live(IR2)
        .instr("st.derived.f", FR0, IR1, IR2, "#rec_f6")
        .instr("ld.derived.f", FR1, IR1, IR2, "#rec_f6")
        .dead(IR1, IR2)
        .fret(FR1)
    )


def generic_ref_method() -> Method:
    # ld.g/st.g lower to LOAD_GENERIC/STORE_GENERIC; the mock IsReference() is
    # constant true, so a plain constant stands in for the TypeInfo register
    # (the three seedable IR registers carry base, value and derived address).
    return (
        method("test_gen_ref", I64, params=(I64, I64, I64))
        .live_ref(IR1, IR2)
        .live(IR3)
        .instr("movi", IR4, imm(0x1000), w=64)
        .instr("st.g", IR2, IR1, IR3, IR4)
        .instr("ld.g", IR5, IR1, IR3, IR4)
        .dead(IR1, IR2, IR3, IR4)
        .instr("ret.ref", IR5)
        .build()
    )


# st.arr.* marks the source register as a *define* in the liveness analyzer, so
# its value must be moved into a fresh register and that register @dead'ed
# before the store (st.raw/st.field use plain usage marks and need no dance).
def arr_method(name: str, st_kind: str, ld_kind: str) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64, I64, I64))
        .live_ref(IR1)
        .live(IR2, IR3)
        .dead(IR3)
        .instr(f"st.arr.{st_kind}", IR3, IR1, IR2)
        .dead(IR3)
        .instr(f"ld.arr.{ld_kind}", IR4, IR1, IR2)
        .dead(IR1, IR2)
        .ret(IR4)
    )


def arr_ref_method() -> Method:
    return (
        method("test_ref", I64, params=(I64, I64, I64))
        .live_ref(IR1, IR3)
        .live(IR2)
        .instr("mov.ref", IR4, IR3)
        .dead(IR4)
        .instr("st.arr.ref", IR4, IR1, IR2)
        .instr("ld.arr.ref", IR5, IR1, IR2)
        .dead(IR1, IR2, IR3)
        .instr("ret.ref", IR5)
        .build()
    )


def arr_float_method(name: str, kind: str) -> Method:
    float_type = AsmType(f"F{kind[1:]}")
    return (
        method(f"test_{name}", float_type, params=(I64, I64, float_type))
        .live_ref(IR1)
        .live(IR2)
        .instr(f"st.arr.{kind}", FR0, IR1, IR2)
        .instr(f"ld.arr.{kind}", FR1, IR1, IR2)
        .dead(IR1, IR2)
        .fret(FR1)
    )


def arr_len_method() -> Method:
    # arr.len lowers to LOAD_OBJ LD_64 at ObjectHeaderSize.
    return (
        method("test_len", I64, params=(I64,))
        .live_ref(IR1)
        .instr("arr.len", IR2, IR1)
        .dead(IR1)
        .ret(IR2)
    )


def arr_leabox_method() -> Method:
    # lea.box lowers to LOAD_OBJ LD_LEA at ObjectHeaderSize.
    return (
        method("test_leabox", I64, params=(I64,))
        .live_ref(IR1)
        .instr("lea.box", IR2, IR1)
        .dead(IR1)
        .ret(IR2)
    )


def generate_mem_obj() -> list[Path]:
    written = []
    raw_files = {
        f"{name}.asm": asm_file([raw_method(name, st_kind, ld_kind, (off,))])
                       for name, st_kind, ld_kind, off in RAW_LOAD_CASES if off is not None
    }
    raw_files.update({
        f"{name}.asm": asm_file([raw_method(name, st_kind, ld_kind, (0x8, 0x18, 0x1000))])
                       for name, st_kind, ld_kind, off in RAW_LOAD_CASES if off is None
    })
    raw_files["stref_ldref.asm"] = asm_file([raw_ref_method("stref_ldref")])
    raw_files.update({
        f"st{kind}_ld{kind}.asm": asm_file([raw_float_method(f"st{kind}_ld{kind}", kind)])
                                  for kind in ("f32", "f64")
    })
    written += write_suite("mem-obj/raw", raw_files)

    field_files = {
        "rec_64.asm": mem_obj_file([field_rec_method("rec_64", "f0", ref=False)]),
        "rec_32.asm": mem_obj_file([field_rec_method("rec_32", "f1", ref=False)]),
        "rec_16s.asm": mem_obj_file([field_rec_method("rec_16s", "f2", ref=False)]),
        "rec_8s.asm": mem_obj_file([field_rec_method("rec_8s", "f3", ref=False)]),
        "rec_8u.asm": mem_obj_file([field_rec_method("rec_8u", "f4", ref=False)]),
        "rec_16u.asm": mem_obj_file([field_rec_method("rec_16u", "f5", ref=False)]),
        "rec_ref.asm": mem_obj_file([field_rec_method("rec_ref", "f8", ref=True)]),
        "rec_f32.asm": mem_obj_file([field_float_method("rec_f32", "f6", 32)]),
        "rec_f64.asm": mem_obj_file([field_float_method("rec_f64", "f7", 64)]),
        "obj_64.asm": mem_obj_file([field_obj_method("obj_64", "x", ref=False)]),
        "obj_32.asm": mem_obj_file([field_obj_method("obj_32", "y", ref=False)]),
        "obj_ref.asm": mem_obj_file([field_obj_method("obj_ref", "r0", ref=True)]),
        "derived_rec_64.asm": mem_obj_file([derived_method("derived_rec_64", "rec_f0", owner_ref=False)]),
        "derived_obj_64.asm": mem_obj_file([derived_method("derived_obj_64", "obj_x", owner_ref=True)]),
        "derived_obj_ref.asm": mem_obj_file([derived_ref_method("derived_obj_ref")]),
        "derived_rec_f32.asm": mem_obj_file([derived_float_method("derived_rec_f32", 32)]),
        "gen_ref.asm": mem_obj_file([generic_ref_method()]),
    }
    written += write_suite("mem-obj/fields", field_files)

    arr_files = {
        f"{name}.asm": asm_file([arr_method(name, st_kind, ld_kind)])
                       for name, st_kind, ld_kind in [
                           ("u8", "8", "u8"),
                           ("s8", "8", "s8"),
                           ("u16", "16", "u16"),
                           ("s16", "16", "s16"),
                           ("32", "32", "32"),
                           ("64", "64", "64"),
                       ]
    }
    arr_files["ref.asm"] = asm_file([arr_ref_method()])
    arr_files.update({
        f"{kind}.asm": asm_file([arr_float_method(kind, kind)]) for kind in ("f32", "f64")
    })
    arr_files["len.asm"] = asm_file([arr_len_method()])
    arr_files["leabox.asm"] = asm_file([arr_leabox_method()])
    written += write_suite("mem-obj/arrays", arr_files)
    return written


# ---------------------------------------------------------------------------
# atomics: CbcSpecializedAtomics suite
# ---------------------------------------------------------------------------

# The cbc-asm DSL exposes only the typed atomic forms (atomic.load/atomic.store/
# atomic.cas/atomic.swap/atomic.fetch.{op} with a #field_ref path); the RT-style
# mnemonics (atomic.ld.8, cas.8, atomic.fetch.add.8, ...) do not exist in the
# parser table. The access kind and width come from the field type (I8 -> LD_S8,
# U8 -> LD_U8, I32/U32 -> LD_32/ST_32, ...), so the s/u spellings are expressed
# by the field type. Float fields are rejected (the rewriter FATALs on their
# kinds in CAS/swap/fetch and the interpreter on LD_F32/ST_F32), and the ref
# fields are blocked by the mock (AtomicReadRef and friends FATAL).
ATOMIC_KINDS = ["8s", "8u", "16s", "16u", "32", "64"]

ATOMIC_FIELD_TYPES = {"8s": I8, "8u": U8, "16s": I16, "16u": U16, "32": I32, "64": I64}

ATOMIC_FETCH_OPS = ["add", "sub", "and", "or", "xor"]


def atomic_type_decls() -> str:
    fields = "\n".join(
        f"  @field f{kind} {ATOMIC_FIELD_TYPES[kind]}\n    @flags PUBLIC\n  @end"
        for kind in ATOMIC_KINDS
    )
    return f"@type atm:Cell\n  @flags PUBLIC RECORD\n{fields}\n@end\n\n"


def atomic_field_refs() -> str:
    return "\n".join(
        f"@field_ref atm_f{kind} = atm:Cell@rec f{kind} {ATOMIC_FIELD_TYPES[kind]}"
        for kind in ATOMIC_KINDS
    ) + "\n"


def atomics_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return ('@main_type "default"\n' + atomic_field_refs() + atomic_type_decls() +
            "@type default\n" + body + "\n@end\n")


def atomic_ld_method(kind: str) -> Method:
    return (
        method(f"test_ld_{kind}", I64, params=(I64,))
        .live_ref(IR1)
        .instr("atomic.load", IR3, IR1, f"#atm_f{kind}")
        .dead(IR1)
        .ret(IR3)
    )


def atomic_st_method(kind: str) -> Method:
    return (
        method(f"test_st_{kind}", I64, params=(I64, I64))
        .live_ref(IR1)
        .live(IR2)
        .instr("atomic.store", IR2, IR1, f"#atm_f{kind}")
        .instr("atomic.load", IR3, IR1, f"#atm_f{kind}")
        .dead(IR1, IR2)
        .ret(IR3)
    )


def atomic_fetch_method(op: str, kind: str) -> Method:
    return (
        method(f"test_fetch_{op}_{kind}", I64, params=(I64, I64))
        .live_ref(IR1)
        .live(IR2)
        .instr(f"atomic.fetch.{op}", IR3, IR1, IR2, f"#atm_f{kind}")
        .dead(IR1, IR2)
        .ret(IR3)
    )


def atomic_swap_method(kind: str) -> Method:
    return (
        method(f"test_swap_{kind}", I64, params=(I64, I64))
        .live_ref(IR1)
        .live(IR2)
        .instr("atomic.swap", IR3, IR1, IR2, f"#atm_f{kind}")
        .dead(IR1, IR2)
        .ret(IR3)
    )


def atomic_cas_method(kind: str, flavor: str) -> Method:
    return (
        method(f"test_cas_{kind}_{flavor}", I64, params=(I64, I64, I64))
        .live_ref(IR1)
        .live(IR2, IR3)
        .instr("atomic.cas", IR4, IR1, IR2, IR3, f"#atm_f{kind}")
        .dead(IR1, IR2, IR3)
        .ret(IR4)
    )


def generate_atomics() -> list[Path]:
    written = write_suite("atomics/ld", {f"{k}.asm": atomics_file([atomic_ld_method(k)]) for k in ATOMIC_KINDS})
    written += write_suite("atomics/st", {f"{k}.asm": atomics_file([atomic_st_method(k)]) for k in ATOMIC_KINDS})
    written += write_suite("atomics/swap", {f"{k}.asm": atomics_file([atomic_swap_method(k)]) for k in ATOMIC_KINDS})
    written += write_suite(
        "atomics/fetch",
        {f"{op}-{k}.asm": atomics_file([atomic_fetch_method(op, k)])
                            for op in ATOMIC_FETCH_OPS for k in ATOMIC_KINDS},
    )
    written += write_suite(
        "atomics/cas",
        {f"{k}-{flavor}.asm": atomics_file([atomic_cas_method(k, flavor)])
                              for k in ATOMIC_KINDS for flavor in ("ok", "fail")},
    )
    return written


# ---------------------------------------------------------------------------
# checks-misc: CbcTest NullCheck*/DivCheck*/NopGcPoint/FloatImm32/FloatImm64/
# FloatNegInf32/FloatNegInf64/MovForms/MoviNegImm32/MoviNegImm64
# ---------------------------------------------------------------------------

# Float literals: plain decimals lex as ([0-9]+ '.' [0-9]*, parsed by java
# Double.parseDouble); the words nan/inf are additionally accepted in the
# fmovi.32/fmovi.64 immediate position (the only float-literal operand slot;
# assembler e08314ae) and encode bit-exactly (0x7ff8.../0x7ff0... for W64,
# 0x7fc0.../0x7f80... for W32). -inf has no spelling (deliberately dropped
# upstream, 75f971d7) - the neg-inf cases model it as inf x (-1.0). The
# overflow literal below still reaches +Infinity through the decimal path, and
# the denormals/maxima keep their exact decimal expansions (a literal without
# a fractional part would lex as an integer, hence the ".0" append).
INF_LITERAL = "1" + "0" * 400 + ".0"


def exact_float_literal(value: float) -> str:
    spelling = format(Decimal(value), "f")
    return spelling if "." in spelling else spelling + ".0"


# The names double as file/method-name parts and must stay in sync with
# FloatImmCases in test/cbc_test.cpp. The W32 literals are exact doubles that
# the assembler converts with floatToRawIntBits(fimm.toFloat).
FLOAT_IMM32_LITERALS = {
    "zero"     : "0.0",
    "negzero"  : "-0.0",
    "one"      : "1.5",
    "inf"      : INF_LITERAL,
    "neginf"   : "-" + INF_LITERAL,
    "denormmin": exact_float_literal(2.0**-149),
    "denormmax": exact_float_literal((2**23 - 1) * 2.0**-149),
    "max"      : exact_float_literal((2**24 - 1) * 2.0**104),
    "nan"      : "nan",
}

FLOAT_IMM64_LITERALS = {
    "zero"     : "0.0",
    "negzero"  : "-0.0",
    "one"      : "1.5",
    "inf"      : INF_LITERAL,
    "neginf"   : "-" + INF_LITERAL,
    "denormmin": exact_float_literal(5e-324),
    "denormmax": exact_float_literal((2**52 - 1) * 2.0**-1074),
    "max"      : exact_float_literal((2**53 - 1) * 2.0**971),
    "minnormal": exact_float_literal(2.0**-1022),
    "nan"      : "nan",
}


def nullcheck_method() -> Method:
    return (
        method("test_nullcheck", I64, params=(I64,))
        .live_ref(IR1)
        .raw("nullcheck IR1")
        .dead(IR1)
        .nops(1)
        .raw("gcpoint")
        .instr("movi", IR1, imm(1), w=64)
        .ret()
    )


def divcheck_method() -> Method:
    return (
        method("test_divcheck", I64, params=(I64,))
        .live(IR1)
        .raw("div.check IR1")
        .dead(IR1)
        .instr("movi", IR1, imm(2), w=64)
        .ret()
    )


def nop_gcpoint_method() -> Method:
    return (
        method("test_nop_gcpoint", I64, params=(I64,))
        .raw("nop")
        .raw("gcpoint")
        .instr("movi", IR1, imm(3), w=64)
        .raw("nop")
        .raw("gcpoint")
        .ret()
    )


def float_imm_method(name: str, literal: str, width: int) -> Method:
    return (
        method(f"test_{name}", AsmType(f"F{width}"))
        .raw(f"fmovi.{width} FR0, {literal}")
        .fret()
    )


def float_neg_inf_method(name: str, width: int) -> Method:
    # -inf is unspeakable; inf x (-1.0) flips only the sign bit (FMUL computes
    # in float/double per width, so the result is the exact -inf pattern).
    return (
        method(f"test_{name}", AsmType(f"F{width}"))
        .instr("fmovi", FR0, "inf", w=width)
        .instr("fmovi", FR1, "-1.0", w=width)
        .instr("fmul", FR0, FR0, FR1, w=width)
        .fret()
    )


def mov_method(width: int) -> Method:
    # Both mov widths lower to the width-less RT MOV (a full 64-bit cell copy).
    return (
        method(f"test_mov{width}", I64, params=(I64,))
        .live(IR1)
        .instr("mov", IR2, IR1, w=width)
        .ret(IR2)
    )


def movi_method(name: str, value: int) -> Method:
    # A 4-bit immediate lowers to the RT MOVI instruction, a 12-bit one to
    # BINI32I, and anything wider to BINI32L (MovImm falls back to AddI from IRZ).
    return (
        method(f"test_{name}", I64)
        .instr("movi", IR1, imm(value), w=32)
        .ret()
    )


def movi_pair_method() -> Method:
    # Negative immediates used to trip the engine's MovImm decoding (implicit
    # signed-to-unsigned conversion, UBSan); fixed upstream by 8049baee and
    # covered by the movi-neg-32/movi-neg-64 case files below.
    return (
        method("test_movi32", I64)
        .instr("movi", IR2, imm(0x7), w=32)
        .instr("movi", IR3, imm(0x3), w=32)
        .instr("add", IR1, IR2, IR3, w=32)
        .ret()
    )


# Negative movi immediates. The DSL has movi.32/movi.64 only (no .8/.16
# spellings; the engine's Emitter::MovImm accepts W32/W64 only, too). The
# immediates sweep the three post-rewrite encodings by signed width:
# [-8, -1] -> RT MOVI (4-bit), [-2048, -9] -> BINI32I/BINI64I (12-bit, via
# AddI from IRZ), anything below -> BINI32L/BINI64L (literal pool), with the
# 4/12-bit boundaries included. The names double as file/method-name parts
# and must stay in sync with MoviImm32Cases/MoviImm64Cases in test/cbc_test.cpp.
MOVI_NEG32_IMMS = {
    "neg1"   : -0x1,
    "neg8"   : -0x8,
    "neg9"   : -0x9,
    "neg80"  : -0x80,
    "neg800" : -0x800,
    "neg801" : -0x801,
    "neg8000": -0x8000,
    "negmin" : -0x80000000,
}

MOVI_NEG64_IMMS = {
    "neg1"   : -0x1,
    "neg8"   : -0x8,
    "neg9"   : -0x9,
    "neg80"  : -0x80,
    "neg800" : -0x800,
    "neg801" : -0x801,
    "neg8000": -0x8000,
    "negmin" : -0x8000000000000000,
}


def movi_neg_imm_method(name: str, value: int, width: int) -> Method:
    return (
        method(f"test_{name}", I64)
        .instr("movi", IR1, imm(value), w=width)
        .ret()
    )


def mov_ref_method() -> Method:
    return (
        method("test_movref", I64, params=(I64,))
        .live_ref(IR1)
        .instr("mov.ref", IR2, IR1)
        .dead(IR1)
        .raw("nop")
        .raw("gcpoint")
        .instr("ret.ref", IR2)
        .build()
    )


def generate_checks_misc() -> list[Path]:
    return (
        write_suite(
            "checks-misc",
            {
                "nullcheck.asm"  : asm_file([nullcheck_method()]),
                "divcheck.asm"   : asm_file([divcheck_method()]),
                "nop-gcpoint.asm": asm_file([nop_gcpoint_method()]),
                "mov-32.asm"     : asm_file([mov_method(32)]),
                "mov-64.asm"     : asm_file([mov_method(64)]),
                "movi-32.asm"    : asm_file([movi_pair_method()]),
                "movi-32-narrow.asm": asm_file([movi_method("movi32_narrow", 0x7FF)]),
                "movi-32-wide.asm"  : asm_file([movi_method("movi32_wide", 0x12345678)]),
                "mov-ref.asm"    : asm_file([mov_ref_method()]),
                "fmovi-neginf-32.asm": asm_file([float_neg_inf_method("fmovi_neginf32", 32)]),
                "fmovi-neginf-64.asm": asm_file([float_neg_inf_method("fmovi_neginf64", 64)]),
            },
        )
        + write_suite(
            "checks-misc/fmovi-32",
            {f"{name}.asm": asm_file([float_imm_method(name, lit, 32)]) for name, lit in FLOAT_IMM32_LITERALS.items()},
        )
        + write_suite(
            "checks-misc/fmovi-64",
            {f"{name}.asm": asm_file([float_imm_method(name, lit, 64)]) for name, lit in FLOAT_IMM64_LITERALS.items()},
        )
        + write_suite(
            "checks-misc/movi-neg-32",
            {f"{name}.asm": asm_file([movi_neg_imm_method(name, value, 32)]) for name, value in MOVI_NEG32_IMMS.items()},
        )
        + write_suite(
            "checks-misc/movi-neg-64",
            {f"{name}.asm": asm_file([movi_neg_imm_method(name, value, 64)]) for name, value in MOVI_NEG64_IMMS.items()},
        )
    )


# ---------------------------------------------------------------------------
# calls: CbcTest Call* suite
# ---------------------------------------------------------------------------

# Calls run through the unit-test mock (I2I callee on the same register set).
# Signatures may exceed the 6-int/8-float register windows of src/cbc/abi.h:
# the tests then pass a real frameBot (frame.start - ExecBytecodeInfo::frameSize)
# and assert the rewriter's staging (st.stack.param.*) via the frame buffer,
# because the DSL cannot read incoming stack params (no IR7 operands, no
# ld.tail.param) — the callees only consume the register window.
# The call itself consumes registers invisibly to the assembler liveness
# analyzer: a register whose value is needed across a call stays live, one
# redefined after a call without a same-op read needs @dead first, and a
# dead register must not be read again (the redefinition by the callee's
# return is invisible).

CALLS_INT_PARAM_REGS = [IR1, IR2, IR3, IR4, IR5, IR6]

CALLS_FLOAT_PARAM_REGS = [FR0, FR1, FR2, FR3, FR4, FR5, FR6, FR7]


def calls_file(refs: list[str], methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    header = '@main_type "default"\n\n' + "\n".join(refs) + "\n\n@type default\n"
    return header + body + "\n@end\n"


def calls_ref(name: str, params: tuple[AsmType, ...], ret: AsmType) -> str:
    return f"@method_ref default.{name} = default@ref {name}({', '.join(params)}){ret}"


def calls_int_sum_callee(name: str, params: tuple[AsmType, ...]) -> Method:
    """Sums the int params left to right into IR1."""
    regs = CALLS_INT_PARAM_REGS[: len(params)]
    builder = method(name, I64, params=params, default_w=64).live(*regs)
    for src in regs[1:]:
        builder = builder.instr("add", IR1, IR1, src)
    return builder.ret()


def calls_int_chain_caller(callee: str, count: int) -> Method:
    """Passes x, x+1, ..., x+(count-1) through the int param registers."""
    builder = method("main", I64, params=(I64,), default_w=64).live(IR1)
    for extra in range(1, count):
        builder = builder.instr("addi", CALLS_INT_PARAM_REGS[extra], IR1, imm(extra))
    call = ", ".join(CALLS_INT_PARAM_REGS[:count])
    return builder.raw(f"call.direct #default.{callee}, {call}").ret()


def calls_float_caller(callee: str, width: int, values: list[str], operands: tuple[str, ...]) -> MethodBuilder:
    float_type = AsmType(f"F{width}")
    builder = method("main", float_type)
    for reg, value in zip(CALLS_FLOAT_PARAM_REGS, values):
        builder = builder.instr("fmovi", reg, value, w=width)
    return builder.raw(f"call.direct #default.{callee}, {', '.join(operands)}")


def generate_calls() -> list[Path]:
    written = []

    six_int = (I64,) * 6
    written += write_suite(
        "calls",
        {
            "sum6i.asm": calls_file(
                [calls_ref("sum6i", six_int, I64)],
                [
                    calls_int_sum_callee("sum6i", six_int),
                    calls_int_chain_caller("sum6i", 6),
                ],
            ),
        },
    )

    eight_float = (F64,) * 8
    fsum8_callee = (
        method("fsum8", F64, params=eight_float, default_w=64)
        .instr("fadd", FR0, FR0, FR1)
        .instr("fadd", FR0, FR0, FR2)
        .instr("fadd", FR0, FR0, FR3)
        .instr("fadd", FR0, FR0, FR4)
        .instr("fadd", FR0, FR0, FR5)
        .instr("fadd", FR0, FR0, FR6)
        .instr("fadd", FR0, FR0, FR7)
        .fret()
    )
    fsum8_caller = calls_float_caller(
        "fsum8", 64, [f"{v}.0" for v in range(1, 9)], tuple(CALLS_FLOAT_PARAM_REGS)
    ).fret()
    written += write_suite(
        "calls",
        {"fsum8.asm": calls_file([calls_ref("fsum8", eight_float, F64)], [fsum8_callee, fsum8_caller])},
    )

    fsumw4_callee = (
        method("fsumw4", F32, params=(F32,) * 4, default_w=32)
        .instr("fadd", FR4, FR0, FR0)
        .instr("fadd", FR4, FR4, FR4)
        .instr("fadd", FR4, FR4, FR4)
        .instr("fadd", FR5, FR1, FR1)
        .instr("fadd", FR5, FR5, FR5)
        .instr("fadd", FR4, FR4, FR5)
        .instr("fadd", FR5, FR2, FR2)
        .instr("fadd", FR4, FR4, FR5)
        .instr("fadd", FR4, FR4, FR3)
        .fret(FR4)
    )
    fsumw4_caller = calls_float_caller(
        "fsumw4", 32, ["1.5", "2.5", "3.5", "4.5"], (FR1, FR0, FR3, FR2)
    ).fret()
    written += write_suite(
        "calls",
        {"fsum4w.asm": calls_file([calls_ref("fsumw4", (F32,) * 4, F32)], [fsumw4_callee, fsumw4_caller])},
    )

    mixed_params = six_int + (F64,) * 4
    mixed_callee = (
        method("sum_mixed", I64, params=mixed_params, default_w=64)
        .live(*CALLS_INT_PARAM_REGS)
        .instr("add", IR1, IR1, IR2)
        .instr("add", IR1, IR1, IR3)
        .instr("add", IR1, IR1, IR4)
        .instr("add", IR1, IR1, IR5)
        .instr("add", IR1, IR1, IR6)
        .dead(IR2, IR3, IR4, IR5, IR6)
        # f2i is width-less (the type tags carry the conversion), so it must
        # bypass the method's default width suffix.
        .raw("f2i F64, I64, FR0, IR2")
        .raw("f2i F64, I64, FR1, IR3")
        .raw("f2i F64, I64, FR2, IR4")
        .raw("f2i F64, I64, FR3, IR5")
        .instr("add", IR1, IR1, IR2)
        .instr("add", IR1, IR1, IR3)
        .instr("add", IR1, IR1, IR4)
        .instr("add", IR1, IR1, IR5)
        .ret()
    )
    mixed_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("addi", IR2, IR1, imm(1))
        .instr("addi", IR3, IR1, imm(2))
        .instr("addi", IR4, IR1, imm(3))
        .instr("addi", IR5, IR1, imm(4))
        .instr("addi", IR6, IR1, imm(5))
        .instr("fmovi", FR0, "10.0", w=64)
        .instr("fmovi", FR1, "20.0", w=64)
        .instr("fmovi", FR2, "30.0", w=64)
        .instr("fmovi", FR3, "40.0", w=64)
        .raw(f"call.direct #default.sum_mixed, {', '.join(CALLS_INT_PARAM_REGS + CALLS_FLOAT_PARAM_REGS[:4])}")
        .ret()
    )
    written += write_suite(
        "calls",
        {"sum-mixed.asm": calls_file([calls_ref("sum_mixed", mixed_params, I64)], [mixed_callee, mixed_caller])},
    )

    pick2_callee = (
        method("pick2", I64, params=(I64, F64))
        .live(IR1)
        .raw("f2i F64, I64, FR0, IR2")
        .instr("add", IR1, IR1, IR2, w=64)
        .ret()
    )
    slots_caller = (
        method("main", I64, params=(I64,))
        .slots(2)
        .live(IR1)
        .instr("st.uslot.64", IR1, "$0")
        .instr("fmovi", FR0, "7.5", w=64)
        .instr("st.uslot.f64", FR0, "$1")
        .raw("call.direct #default.pick2, $0, $1")
        .ret()
    )
    written += write_suite(
        "calls",
        {
            "args-from-slots.asm": calls_file(
                [calls_ref("pick2", (I64, F64), I64)], [pick2_callee, slots_caller]
            ),
        },
    )

    pick3_callee = (
        method("pick3", I64, params=(I64, I64, I64), default_w=64)
        .live(IR1, IR2, IR3)
        .instr("muli", IR1, IR1, imm(0x100))
        .instr("muli", IR2, IR2, imm(0x10))
        .instr("add", IR1, IR1, IR2)
        .instr("add", IR1, IR1, IR3)
        .ret()
    )
    shuffle_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("addi", IR2, IR1, imm(1))
        .instr("addi", IR3, IR1, imm(2))
        .raw("call.direct #default.pick3, IR3, IR2, IR1")
        .ret()
    )
    written += write_suite(
        "calls",
        {"shuffle.asm": calls_file([calls_ref("pick3", (I64, I64, I64), I64)], [pick3_callee, shuffle_caller])},
    )

    leaf_callee = (
        method("leaf", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("add", IR1, IR1, IR1)
        .ret()
    )
    mid_method = (
        method("mid", I64, params=(I64, I64))
        .slots(2)
        .live(IR1)
        .instr("st.uslot.64", IR1, "$0")
        .instr("subi", IR2, IR1, imm(1), w=64)
        .raw("call.direct #default.leaf, IR2")
        .dead(IR2)
        .instr("ld.uslot.64", IR2, "$0")
        .instr("ld.uslot.64", IR3, "$0")
        .instr("add", IR1, IR1, IR2, w=64)
        .instr("add", IR1, IR1, IR3, w=64)
        .dead(IR2, IR3, "$0")
        .ret()
    )
    nested_caller = (
        method("main", I64, params=(I64,))
        .slots(1)
        .live(IR1)
        .instr("st.uslot.64", IR1, "$0")
        .raw("call.direct #default.mid, $0, IRZ")
        .ret()
    )
    written += write_suite(
        "calls",
        {
            "nested.asm": calls_file(
                [calls_ref("leaf", (I64,), I64), calls_ref("mid", (I64, I64), I64)],
                [leaf_callee, mid_method, nested_caller],
            ),
        },
    )

    twocalls_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .live(IR1)
        .raw("call.direct #default.twice, IR1")
        .instr("add", IR2, IR1, IR1)
        .instr("addi", IR3, IR2, imm(1))
        .raw("call.direct #default.twice, IR3")
        .ret()
    )
    twice_callee = (
        method("twice", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("add", IR1, IR1, IR1)
        .ret()
    )
    written += write_suite(
        "calls",
        {"twocalls.asm": calls_file([calls_ref("twice", (I64,), I64)], [twice_callee, twocalls_caller])},
    )

    scribble_callee = (
        method("scribble", I64, params=(I64, I64, I64), default_w=64)
        .saved_iregs(IR8, IR9)
        .live(IR1)
        .instr("movi", IR8, imm(0x5), w=64)
        .instr("movi", IR9, imm(0x6), w=64)
        .dead(IR8, IR9)
        .ret()
    )
    nonvol_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("movi", IR8, imm(0x77), w=64)
        .instr("movi", IR9, imm(0x88), w=64)
        .raw("call.direct #default.scribble, IR1, IRZ, IRZ")
        .instr("add", IR1, IR1, IR8)
        .instr("add", IR1, IR1, IR9)
        .ret()
    )
    written += write_suite(
        "calls",
        {
            "nonvol.asm": calls_file(
                [calls_ref("scribble", (I64, I64, I64), I64)], [scribble_callee, nonvol_caller]
            ),
        },
    )

    # Stack-passed arguments: args 7+ of an 8-int signature are staged by the
    # rewriter to frameBot + k * 8 (st.stack.param), args 9+ of a 10-float
    # signature likewise (st.stack.param.f). The callee bodies read only the
    # register window; the staged cells are checked by the C++ tests.
    eight_int = (I64,) * 8
    sum8i_callee = (
        method("sum8i", I64, params=eight_int, default_w=64)
        .live(*CALLS_INT_PARAM_REGS)
        .instr("add", IR1, IR1, IR2)
        .instr("add", IR1, IR1, IR3)
        .instr("add", IR1, IR1, IR4)
        .instr("add", IR1, IR1, IR5)
        .instr("add", IR1, IR1, IR6)
        .ret()
    )
    sum8i_staged_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("addi", IR2, IR1, imm(1))
        .instr("addi", IR3, IR1, imm(2))
        .instr("addi", IR4, IR1, imm(3))
        .instr("addi", IR5, IR1, imm(4))
        .instr("addi", IR6, IR1, imm(5))
        .instr("addi", IR8, IR1, imm(6))
        .instr("addi", IR9, IR1, imm(7))
        .raw(f"call.direct #default.sum8i, {', '.join(CALLS_INT_PARAM_REGS + [IR8, IR9])}")
        .ret()
    )
    written += write_suite(
        "calls",
        {"sum8i.asm": calls_file([calls_ref("sum8i", eight_int, I64)], [sum8i_callee, sum8i_staged_caller])},
    )

    # Stack args sourced from the caller's frame slots take the
    # st.stack.param.s.16 path (slot cell -> staged cell copy).
    sum8i_slots_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .slots(2)
        .live(IR1)
        .instr("addi", IR2, IR1, imm(1))
        .instr("addi", IR3, IR1, imm(2))
        .instr("addi", IR4, IR1, imm(3))
        .instr("addi", IR5, IR1, imm(4))
        .instr("addi", IR6, IR1, imm(5))
        .instr("addi", IR8, IR1, imm(6))
        .instr("addi", IR9, IR1, imm(7))
        .raw("st.uslot.64 IR8, $0")
        .raw("st.uslot.64 IR9, $1")
        .raw(f"call.direct #default.sum8i, {', '.join(CALLS_INT_PARAM_REGS + ['$0', '$1'])}")
        .ret()
    )
    written += write_suite(
        "calls",
        {"sum8i-slots.asm": calls_file([calls_ref("sum8i", eight_int, I64)], [sum8i_callee, sum8i_slots_caller])},
    )

    # Same staging from slots, but the 16-bit source offset overflows
    # (staticFrameSize > 32K), so the rewriter picks st.stack.param.s.32.
    sum8i_slots_wide_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .slots(5000)
        .live(IR1)
        .instr("addi", IR2, IR1, imm(1))
        .instr("addi", IR3, IR1, imm(2))
        .instr("addi", IR4, IR1, imm(3))
        .instr("addi", IR5, IR1, imm(4))
        .instr("addi", IR6, IR1, imm(5))
        .instr("addi", IR8, IR1, imm(6))
        .instr("addi", IR9, IR1, imm(7))
        .raw("st.uslot.64 IR8, $0")
        .raw("st.uslot.64 IR9, $1")
        .raw(f"call.direct #default.sum8i, {', '.join(CALLS_INT_PARAM_REGS + ['$0', '$1'])}")
        .ret()
    )
    written += write_suite(
        "calls",
        {
            "sum8i-slots-wide.asm": calls_file(
                [calls_ref("sum8i", eight_int, I64)], [sum8i_callee, sum8i_slots_wide_caller]
            ),
        },
    )

    ten_float = (F64,) * 10
    fsum10f_callee = (
        method("fsum10f", F64, params=ten_float, default_w=64)
        .instr("fadd", FR0, FR0, FR1)
        .instr("fadd", FR0, FR0, FR2)
        .instr("fadd", FR0, FR0, FR3)
        .instr("fadd", FR0, FR0, FR4)
        .instr("fadd", FR0, FR0, FR5)
        .instr("fadd", FR0, FR0, FR6)
        .instr("fadd", FR0, FR0, FR7)
        .fret()
    )
    fsum10f_caller = (
        method("main", F64, default_w=64)
        .instr("fmovi", FR0, "1.0", w=64)
        .instr("fmovi", FR1, "2.0", w=64)
        .instr("fmovi", FR2, "3.0", w=64)
        .instr("fmovi", FR3, "4.0", w=64)
        .instr("fmovi", FR4, "5.0", w=64)
        .instr("fmovi", FR5, "6.0", w=64)
        .instr("fmovi", FR6, "7.0", w=64)
        .instr("fmovi", FR7, "8.0", w=64)
        .instr("fmovi", FR8, "9.0", w=64)
        .instr("fmovi", FR9, "10.0", w=64)
        .raw(f"call.direct #default.fsum10f, {', '.join(CALLS_FLOAT_PARAM_REGS + [FR8, FR9])}")
        .fret()
    )
    written += write_suite(
        "calls",
        {"fsum10f.asm": calls_file([calls_ref("fsum10f", ten_float, F64)], [fsum10f_callee, fsum10f_caller])},
    )

    # Both register windows exactly full (6 int + 8 float), no staging.
    sum6i8f_callee = (
        method("sum6i8f", I64, params=six_int + eight_float, default_w=64)
        .live(*CALLS_INT_PARAM_REGS)
        .instr("fadd", FR0, FR0, FR1)
        .instr("fadd", FR0, FR0, FR2)
        .instr("fadd", FR0, FR0, FR3)
        .instr("fadd", FR0, FR0, FR4)
        .instr("fadd", FR0, FR0, FR5)
        .instr("fadd", FR0, FR0, FR6)
        .instr("fadd", FR0, FR0, FR7)
        .instr("add", IR1, IR1, IR2)
        .instr("add", IR1, IR1, IR3)
        .instr("add", IR1, IR1, IR4)
        .instr("add", IR1, IR1, IR5)
        .instr("add", IR1, IR1, IR6)
        .dead(IR2, IR3, IR4, IR5, IR6)
        # f2i is width-less (the type tags carry the conversion), so it must
        # bypass the method's default width suffix.
        .raw("f2i F64, I64, FR0, IR2")
        .instr("add", IR1, IR1, IR2)
        .ret()
    )
    sum6i8f_caller = (
        method("main", I64, params=(I64,), default_w=64)
        .live(IR1)
        .instr("addi", IR2, IR1, imm(1))
        .instr("addi", IR3, IR1, imm(2))
        .instr("addi", IR4, IR1, imm(3))
        .instr("addi", IR5, IR1, imm(4))
        .instr("addi", IR6, IR1, imm(5))
        .instr("fmovi", FR0, "1.0", w=64)
        .instr("fmovi", FR1, "2.0", w=64)
        .instr("fmovi", FR2, "3.0", w=64)
        .instr("fmovi", FR3, "4.0", w=64)
        .instr("fmovi", FR4, "5.0", w=64)
        .instr("fmovi", FR5, "6.0", w=64)
        .instr("fmovi", FR6, "7.0", w=64)
        .instr("fmovi", FR7, "8.0", w=64)
        .raw(f"call.direct #default.sum6i8f, {', '.join(CALLS_INT_PARAM_REGS + CALLS_FLOAT_PARAM_REGS)}")
        .ret()
    )
    written += write_suite(
        "calls",
        {
            "sum6i8f.asm": calls_file(
                [calls_ref("sum6i8f", six_int + eight_float, I64)], [sum6i8f_callee, sum6i8f_caller]
            ),
        },
    )
    return written


# ---------------------------------------------------------------------------
# mock-objects: newobj/newarr/index/load.type.info over the synthetic TypeInfos
# ---------------------------------------------------------------------------

# CreateTypeInfo (test/mock/interpreter.cpp) resolves every bytecode-embedded
# term to an immortal synthetic TestTypeInfo: object-like {size = 2 *
# ObjectHeaderSize, align = sizeof(void*), no reference offsets} for
# non-primitive terms, natural sizes for primitives. MockNewObj/MockNewArr
# allocate from the mock heap and store that TI pointer in the object header,
# so the in-asm identities below hold: the rewrite-time TI is pointer-identical
# to the run-time one. Two quirks shape the method bodies: newarr defines IR1
# with a REF liveness mark while index needs a REC-marked source (allocated
# arrays are returned to the C++ test and seeded into the index workers of
# index.asm instead), and the synthetic TI covers header + one payload slot, so
# only the x I64@8 field of mem:Obj fits a newobj allocation (a second class
# field would overflow it).
# The CC of scc/bcc is comma-separated from the register operands, and the
# length register IR2 of newarr/newarr(.g) is consumed via a prim liveness use
# even when the method has no matching parameter.
#
# Family 2 (copy/tag/interf): copy.local(.g)/copy.to.local(.g)/copy.from.local(.g)
# are raw memcpys of the synthetic TI's size (16 = header + payload for a rec), so
# their round-trips cover the header word of the mock-heap objects - the source
# header is clobbered before the copy to observe the overwrite - and a third
# object allocated right after the destination guards against over-copy. The
# useRec marks these ops demand (newobj defines REF) are re-stamped with the
# @dead + @live.rec dance. The .g forms take the same registry TI from a
# load.type.info register. tag.g rewrites to BRANCH_IS_REF + a ref load of the
# payload word + SCC against IRZ; the mock's IsReference is constant true, so the
# ref branch always runs and the tag is the payload-word non-nullness (REQ/RNE
# per the option's enum kind). call.interf resolves through the interface's
# method table (flagless method refs: MethodDefinition::GetABIFlags has no
# abiKind bits, so any abi kind on the ref would fail the ABI compatibility
# check) and dispatches through InterfaceDispatchMock: the C++ test supplies an
# I2ICall thunk targeting the impl method of the same image and reads the
# recorded dispatch (receiver TI from the object header, interf TI, methodNum).
# Excluded: copy/copy.g (CopyDerived always unwraps the derived pointer through
# UnwrapDerivedPtr, which FATALs), copy.typed*/copy.to.typed*/copy.from.typed*
# (TypedSlotOffset in isa_rewriter.cpp still wraps negative offsets through the
# unsigned subtraction that the UBSan build rejects). new.none.g/new.some.g
# moved to new-option.asm once todo-mock 3.4 put the NEWOBJ_ACC_G result into
# IR_ACC; payload.g was excluded for the same FATALing GetFieldOffset and moved
# to the mock-lea suite once todo-mock 3.3 stubbed it.

MOCK_REC_ARRAY = "@arr[mem:Rec@rec]"  # 16-byte elements (rec elem TI)
MOCK_I64_ARRAY = "@arr[I64]"          # 8-byte elements (prim elem TI)

MOCK_OBJ_FIELD_REFS = """
@field_ref obj_x = mem:Obj@ref x I64
"""

MOCK_OBJ_TYPES = """
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
@end

"""


def mock_obj_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MOCK_OBJ_FIELD_REFS.lstrip("\n") + MOCK_OBJ_TYPES.lstrip(
        "\n"
    ) + "@type default\n" + body + "\n@end\n"


def newobj_methods() -> list[Method]:
    return [
        # Returns the raw object pointer for the C++-side header inspection.
        method("test_ptr", I64)
            .raw("newobj mem:Obj@ref")
            .raw("ret.ref IR1")
            .build(),

        # Stores a prim through the class field path (x I64@8) and returns the
        # object pointer, so the C++ test checks the payload slot behind the
        # untouched TI header.
        method("test_field", I64)
            .live(IR2)
            .raw("newobj mem:Obj@ref")
            .raw("st.field IR2, IR1, #obj_x")
            .dead(IR2)
            .raw("ret.ref IR1")
            .build(),

        # Two newobj results are distinct mock-heap objects (bcc RNE compares
        # the raw pointer cells).
        method("test_distinct", I64)
            .raw("newobj mem:Rec@rec")
            .raw("mov.ref IR4, IR1")
            .dead(IR1)
            .raw("newobj mem:Rec@rec")
            .instr("movi", IR3, imm(1), w=64)
            .raw("bcc.64 RNE, IR1, IR4, skip")
            .instr("subi", IR3, IR3, imm(1), w=64)
            .label("skip")
            .dead(IR1, IR4)
            .ret(IR3),

        # The header TI is pointer-identical to the rewrite-time TI of the same
        # term (load.type.info embeds it as a MovImm immediate).
        method("test_ti", I64)
            .raw("newobj mem:Rec@rec")
            .raw("load.type.info IR2, mem:Rec@rec")
            .raw("ld.raw.64 IR3, IR1, 0")
            .raw("scc.64 EQ, IR4, IR2, IR3")
            .dead(IR1, IR2, IR3)
            .ret(IR4),

        # newobj.g takes the TI from a register and still stores it in the
        # header.
        method("test_g", I64)
            .raw("load.type.info IR4, mem:Rec@rec")
            .raw("newobj.g IR4, mem:Rec@rec")
            .raw("ld.raw.64 IR3, IR1, 0")
            .raw("scc.64 EQ, IR5, IR4, IR3")
            .dead(IR1, IR3, IR4)
            .ret(IR5),
    ]


def newarr_methods() -> list[Method]:
    def len_method(name: str, array_sig: str) -> Method:
        return (
            method(name, I64)
            .live(IR2)
            .raw(f"newarr {array_sig}")
            .raw("arr.len IR3, IR1")
            .dead(IR1)
            .ret(IR3)
        )

    def ptr_method(name: str, array_sig: str) -> Method:
        return (
            method(name, I64)
            .live(IR2)
            .raw(f"newarr {array_sig}")
            .raw("ret.ref IR1")
            .build()
        )

    return [
        len_method("test_len", MOCK_REC_ARRAY),
        len_method("test_len_prim", MOCK_I64_ARRAY),
        # newarr.g takes the TI from a register (load.type.info result).
        (
            method("test_len_g", I64)
            .live(IR2)
            .raw(f"load.type.info IR4, {MOCK_I64_ARRAY}")
            .raw(f"newarr.g IR4, {MOCK_I64_ARRAY}")
            .dead(IR4)
            .raw("arr.len IR3, IR1")
            .dead(IR1)
            .ret(IR3)
        ),
        # Returns the raw array pointer for the C++-side layout inspection.
        ptr_method("test_ptr", MOCK_REC_ARRAY),
        ptr_method("test_ptr_prim", MOCK_I64_ARRAY),
        # The header TI of a newarr allocation is the synthetic TI of the array
        # term itself, identical to the rewrite-time one.
        (
            method("test_arr_ti", I64)
            .live(IR2)
            .raw(f"newarr {MOCK_REC_ARRAY}")
            .dead(IR2)
            .raw(f"load.type.info IR2, {MOCK_REC_ARRAY}")
            .raw("ld.raw.64 IR3, IR1, 0")
            .raw("scc.64 EQ, IR4, IR2, IR3")
            .dead(IR1, IR2, IR3)
            .ret(IR4)
        ),
    ]


def index_methods() -> list[Method]:
    # index dst/src must be REC-marked (the assembler marks both with useRec),
    # hence @live.rec on the array parameter and on the result register; the
    # elements round-trip through st.raw/ld.raw at the computed addresses.
    def index_method(name: str, array_sig: str) -> Method:
        return (
            method(name, I64, params=(I64, I64, I64))
            .live_rec(IR1, IR4)
            .live(IR2, IR3)
            .raw(f"index IR4, IR1, IR2, {array_sig}")
            .raw("st.raw.64 IR3, IR4, 0")
            .raw("ld.raw.64 IR5, IR4, 0")
            .dead(IR1, IR2, IR3)
            .ret(IR5)
        )

    return [
        index_method("test_index", MOCK_REC_ARRAY),
        index_method("test_index_prim", MOCK_I64_ARRAY),
    ]


def load_type_info_methods() -> list[Method]:
    # The Bool spelling is the DSL's Boolean type keyword.
    def lti_method(name: str, type_spelling: str) -> Method:
        return (
            method(name, I64)
            .raw(f"load.type.info IR2, {type_spelling}")
            .ret(IR2)
        )

    return [
        lti_method("test_lti_rec", "mem:Rec@rec"),
        lti_method("test_lti_i64", "I64"),
        lti_method("test_lti_u8", "U8"),
        lti_method("test_lti_f64", "F64"),
        lti_method("test_lti_bool", "Bool"),
        lti_method("test_lti_arr", MOCK_REC_ARRAY),
    ]


# The copy round-trip: three newobj-allocated objects (source, destination, guard,
# adjacent in the mock heap). The source header word is clobbered with the seed
# (copy.local needs no live TI in it - the size comes from the bytecode-embedded
# synthetic TI), the payload word carries the second seed, the guard header a
# distinct constant. After the copy: the destination header equals the clobbered
# seed (bytes [0, 8) were copied), the destination payload equals its seed (bytes
# [8, 16) were copied), the guard stays intact (nothing beyond 16 was written).
# The guarded copy line is spelled by the caller; `ti` selects the .g form, which
# takes the same registry TI from a load.type.info register.
MOCK_COPY_GUARD_SEED = 0xFEED


def copy_method(name: str, copy_line: str, ti: bool) -> Method:
    builder = (
        method(name, I64, params=(I64, I64))
        .live(IR1, IR2)
        .instr("mov.64", IR5, IR1)
        .instr("mov.64", IR6, IR2)
        .dead(IR1, IR2)
    )
    if ti:
        builder = builder.raw("load.type.info IR10, mem:Rec@rec")
    builder = (
        builder
        .raw("newobj mem:Rec@rec")
        .raw("mov.ref IR2, IR1")
        .dead(IR1)
        .raw("newobj mem:Rec@rec")
        .raw("mov.ref IR3, IR1")
        .dead(IR1)
        .raw("newobj mem:Rec@rec")
        .raw("mov.ref IR4, IR1")
        .dead(IR1)
        .instr("movi.64", IR7, imm(MOCK_COPY_GUARD_SEED))
        .instr("st.raw.64", IR5, IR2, imm(0))
        .instr("st.raw.64", IR6, IR2, imm(8))
        .instr("st.raw.64", IR7, IR4, imm(0))
        .dead(IR2, IR3)
        .live_rec(IR2, IR3)
        .raw(copy_line)
        .instr("ld.raw.64", IR8, IR3, imm(0))
        .instr("scc.64", "EQ", IR8, IR8, IR5)
        .instr("ld.raw.64", IR9, IR3, imm(8))
        .instr("scc.64", "EQ", IR9, IR9, IR6)
        .instr("ld.raw.64", IR1, IR4, imm(0))
        .instr("scc.64", "EQ", IR1, IR1, IR7)
        .instr("mul.64", IR8, IR8, IR9)
        .instr("mul.64", IR8, IR8, IR1)
        .dead(IR2, IR3, IR4, IR5, IR6, IR7)
    )
    if ti:
        builder = builder.dead(IR10)
    return builder.ret(IR8)


def copy_methods() -> list[Method]:
    return [
        copy_method("test_copy_local", "copy.local IR3, IR2, mem:Rec@rec", ti=False),
        copy_method("test_copy_local_g", "copy.local.g IR3, IR2, IR10", ti=True),
        copy_method("test_copy_to_local", "copy.to.local IR3, IRZ, IR2, mem:Rec@rec", ti=False),
        copy_method("test_copy_to_local_g", "copy.to.local.g IR3, IRZ, IR2, IR10", ti=True),
        copy_method("test_copy_from_local", "copy.from.local IRZ, IR3, IR2, mem:Rec@rec", ti=False),
        copy_method("test_copy_from_local_g", "copy.from.local.g IRZ, IR3, IR2, IR10", ti=True),
    ]


# tag.g over Option<I64>-shaped enums: the option object is newobj-allocated and
# the payload word (behind the header) holds the parameter; the underlying-TI
# register comes from load.type.info (its value is only branched on, and the
# mock's IsReference is constant true). The tag is the payload-word non-nullness
# per the enum kind: Option1 { None, Some } -> RNE (non-null reads Some = 1),
# Option0 { Some, None } -> REQ (null reads None = 1).
MOCK_OPTION_TYPES = """
@type mem:Opt1
  @flags PUBLIC
  @enum_kind Option1
  @enum I64
@end

@type mem:Opt0
  @flags PUBLIC
  @enum_kind Option0
  @enum I64
@end

"""


def tag_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MOCK_OPTION_TYPES.lstrip(
        "\n"
    ) + "@type default\n" + body + "\n@end\n"


def tag_methods() -> list[Method]:
    def tag_method(name: str, option: str) -> Method:
        # newobj always defines IR1, so the payload param must move aside first.
        # The option type defs carry no RECORD flag, so their spelling is @ref
        # (a @rec term would fail the reference/record resolution check).
        return (
            method(name, I64, params=(I64,))
            .live(IR1)
            .instr("mov.64", IR2, IR1)
            .dead(IR1)
            .raw(f"newobj mem:{option}@ref")
            .instr("st.raw.64", IR2, IR1, imm(8))
            .raw("load.type.info IR3, I64")
            .raw(f"tag.g IR4, IR1, IR3, mem:{option}@ref")
            .dead(IR1, IR2, IR3)
            .ret(IR4)
        )

    return [
        tag_method("test_tag_opt1", "Opt1"),
        tag_method("test_tag_opt0", "Opt0"),
    ]


MOCK_INTF_TYPES = """
@type mem:Intf
  @flags PUBLIC INTERFACE
  @method foo(I64)I64
    @flags PUBLIC ABSTRACT VIRTUAL
  @end
@end

@type mem:Impl
  @interfaces mem:Intf@ref
  @method impl_foo(I64)I64
    @flags PUBLIC VIRTUAL
    @code
      @live.prim IR1
      load.type.info IR3, mem:Impl@ref
      ld.raw.64 IR4, IR1, 0
      scc.64 EQ, IR4, IR4, IR3
      ld.raw.64 IR2, IR1, 8
      addi.64 IR2, IR2, 0x1
      mul.64 IR2, IR2, IR4
      @dead IR1, IR3, IR4
      mov.64 IR1, IR2
      @dead IR2
      ret.64 IR1
    @end
  @end
@end

@method_ref intf_foo = mem:Intf@ref foo(I64)I64

"""


def interf_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MOCK_INTF_TYPES.lstrip(
        "\n"
    ) + "@type default\n" + body + "\n@end\n"


def interf_methods() -> list[Method]:
    # The receiver is staged as a plain I64 param (flagless method refs resolve
    # with the STATIC abi kind); the result overwrites the receiver register, so
    # it needs the @dead + @live.prim restamp after the call.
    return [
        method("main", I64, params=(I64,))
            .live(IR1)
            .instr("mov.64", IR2, IR1)
            .dead(IR1)
            .raw("newobj mem:Impl@ref")
            .instr("st.raw.64", IR2, IR1, imm(8))
            .raw("call.interf #intf_foo, IR1")
            .dead(IR1)
            .live(IR1)
            .ret(IR1),
    ]


# new.none.g/new.some.g (todo-mock 3.4): both lower to NEWOBJ_ACC_G - the
# engine's ACC-form allocation, whose result lands in IR_ACC (the real thunk
# stores it there; InitNoneGenericOpt0/InitSomeGeneric read their write base
# from IR_ACC) - plus a final Mov(dst, IR_ACC), so the mock's NewObjectAcc
# thunk must put the object into IR_ACC for the flows to work at all. The
# option/underlying TI registers hold the registry TIs via load.type.info;
# new.some.g's src is the raw payload value (the mock's IsReference is constant
# true, so InitSomeGeneric stores it behind the header via WriteObjectInstance
# and InitNoneGenericOpt0 is a no-op). The fresh allocations are deterministic
# (the mock heap rewinds per run), so the same method run through the
# IR_ACC-reading mock entry point (InterpretAccRes) returns the very pointer
# the dst-reading run sees.

def new_none_ptr_method(option: str) -> Method:
    # Returns the fresh None object: the C++ test checks the option TI in the
    # header and the null payload word behind it.
    return (
        method(f"test_none_{option.lower()}", I64)
        .raw("load.type.info IR3, I64")
        .raw(f"load.type.info IR4, mem:{option}@ref")
        .raw(f"new.none.g IR1, IR3, IR4, mem:{option}@ref")
        .dead(IR3, IR4)
        .raw("ret.ref IR1")
        .build()
    )


def new_none_tag_method(option: str) -> Method:
    # The fresh None object read through tag.g: Option1 { None, Some } (RNE)
    # reads the null payload as 0, Option0 { Some, None } (REQ) as 1 - the
    # none.g objects are genuinely None-valued for both enum kinds.
    return (
        method(f"test_none_tag_{option.lower()}", I64)
        .raw("load.type.info IR3, I64")
        .raw(f"load.type.info IR4, mem:{option}@ref")
        .raw(f"new.none.g IR1, IR3, IR4, mem:{option}@ref")
        .raw(f"tag.g IR5, IR1, IR3, mem:{option}@ref")
        .dead(IR1, IR3, IR4)
        .ret(IR5)
    )


def new_some_ptr_method(option: str) -> Method:
    # Returns the fresh Some object with the payload parameter stored behind
    # the header; the C++ test reads the payload word back raw.
    return (
        method(f"test_some_{option.lower()}", I64, params=(I64,))
        .live(IR1)
        .instr("mov.64", IR2, IR1)
        .dead(IR1)
        .raw("load.type.info IR3, I64")
        .raw(f"load.type.info IR4, mem:{option}@ref")
        .raw(f"new.some.g IR1, IR2, IR3, IR4, mem:{option}@ref")
        .dead(IR2, IR3, IR4)
        .raw("ret.ref IR1")
        .build()
    )


def new_some_payload_method(option: str) -> Method:
    # The full round-trip: the some.g payload is read back in-asm through
    # payload.g (its LEA_GENERIC of ordinal 1 folds to the payload word behind
    # the header over the overlapping option TI).
    return (
        method(f"test_some_payload_{option.lower()}", I64, params=(I64,))
        .live(IR1)
        .instr("mov.64", IR2, IR1)
        .dead(IR1)
        .raw("load.type.info IR3, I64")
        .raw(f"load.type.info IR4, mem:{option}@ref")
        .raw(f"new.some.g IR1, IR2, IR3, IR4, mem:{option}@ref")
        .dead(IR2)
        .raw(f"payload.g IR5, IR1, IR3, IR4, mem:{option}@ref")
        .dead(IR1, IR3, IR4)
        .raw("ret.ref IR5")
        .build()
    )


def new_option_methods() -> list[Method]:
    return [
        new_none_ptr_method("Opt0"),
        new_none_ptr_method("Opt1"),
        new_none_tag_method("Opt0"),
        new_none_tag_method("Opt1"),
        new_some_ptr_method("Opt0"),
        new_some_ptr_method("Opt1"),
        new_some_payload_method("Opt0"),
        new_some_payload_method("Opt1"),
    ]


def generate_mock_objects() -> list[Path]:
    return write_suite(
        "mock-objects",
        {
            "newobj.asm"        : mock_obj_file(newobj_methods()),
            "newarr.asm"        : mock_obj_file(newarr_methods()),
            "index.asm"         : mock_obj_file(index_methods()),
            "load-type-info.asm": mock_obj_file(load_type_info_methods()),
            "copy.asm"          : mock_obj_file(copy_methods()),
            "tag.asm"           : tag_file(tag_methods()),
            "new-option.asm"    : tag_file(new_option_methods()),
            "interf.asm"        : interf_file(interf_methods()),
        },
    )


# ---------------------------------------------------------------------------
# mock-lea: lea family / offset / movbp / lea.static / payload.g over the mock base pointers
# ---------------------------------------------------------------------------

# todo-mock 3.3: GetFieldOffset, GetGlobalBasePtr/GetLocalBasePtr and
# DerivedPtrOfGlobal return deterministic mock-owned values instead of FATALing,
# so the lea family, offset/add.offset, movbp.local/.global, lea.static and
# payload.g rewrite and execute over the mock. The addressing identities the
# generated methods rely on:
# - lea/lea.g/offset/add.offset of the slot-aligned I64 rec fields stay
#   cell-relative (ordinal * 8) and of the class fields carry the object header
#   on top (ordinal * 8 + ObjectHeaderSize) - exactly the ld.field/st.field
#   offsets of the same fields;
# - lea.g/offset take the container TI in a register (load.type.info); the
#   non-overlapping synthetic shape folds GetFieldOffset to ordinal * 8, while
#   payload.g relies on the overlapping option TI (every ordinal folds to 0)
#   plus the header addend, so its address is the payload word behind the header;
# - movbp.local/.global and lea.static's base-ref register expose the two
#   distinct mock-owned base areas (zeroed by InitializeMockInterpreter,
#   inspectable via MockLocalBaseArea()/MockGlobalBaseArea()); the generic
#   loads/stores address through the derived pointer alone (the mock ignores
#   the base register), so pre-seeded words are seeded/inspected at the exact
#   derived address C++-side;
# - lea.static's address register holds the engine static-slot address (the
#   LOAD_ADDR LD_LEA execution runs it through DerivedPtrOfGlobal), so the raw
#   store through it must read back through ld.static.
# Two liveness quirks shape the bodies: movbp.local/.global leave their
# destination unmarked in the assembler's liveness analysis (the result is
# stamped with an explicit @live before use), and lea.static defines the base
# register with the REF mark (returned via ret.ref) and the address register
# with the PRIM one.

MOCK_LEA_FIELD_REFS = """
@field_ref rec_f0 = mem:Rec@rec f0 I64
@field_ref rec_f1 = mem:Rec@rec f1 I64
@field_ref obj_x = mem:Obj@ref x I64
@field_ref obj_y = mem:Obj@ref y I64
"""

MOCK_LEA_TYPES = """
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
  @field f1 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
  @field y I64
    @flags PUBLIC
  @end
@end

"""

MOCK_LEA_STATICS = """
@field_ref stat0 = default@ref s0 I64
@field_ref stat1 = default@ref s1 I64

@type default
  @field s0 I64
    @flags STATIC
  @end
  @field s1 I64
    @flags STATIC
  @end

"""


def mock_lea_field_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MOCK_LEA_FIELD_REFS.lstrip("\n") + MOCK_LEA_TYPES.lstrip(
        "\n"
    ) + "@type default\n" + body + "\n@end\n"


def mock_lea_bare_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n\n@type default\n' + body + "\n@end\n"


def mock_lea_static_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MOCK_LEA_STATICS.lstrip("\n") + body + "\n@end\n"


def mock_lea_option_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return '@main_type "default"\n' + MOCK_OPTION_TYPES.lstrip(
        "\n"
    ) + "@type default\n" + body + "\n@end\n"


def lea_addr_method(name: str, field: str, rec: bool) -> Method:
    builder = method(f"test_{name}", I64, params=(I64,))
    builder = builder.live_rec(IR1) if rec else builder.live_ref(IR1)
    return builder.instr("lea", IR2, IR1, f"#{field}").dead(IR1).ret(IR2)


def lea_rt_method(name: str, field: str, rec: bool) -> Method:
    # The raw store/load through the lea'd pointer must byte-match ld.field of
    # the same field.
    builder = method(f"test_{name}", I64, params=(I64, I64))
    builder = builder.live_rec(IR1) if rec else builder.live_ref(IR1)
    return (
        builder
        .live(IR2)
        .instr("lea", IR3, IR1, f"#{field}")
        .instr("st.raw.64", IR2, IR3, imm(0))
        .instr("ld.field", IR4, IR1, f"#{field}")
        .instr("ld.raw.64", IR5, IR3, imm(0))
        .instr("scc.64", "EQ", IR6, IR4, IR2)
        .instr("scc.64", "EQ", IR7, IR5, IR2)
        .instr("mul.64", IR6, IR6, IR7)
        .dead(IR1, IR2, IR3, IR4, IR5, IR7)
        .ret(IR6)
    )


def lea_g_addr_method(name: str, field: str, owner: str) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64,))
        .live_ref(IR1)
        .raw(f"load.type.info IR2, {owner}")
        .instr("lea.g", IR3, IR1, IR2, f"#{field}")
        .dead(IR1, IR2)
        .ret(IR3)
    )


def lea_g_rt_method(name: str, field: str, owner: str, rec: bool) -> Method:
    # lea.g demands the REF mark on the base while ld.field of a rec field
    # demands REC, so the rec bodies re-stamp the base with the @dead +
    # @live.rec dance between the two ops.
    builder = (
        method(f"test_{name}", I64, params=(I64, I64))
        .live_ref(IR1)
        .live(IR2)
        .raw(f"load.type.info IR3, {owner}")
        .instr("lea.g", IR4, IR1, IR3, f"#{field}")
    )
    if rec:
        builder = builder.dead(IR1).live_rec(IR1)
    return (
        builder
        .instr("st.raw.64", IR2, IR4, imm(0))
        .instr("ld.field", IR5, IR1, f"#{field}")
        .instr("ld.raw.64", IR6, IR4, imm(0))
        .instr("scc.64", "EQ", IR7, IR5, IR2)
        .instr("scc.64", "EQ", IR8, IR6, IR2)
        .instr("mul.64", IR7, IR7, IR8)
        .dead(IR1, IR2, IR3, IR4, IR5, IR6, IR8)
        .ret(IR7)
    )
    return builder.dead(IR1, IR2, IR3, IR4, IR5, IR6, IR8).ret(IR7)


def offset_method(name: str, field: str, owner: str) -> Method:
    # The offset op reads its TI register at run time only (the analyzer never
    # marks it), so load.type.info must precede it.
    return (
        method(f"test_{name}", I64)
        .raw(f"load.type.info IR2, {owner}")
        .instr("offset", IR1, f"#{field}", IR2)
        .ret(IR1)
    )


def add_offset_method(name: str, field: str, owner: str) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64,))
        .live(IR1)
        .raw(f"load.type.info IR2, {owner}")
        .instr("add.offset", IR1, f"#{field}", IR2)
        .ret(IR1)
    )


def add_offset_rt_method(name: str, field: str, owner: str) -> Method:
    # add.offset needs its destination PRIM-marked (usePrim on the in-place
    # accumulate), so the container base rides in a plain register and the
    # round-trip goes through the raw memory ops only.
    return (
        method(f"test_{name}", I64, params=(I64, I64))
        .live(IR1, IR2)
        .raw(f"load.type.info IR3, {owner}")
        .instr("add.offset", IR1, f"#{field}", IR3)
        .instr("st.raw.64", IR2, IR1, imm(0))
        .instr("ld.raw.64", IR4, IR1, imm(0))
        .instr("scc.64", "EQ", IR5, IR4, IR2)
        .dead(IR1, IR2, IR3, IR4)
        .ret(IR5)
    )


def movbp_value_method(name: str, local: bool) -> Method:
    return (
        method(f"test_{name}", I64)
        .raw(f"movbp.{'local' if local else 'global'} IR1")
        .live(IR1)
        .ret(IR1)
    )


def movbp_distinct_method() -> Method:
    return (
        method("test_movbp_distinct", I64)
        .raw("movbp.local IR1")
        .live(IR1)
        .raw("movbp.global IR2")
        .live(IR2)
        .instr("scc.64", "NE", IR3, IR1, IR2)
        .dead(IR1, IR2)
        .ret(IR3)
    )


def bp_generic_read_method(name: str, local: bool) -> Method:
    # Reads the pre-seeded word at the derived address (a test-provided prim)
    # through movbp + ld.g; the TypeInfo register is a plain constant like in
    # the mem-obj generic test.
    return (
        method(f"test_{name}", I64, params=(I64,))
        .live(IR1)
        .raw(f"movbp.{'local' if local else 'global'} IR2")
        .live_ref(IR2)
        .instr("movi", IR9, imm(0x1000), w=64)
        .instr("ld.g", IR4, IR2, IR1, IR9)
        .dead(IR1, IR2, IR9)
        .raw("ret.ref IR4")
        .build()
    )


def bp_generic_write_method(name: str, local: bool) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64, I64))
        .live(IR1)
        .live_ref(IR2)
        .raw(f"movbp.{'local' if local else 'global'} IR3")
        .live_ref(IR3)
        .instr("movi", IR9, imm(0x1000), w=64)
        .instr("st.g", IR2, IR3, IR1, IR9)
        .instr("ld.g", IR5, IR3, IR1, IR9)
        .dead(IR1, IR2, IR3, IR9)
        .raw("ret.ref IR5")
        .build()
    )


def lea_static_base_method() -> Method:
    return (
        method("test_lea_static_base", I64)
        .raw("lea.static IR1, IR2, #stat0")
        .raw("ret.ref IR2")
        .build()
    )


def lea_static_rt_method(name: str, field: str) -> Method:
    return (
        method(f"test_{name}", I64, params=(I64,))
        .live(IR1)
        .raw(f"lea.static IR2, IR3, #{field}")
        .dead(IR3)
        .instr("st.raw.64", IR1, IR2, imm(0))
        .instr("ld.static", IR4, f"#{field}")
        .instr("scc.64", "EQ", IR5, IR4, IR1)
        .dead(IR1, IR2, IR4)
        .ret(IR5)
    )


def static_pair_method() -> Method:
    return (
        method("test_static_pair", I64, params=(I64, I64))
        .live(IR1, IR2)
        .instr("st.static", IR1, "#stat0")
        .instr("st.static", IR2, "#stat1")
        .instr("ld.static", IR3, "#stat1")
        .instr("ld.static", IR4, "#stat0")
        .instr("scc.64", "EQ", IR5, IR3, IR2)
        .instr("scc.64", "EQ", IR6, IR4, IR1)
        .instr("mul.64", IR5, IR5, IR6)
        .dead(IR1, IR2, IR3, IR4, IR6)
        .ret(IR5)
    )


def payload_raw_method(name: str, option: str) -> Method:
    # st.raw.64 seeds the payload word behind the header; payload.g must read
    # exactly that word.
    return (
        method(f"test_{name}", I64, params=(I64,))
        .live(IR1)
        .instr("mov.64", IR2, IR1)
        .dead(IR1)
        .raw(f"newobj mem:{option}@ref")
        .instr("st.raw.64", IR2, IR1, imm(8))
        .raw("load.type.info IR3, I64")
        .raw(f"load.type.info IR4, mem:{option}@ref")
        .raw(f"payload.g IR5, IR1, IR3, IR4, mem:{option}@ref")
        .dead(IR1, IR2, IR3, IR4)
        .raw("ret.ref IR5")
        .build()
    )


def payload_generic_write_method(name: str, option: str) -> Method:
    # The reverse direction: the payload word is written through the generic
    # path (st.g with the derived address = lea.box(obj) = obj + 8) and read
    # back through payload.g.
    return (
        method(f"test_{name}", I64, params=(I64,))
        .live_ref(IR1)
        .raw("mov.ref IR2, IR1")
        .dead(IR1)
        .raw(f"newobj mem:{option}@ref")
        .instr("lea.box", IR3, IR1)
        .raw("load.type.info IR4, I64")
        .instr("st.g", IR2, IR1, IR3, IR4)
        .raw(f"load.type.info IR5, mem:{option}@ref")
        .raw(f"payload.g IR6, IR1, IR4, IR5, mem:{option}@ref")
        .dead(IR1, IR2, IR3, IR4, IR5)
        .raw("ret.ref IR6")
        .build()
    )


def generate_mock_lea() -> list[Path]:
    written = write_suite(
        "mock-lea",
        {
            "lea.asm": mock_lea_field_file(
                [
                    lea_addr_method("lea_rec_f0", "rec_f0", rec=True),
                    lea_addr_method("lea_rec_f1", "rec_f1", rec=True),
                    lea_addr_method("lea_obj_x", "obj_x", rec=False),
                    lea_addr_method("lea_obj_y", "obj_y", rec=False),
                    lea_rt_method("lea_rec_rt", "rec_f0", rec=True),
                    lea_rt_method("lea_obj_rt", "obj_x", rec=False),
                ]
            ),
            "lea-g.asm": mock_lea_field_file(
                [
                    lea_g_addr_method("leag_rec_f0", "rec_f0", "mem:Rec@rec"),
                    lea_g_addr_method("leag_rec_f1", "rec_f1", "mem:Rec@rec"),
                    lea_g_addr_method("leag_obj_x", "obj_x", "mem:Obj@ref"),
                    lea_g_addr_method("leag_obj_y", "obj_y", "mem:Obj@ref"),
                    lea_g_rt_method("leag_rec_rt", "rec_f0", "mem:Rec@rec", rec=True),
                    lea_g_rt_method("leag_obj_rt", "obj_x", "mem:Obj@ref", rec=False),
                ]
            ),
            "offset.asm": mock_lea_field_file(
                [
                    offset_method("offset_rec_f0", "rec_f0", "mem:Rec@rec"),
                    offset_method("offset_rec_f1", "rec_f1", "mem:Rec@rec"),
                    offset_method("offset_obj_x", "obj_x", "mem:Obj@ref"),
                    offset_method("offset_obj_y", "obj_y", "mem:Obj@ref"),
                    add_offset_method("add_offset_rec_f1", "rec_f1", "mem:Rec@rec"),
                    add_offset_method("add_offset_obj_x", "obj_x", "mem:Obj@ref"),
                    add_offset_rt_method("add_offset_rec_rt", "rec_f1", "mem:Rec@rec"),
                    add_offset_rt_method("add_offset_obj_rt", "obj_x", "mem:Obj@ref"),
                ]
            ),
            "movbp.asm": mock_lea_bare_file(
                [
                    movbp_value_method("movbp_local", local=True),
                    movbp_value_method("movbp_global", local=False),
                    movbp_distinct_method(),
                    bp_generic_read_method("movbp_local_read", local=True),
                    bp_generic_read_method("movbp_global_read", local=False),
                    bp_generic_write_method("movbp_local_write", local=True),
                    bp_generic_write_method("movbp_global_write", local=False),
                ]
            ),
            "lea-static.asm": mock_lea_static_file(
                [
                    lea_static_base_method(),
                    lea_static_rt_method("lea_static_rt", "stat0"),
                    static_pair_method(),
                ]
            ),
            "payload.asm": mock_lea_option_file(
                [
                    payload_raw_method("payload_raw_opt1", "Opt1"),
                    payload_raw_method("payload_raw_opt0", "Opt0"),
                    payload_generic_write_method("payload_gen_opt1", "Opt1"),
                    payload_generic_write_method("payload_gen_opt0", "Opt0"),
                ]
            ),
        },
    )
    return written


# ---------------------------------------------------------------------------
# mock-throw: throw/catch minimal flow over the mocked HandleException
# ---------------------------------------------------------------------------

# todo-mock 3.5: `throw irX` lowers to the RT THROW (FATALs on a null operand
# and returns the ThunkOp::HandleException thunk carrying the exception object,
# with the reader parked right after the throw) and `catch irY` lowers to the
# RT CATCH (reads IR_ACC - where the real trampoline's handler-found leg parks
# the exception object - into irY, falling through only when it is non-null).
# The mock models that leg unconditionally and has no exception regions, so the
# catch pad is placed immediately after the throw in these images; the pending
# exception is published through GetPendingException. Liveness quirks: throw
# uses its operand with the REF mark and catch defines its destination with
# REF, so the destination must be @dead'ed first (a live re-definition aborts
# the assembler); scc operands are USE_PRIM (reference identity goes through
# bcc REQ/RNE), while raw loads/stores accept either mark on the base.

MOCK_THROW_EXC_TYPES = """
@type mem:Exc
  @field x I64
    @flags PUBLIC
  @end
@end

"""


def mock_throw_file(methods: list[Method]) -> str:
    body = "\n\n".join(m.render() for m in methods)
    return ('@main_type "default"\n' + MOCK_THROW_EXC_TYPES.lstrip("\n") +
            "@type default\n" + body + "\n@end\n")


def catch_flag_method() -> Method:
    # The full in-asm round-trip over registers kept live across the throw: the
    # payload seeded before the throw reads back after the catch (IR2 survived),
    # the caught object is the thrown one (IR3 survived, bcc REQ identity).
    return (
        method("test_catch", I64, params=(I64,))
        .live(IR1)
        .instr("mov.64", IR2, IR1)
        .dead(IR1)
        .raw("newobj mem:Exc@ref")
        .raw("mov.ref IR3, IR1")
        .instr("st.raw.64", IR2, IR1, imm(8))
        .raw("throw IR1")
        .dead(IR1, IR3)
        .raw("catch IR1")
        .live_ref(IR3)
        .instr("movi.64", IR6, imm(1))
        .instr("ld.raw.64", IR5, IR1, imm(8))
        .instr("scc.64", "EQ", IR7, IR5, IR2)
        .instr("mul.64", IR6, IR6, IR7)
        .instr("bcc", "REQ", IR1, IR3, "ok", w=64)
        .instr("subi.64", IR6, IR6, imm(1))
        .label("ok")
        .dead(IR1, IR2, IR3, IR5, IR7)
        .ret(IR6)
    )


def caught_ptr_method() -> Method:
    # Returns the caught object: the C++ test checks its header/payload and
    # that it is still parked in IR_ACC (CATCH reads IR_ACC and the lowering
    # moves it nowhere else before the ret).
    return (
        method("test_caught_ptr", I64, params=(I64,))
        .live(IR1)
        .instr("mov.64", IR2, IR1)
        .dead(IR1)
        .raw("newobj mem:Exc@ref")
        .instr("st.raw.64", IR2, IR1, imm(8))
        .raw("throw IR1")
        .dead(IR1, IR2)
        .raw("catch IR1")
        .raw("ret.ref IR1")
        .build()
    )


def throw_param_method() -> Method:
    # The exception object fabricated outside the mock heap (a test-seeded
    # buffer arriving as the parameter): the catch returns it unchanged.
    return (
        method("test_throw_param", I64, params=(I64,))
        .live_ref(IR1)
        .raw("mov.ref IR3, IR1")
        .instr("movi.64", IR6, imm(1))
        .raw("throw IR1")
        .dead(IR1, IR3)
        .raw("catch IR1")
        .live_ref(IR3)
        .instr("bcc", "REQ", IR1, IR3, "ok", w=64)
        .instr("subi.64", IR6, IR6, imm(1))
        .label("ok")
        .dead(IR1, IR3)
        .ret(IR6)
    )


def generate_mock_throw() -> list[Path]:
    return write_suite(
        "mock-throw",
        {
            "catch.asm": mock_throw_file([catch_flag_method(), caught_ptr_method(), throw_param_method()]),
        },
    )


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
    "alu-rr"           : generate_alu_rr,
    "frame-slots"      : generate_frame_slots,
    "mem-obj"          : generate_mem_obj,
    "atomics"          : generate_atomics,
    "checks-misc"      : generate_checks_misc,
    "calls"            : generate_calls,
    "mock-objects"     : generate_mock_objects,
    "mock-lea"         : generate_mock_lea,
    "mock-throw"       : generate_mock_throw,
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
