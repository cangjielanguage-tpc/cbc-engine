# GDB Interpreter Frame Debugger

GDB Python helper that augments frame inspection for interpreter frames with
custom layout (FuH, bytecode cursor, etc).

## Setup

```bash
# In your GDB session:
(gdb) source /path/to/gdbinit.py
```

Or copy to `~/.gdbinit` for auto-load.

## Commands

| Command | Description |
|---------|-------------|
| `bt` | Backtrace with interpreter frame metadata appended |
| `iframe [N]` | Extended info for frame N (FuH, bytecode, bcPos) |

All commands accept an optional frame number (default: current frame).

## Example Output

```
(gdb) ibt
# 0   engine_interpretation_loop ()
    at /home/ubuntu/wellox/cbc-engine-initial-stage/src/interpreter/interpretation_loop.cpp:63
# 1   0x0000fffff77a89d0 in perform_2i_call () at /workspace/cbc-engine/src/arch_os/aarch64_linux/trampolines.S:512
  default._CGP7defaultiiHv() -> Void: 0x15
# 2   0x0000fffff77a8b8c in perform_2i_call () at /workspace/cbc-engine/src/arch_os/aarch64_linux/trampolines.S:600
  default.main() -> Int64: 0x3
```

