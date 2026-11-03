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


def reg_for(t: AsmType) -> str:
    if t.is_float:
        return FR0
    return IR1


def imm(value: int) -> str:
    return format(value, "#x")


def live(*regs: str) -> str:
    return "@live.prim " + ", ".join(regs)


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

    def render(self) -> str:
        lines = "\n".join(f"      {line}" for line in self.body)
        params = ", ".join(self.params)
        return (f"  @method {self.name}({params}){self.ret}\n"
                f"    @code\n{lines}\n"
                f"    @end\n  @end")


ASM_FILE_HEADER = '@main_type "default"\n\n@type default\n'


def asm_file(methods: list[Method]) -> str:    
    return ASM_FILE_HEADER + "\n\n".join(m.render() for m in methods) + "\n@end\n"


class MethodBuilder:

    def __init__(self, name: str, ret: AsmType, params: tuple[AsmType, ...] = (),
                 default_w: int | None = None):
        self._name = name
        self._ret = ret
        self._params = params
        self._lines: list[str] = []
        self._default_w = default_w

    def live(self, *regs: str) -> "MethodBuilder":
        self._lines.append(live(*regs))
        return self

    def instr(self, mnemonic: str, *operands: object,
              suffixes: tuple[object, ...] = ()) -> "MethodBuilder":
        w = self._default_w
        if suffixes:
            w = None
        self._lines.append(instr(mnemonic, *operands, w=w, suffixes=suffixes))
        return self

    def _close(self, mnemonic: str, reg: str) -> Method:
        w = int(self._ret[1:])
        return Method(self._name, self._ret, tuple(self._lines) + (instr(mnemonic, reg, w=w),),
                      self._params)

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


# ---------------------------------------------------------------------------

GENERATORS = {
    "arith"            : generate_arith,
    "float-arith"      : generate_float_arith,
    "cast"             : generate_cast,
    "bfx"              : generate_bfx,
    "arith-specialized": generate_arith_specialized,
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
