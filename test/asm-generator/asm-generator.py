#!/usr/bin/env python3
"""Generates .asm test resources for the cbc-engine tests.

Usage:
    python3 asm-generator.py <generator-name>

Each named generator overwrites resource files in test/resources/generated-asm/<testsuite-name>/. 
"""

import argparse
import shutil
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
IR2 = "IR2"
FR0 = "FR0"
FR1 = "FR1"


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

    def instr(self, mnemonic: str, *operands: object,
              suffixes: tuple[object, ...] = ()) -> "MethodBuilder":
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

FLOAT_UNARY_OPS = ["NEG", "SQRT", "ABS", "SIN", "COS", "LOG", "CEIL", "FLOOR", "LOG10", "LOG2", "EXP", "EXP2", "TRUNC"]
FLOAT_BINARY_OPS = ["ADD", "SUB", "MUL", "DIV", "POW"]


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


def float_powi_method(width: int) -> Method:
    """fpowi: FR0 = FR1 raised to the signed I64 exponent in IR1 (the base is
    F32/F64, the exponent stays I64 in both widths)."""
    float_type = AsmType(f"F{width}")
    return (
        method("test_FPOWI", float_type, params=(float_type, I64), default_w=width)
        .live(IR1)
        .instr("fpowi", FR0, FR1, IR1)
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
        files["FPOWI.asm"] = asm_file([float_powi_method(width)])
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
# checked-arith: pipeline-smoke images for CbcCheckedArithAsm.
# Operand cartesian lives in the Emitter suite (checked_arith_emit_test.cpp);
# these files only prove assembler -> rewriter -> interpreter still works.
#
# Layout:
#   checked-arith/rr/<op>.asm         - test_<w> methods, register form
#   checked-arith/imm_short/<op>i.asm - test_<w> methods, short imm 0x1
#   checked-arith/imm_wide/<op>i.asm  - test_<w> methods, wide imm 0x1000
#                                      (no W8: compiler never emits it)
# ---------------------------------------------------------------------------

CHECKED_WIDTHS = [8, 16, 32, 64]


CHECKED_OPS = [
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
    "sdiv",
    "smod",
    "spow",
    "sulsr",
]


def checked_imm_method(op: str, w: int, rhs: int) -> Method:
    return (
        method(f"test_{w}", I64, params=(I64,), default_w=w)
        .live(IR1)
        .instr(op, IR1, IR1, imm(rhs))
        .ret()
    )


def checked_rr_method(op: str, w: int) -> Method:
    return (
        method(f"test_{w}", I64, params=(I64, I64), default_w=w)
        .live(IR1, IR2)
        .instr(op, IR1, IR1, IR2)
        .ret()
    )


def generate_checked_arith() -> list[Path]:
    directory = GENERATED_ASM / "checked-arith"
    if directory.exists():
        shutil.rmtree(directory)
    rr_files = {}
    short_files = {}
    wide_files = {}
    for op in CHECKED_OPS:
        rr_files[f"{op}.asm"] = asm_file([checked_rr_method(op, w) for w in CHECKED_WIDTHS])
        short_files[f"{op}i.asm"] = asm_file([checked_imm_method(op + "i", w, 0x1) for w in CHECKED_WIDTHS])
        wide_files[f"{op}i.asm"] = asm_file(
            [checked_imm_method(op + "i", w, 0x1000) for w in CHECKED_WIDTHS if w != 8]
        )
    written = write_suite("checked-arith/rr", rr_files)
    written += write_suite("checked-arith/imm_short", short_files)
    written += write_suite("checked-arith/imm_wide", wide_files)
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
