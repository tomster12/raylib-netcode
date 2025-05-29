#!/bin/bash
# Build project, run binary, duplicate stdout / stderr to a log, forward SIGINT to the process
SRC="./client/main.c"
BIN="./build/client"
LOG="./logs/client.log"
./cbuild.sh "$SRC" -silent -output "$BIN" || exit 1
stdbuf -o0 -eL "$BIN" 2>&1 | tee -i "$LOG"
