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
# 0   engine_interpretation_loop
    at /workspace/cbc-engine/src/interpreter/interpretation_loop.cpp:63
# 1   0x00007ffff70da2f0 in perform_2i_call at /workspace/cbc-engine/src/arch_os/x86_64_linux/trampolines.S:540
  default.fannkuch(Int64) -> Unit: 0x3a6
# 2   0x00007ffff70da529 in perform_2i_call at /workspace/cbc-engine/src/arch_os/x86_64_linux/trampolines.S:649
  default._CN7default4mainHvEL_E(@$ClN7default4mainHvEL_E$0) -> Unit: 0x13
# 3   0x00007ffff70da529 in perform_2i_call at /workspace/cbc-engine/src/arch_os/x86_64_linux/trampolines.S:649
  $ClN7default4mainHvEL_E$0.$GenericVirtualFunc() -> $box<Unit>: 0x7
# 4   0x00007ffff70da1c5 in engine_iregs_only_c2i_call at /workspace/cbc-engine/src/arch_os/x86_64_linux/trampolines.S:404
# 5   0x00007ffff7c6ebba in _CNat6FutureIG_E7executeHv
# 6   0x000000000061a8c3 in ExecuteCangjieStub
# 7   0x0000000000615ed5 in WrapperTask
# 8   0x0000000000680ffa in CJ_CJThreadEntry
# 9   0x0000000000000000 in ??
(gdb) iframe 1
Frame 1: 0x7ffff70da2f0 in perform_2i_call
  --- interpreter frame ---
  FrameLayout(fp=0x7ffff443fe40, sp=0x7ffff443fb60, size=736)
    FuH: DynamicFunctionHandle(addr=0x7ffec8012c60, mdef=0x19228)
      name: default.fannkuch(Int64) -> Unit
      mdef: 0x19228
      bytecode: ExecBytecodeInfo(ptr=0x7ffec8012f60, start=0x7ffec80268f0, bc_size=0xb70, frame_size=131129, static_frame_size=0)
        start: 0x7ffec80268f0
        end:   0x7ffec8027460
        bc_size: 0xb70 (2928)
        frame_size: 131129
        static_frame_size: 0
      bcPos: 0x3a6 (934)
(gdb)
```

