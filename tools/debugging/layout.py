"""Layout descriptors: frame slots, function handles, bytecode info."""

from api import Arch, Debugger, Frame, TypeKind, map_opt
from typing import Any, Callable, Optional, TypeVar, overload

import gdb

T = TypeVar("T")

class Field:
    def __init__(self, offset: int | Callable[[Debugger], int], tk: TypeKind) -> None:
        if isinstance(offset, int):
            offs = offset
            get_offs = lambda _: 0
        else:
            offs = 0
            get_offs = offset
        self.offset = offs
        self.get_offset = get_offs
        self.tk = tk

    @overload
    def __get__(self, instance: None, owner: type) -> "Field": ...

    @overload
    def __get__(self, instance: Any, owner: type | None = None) -> int: ...

    def __get__(self, instance: Any, owner: type | None = None) -> Any:
        raise Exception(f"{instance} of {owner} must be processed by @with_layout")


def with_layout(c: type[T]) -> type[T]:
    for attr, value in c.__dict__.items():
        if isinstance(value, Field):
            def read(t, v: Field):
                dbg = t.dbg
                ptr = t.ptr
                assert isinstance(dbg, Debugger)
                assert isinstance(ptr, int)
                return dbg.read(ptr + v.offset + v.get_offset(dbg), v.tk)
            setattr(c, attr, property(lambda self, v=value: read(self, v)))
    return c


@with_layout
class ExecBytecodeInfo:
    bc_size           = Field(0x0, TypeKind.U64)
    start             = Field(0x8, TypeKind.U64)
    frame_size        = Field(0x30, TypeKind.U32)
    static_frame_size = Field(0x34, TypeKind.U32)

    def __init__(self, dbg: Debugger, ptr: int) -> None:
        self.ptr = ptr
        self.dbg = dbg

    @property
    def end(self) -> int:
        return self.start + self.bc_size

    def __repr__(self) -> str:
        return (f"ExecBytecodeInfo(ptr=0x{self.ptr:x}, "
                f"start=0x{self.start:x}, bc_size=0x{self.bc_size:x}, "
                f"frame_size={self.frame_size}, static_frame_size={self.static_frame_size})")


@with_layout
class DynamicFunctionHandle:
    bytecode_ptr = Field(0x10, TypeKind.U64)
    mdef         = Field(0x18, TypeKind.U64)
    mdef_name    = Field(0x20, TypeKind.U64)

    def __init__(self, dbg: Debugger, ptr: int) -> None:
        self.ptr = ptr
        self.dbg = dbg

    @property
    def bytecode(self) -> Optional[ExecBytecodeInfo]:
        return map_opt(self.bytecode_ptr, lambda x: ExecBytecodeInfo(self.dbg, x))

    @property
    def name(self) -> str:
        addr = self.mdef_name
        if not addr:
            return "<unknown>"
        return self.dbg.read_cstring(addr) or "<unreadable>"

    def __repr__(self) -> str:
        return f"DynamicFunctionHandle(addr=0x{self.ptr:x}, mdef=0x{self.mdef:x})"


def _cursor_pos(dbg: Debugger) -> int:
    match dbg.arch:
        case Arch.X64: return -0x40
        case Arch.AArch64: return -0x38

@with_layout
class FrameLayout:
    saved_fp_ptr = Field(-0x0, TypeKind.U64)
    dyn_fuh_ptr  = Field(-0x8, TypeKind.U64)
    cursor       = Field(_cursor_pos, TypeKind.U64)

    def __init__(self, dbg: Debugger, fp: int, sp: int) -> None:
        assert fp >= sp
        self.fp = fp
        self.ptr = fp
        self.sp = sp
        self.dbg = dbg

    @property
    def dyn_fuh(self) -> Optional[DynamicFunctionHandle]:
        return map_opt(self.dyn_fuh_ptr, lambda x: DynamicFunctionHandle(self.dbg, x))

    @property
    def size(self) -> int:
        return self.fp - self.sp

    def __repr__(self) -> str:
        return f"FrameLayout(fp=0x{self.fp:x}, sp=0x{self.sp:x}, size={self.size})"


def make_int_frame_layout(dbg: Debugger, frame: Frame) -> Optional[FrameLayout]:
    fp = frame.fp
    name = frame.name
    if not name or not name == "perform_2i_call":
        return None
    if not fp:
        return None
    return FrameLayout(dbg, fp, frame.sp)

