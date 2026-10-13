
from dataclasses import dataclass
from functools import cached_property
from typing import Any, Callable, Optional, TypeVar
from enum import Enum, IntEnum

import gdb
import struct


class TypeKind(IntEnum):
    U64 = 0
    U32 = 1
    I64 = 2
    I32 = 3


X = TypeVar("X")
R = TypeVar("R")


def map_opt(x: Optional[X], fn: Callable[[X], R]) -> Optional[R]:
    if x:
        return fn(x)
    return None


def green(s: str) -> str:
    return f"\033[01;32m{s}\033[0m"


def blue(s: str) -> str:
    return f"\033[01;34m{s}\033[0m"


def safe_call(method, frame: gdb.Frame, default=None):
    """Call a stub method, returning `default` if NotImplementedError."""
    try:
        return method(frame)
    except NotImplementedError:
        return default


def debugger() -> "Debugger":
    return Debugger()


class Arch(Enum):
    AArch64 = 0
    X64 = 1


@dataclass
class Debugger:

    @cached_property
    def _inferior(self):
        return gdb.selected_inferior()

    def _read(self, addr: int, sz: int, fmt: str) -> Optional[int]:
        mem = self._inferior.read_memory(addr, sz)
        return struct.unpack(f"<{fmt}", mem)[0]

    def read(self, addr: int, tk: TypeKind) -> Optional[int]:
        try:
            match tk:
                case TypeKind.U64: return self._read(addr, 8, "Q")
                case TypeKind.U32: return self._read(addr, 4, "I")
                case TypeKind.I64: return self._read(addr, 8, "q")
                case TypeKind.I32: return self._read(addr, 4, "i")
        except gdb.MemoryError:
            print(f"failed to read at 0x{addr:x}")
            return None

    def read_cstring(self, addr: int) -> Optional[str]:
        try:
            return gdb.parse_and_eval(f"(char*)0x{addr:x}").string()
        except:
            return None

    def frame(self, n: int) -> Optional["Frame"]:
        frame = gdb.newest_frame()
        for _ in range(n):
            if frame is None:
                return None
            frame = frame.older()
        return map_opt(frame, lambda x: Frame(x, self))

    def selected_frame(self) -> Optional["Frame"]:
        return map_opt(gdb.selected_frame(), lambda x: Frame(x, self))

    def on_exit(self, fn: Callable[[Any], Any]):
        gdb.events.exited.connect(fn)

    def on_lib_load(self, fn: Callable[["NewLibraryEvent"], Any]):
        def wrapper(event):
            fn(NewLibraryEvent(event))
        gdb.events.new_objfile.connect(wrapper)

    @cached_property
    def arch(self) -> Arch:
        arch = self._inferior.architecture()
        name = arch.name()
        if "aarch64" in name:
            return Arch.AArch64
        elif "x86-64" in name or "x86_64" in name:
            return Arch.X64
        raise Exception(f"Unknown architecture {name}")


@dataclass
class Sal:
    _sal: gdb.Symtab_and_line

    @property
    def filename(self) -> Optional[str]:
        if self._sal and self._sal.symtab:
            return self._sal.symtab.filename
        return None

    @property
    def line(self) -> int:
        if self._sal:
            return self._sal.line or -1
        return -1


@dataclass
class Frame:
    gdb_frame: gdb.Frame
    dbg: Debugger

    @property
    def pc(self) -> int:
        return self.gdb_frame.pc()

    @property
    def name(self) -> Optional[str]:
        return self.gdb_frame.name()

    @property
    def sp(self) -> int:
        val = self.gdb_frame.read_register("sp")
        assert val, "failed to get stack pointer"
        return int(val)

    @property
    def fp(self) -> Optional[int]:
        match self.dbg.arch:
            case Arch.X64: reg = "rbp"
            case Arch.AArch64: reg = "x29"
        return map_opt(self.gdb_frame.read_register(reg), int)

    @property
    def older(self) -> Optional["Frame"]:
        try:
            f = self.gdb_frame.older()
        except:
            return None
        return map_opt(f, lambda x: Frame(x, self.dbg))

    @property
    def sal(self) -> Optional[Sal]:
        return map_opt(self.gdb_frame.find_sal(), Sal)


@dataclass
class Symbol:
    _sym: gdb.Symbol

    @property
    def name(self) -> str:
        return self._sym.name

    @property
    def location(self) -> int:
        addr = self._sym.value().address
        assert addr is not None, f"symbol {self._sym.name} is not addressable"
        return int(addr)

    def set(self, value: int) -> None:
        self._sym.value().assign(gdb.Value(value))


@dataclass
class NewLibraryEvent:
    _event: gdb.NewObjFileEvent

    @property
    def filename(self) -> Optional[str]:
        return self._event.new_objfile.filename

    def symbol(self, name: str) -> Optional["Symbol"]:
        sym = self._event.new_objfile.lookup_global_symbol(name)
        return map_opt(sym, Symbol)

