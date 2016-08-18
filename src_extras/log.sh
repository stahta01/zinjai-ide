#!/bin/bash
LANG=en_US
gdb --ex "set height 0" --ex "set confirm off" --ex "run" --ex "bt full" --ex "quit" --args bin/zinjai.bin --for-gdb > "$HOME/zinjai-log.txt"
