
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import state  # noqa: F401 - registers cache + event handler
from commands import InfoFrameCommand, IBtCommand

IBtCommand()
InfoFrameCommand()

print("Interpreter frame debugger loaded.")
print("  ibt      - backtrace + interpreter metadata")
print("  iframe N - extended frame info")
print("Stubs not implemented: metadata will show <stub>.")
