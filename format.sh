#!/bin/bash

find . -iname "*.cpp" -o -iname "*.h" -o -iname "*.hpp" -o -iname "*.c" | xargs clang-format -i
