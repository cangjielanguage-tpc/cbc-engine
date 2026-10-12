"""Commands: GDB command classes and frame utilities."""

from api import Debugger, Frame, blue, debugger, green
import gdb
import traceback

from layout import FrameLayout, make_int_frame_layout


def dump_frame(dbg: Debugger, frame: Frame) -> None:
    if frame.fp is None or frame.sp is None:
        print("  cannot read FP/SP")
        return
    layout = FrameLayout(dbg, frame.fp, frame.sp)
    print(f"  {layout}")

    # FuH
    dfh = layout.dyn_fuh
    if dfh is None:
        print(f"    FuH: <unreadable>")
        return
    print(f"    FuH: {dfh}")
    print(f"      name: {dfh.name}")
    try:
        print(f"      mdef: 0x{dfh.mdef:x}")
    except gdb.MemoryError:
        print(f"      mdef: <unreadable>")

    # Bytecode
    try:
        bc = dfh.bytecode
    except gdb.MemoryError:
        bc = None
    if bc is None:
        print(f"      bytecode: <null or unreadable>")
    else:
        try:
            print(f"      bytecode: {bc}")
            print(f"        start: 0x{bc.start:x}")
            print(f"        end:   0x{bc.end:x}")
            print(f"        bc_size: 0x{bc.bc_size:x} ({bc.bc_size})")
            print(f"        frame_size: {bc.frame_size}")
            print(f"        static_frame_size: {bc.static_frame_size}")
        except gdb.MemoryError:
            print(f"      bytecode: 0x{dfh.ptr:x} <unreadable>")
            bc = None

    # bcPos: cursor from Reader slot closest to SP (Reader[2] at fp-64)
    cursor = layout.cursor
    if cursor is not None and bc is not None:
        if bc.start <= cursor < bc.end:
            pos = cursor - bc.start
            print(f"      bcPos: {pos} (0x{pos:x})")
        else:
            print(f"      bcPos: 0x{cursor:x} OUT OF RANGE [0x{bc.start:x}, 0x{bc.end:x})")


class IBtCommand(gdb.Command):
    """`ibt` - prints backtrace + interpreter frame metadata."""

    def __init__(self) -> None:
        super().__init__("ibt", gdb.COMMAND_USER)

    def invoke(self, argument: str, from_tty: bool) -> None:
        dbg = debugger()
        def frames():
            frame = dbg.selected_frame()
            num = 0
            while frame:
                yield num, frame
                num += 1
                frame = frame.older

        for num, frame in frames():
            self._print_frame(num, frame)

    def _print_frame(self, num: int, frame: Frame) -> None:
        dbg = frame.dbg
        try:
            pc = frame.pc
        except gdb.MemoryError:
            print(f"# {num:<2d}  <unavailable>")
            return

        name = frame.name
        sal = frame.sal

        name = name or "??"
        func_str = f"{green(name)}"

        # Source location or shared library
        if sal and sal.filename:
            loc = f"at {blue(f'{sal.filename}:{sal.line}')}"
        else:
            lib = frame.shared_library
            if lib:
                loc = f"from {lib}"
            else:
                loc = ""

        if num == 0 and name is not None and sal and sal.filename:
            # Frame 0: no address prefix, two-line format
            print(f"# {num:<2d}  {func_str}")
            print(f"    at {blue(f'{sal.filename}:{sal.line}')}")
        elif loc:
            print(f"# {num:<2d}  0x{pc:016x} in {func_str} {loc}")
        else:
            print(f"# {num:<2d}  0x{pc:016x} in {func_str}")

        # Interpreter frame metadata
        if frame_layout := make_int_frame_layout(dbg, frame):
            try:
                fuh = frame_layout.dyn_fuh
                cursor = frame_layout.cursor
                if not fuh:
                    return
                bc = fuh.bytecode
                start = bc.start if bc else cursor
                print(f"  {fuh.name or '???'}: 0x{cursor - start:x}")
            except:
                pass


class InfoFrameCommand(gdb.Command):
    """`iframe [N]` - extended info frame for interpreter frames."""

    def __init__(self) -> None:
        super().__init__("iframe", gdb.COMMAND_USER)

    def invoke(self, argument: str, from_tty: bool) -> None:
        try:
            dbg = debugger()
            n = int(argument) if argument else 0
            frame = dbg.frame(n)
            if frame is None:
                print(f"No frame {n}")
                return
            try:
                pc = frame.pc
                name = frame.name or "???"
                print(f"Frame {n}: 0x{pc:x} in {name}")
            except gdb.MemoryError:
                print(f"Frame {n}: <unavailable>")
                return

            if make_int_frame_layout(dbg, frame):
                print(f"  --- interpreter frame ---")
                dump_frame(dbg, frame)
            else:
                print(f"  (not an interpreter frame)")
        except Exception:
            traceback.print_exc()
