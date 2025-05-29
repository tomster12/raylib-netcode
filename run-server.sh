#!/bin/bash
# Build project, run binary, duplicate stdout / stderr to a log, forward SIGINT to the process
SRC="./server/main.c"
BIN="./build/server"
LOG="./logs/server.log"
./cbuild.sh "$SRC" -silent -output "$BIN" || exit 1
stdbuf -o0 -eL "$BIN" 2>&1 | tee -i "$LOG"
