#!/usr/bin/env zsh
IN_FILE=SOKOLVLS.bin
OUT_FILE=SOKOLVLS.8xv
CALC_NAME=SOKOLVLS
I_FORMAT=bin
O_FORMAT=8xv

convbin --input "$IN_FILE" --output "$OUT_FILE" --iformat "$I_FORMAT" --oformat "$O_FORMAT" --name "$CALC_NAME"