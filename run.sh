#!/bin/bash 
set -e

SRC="huff_only.cpp"
OUT="main"


TEST="huff_only_serial.cpp"
OUT_TEST="main_serial"

INCLUDE_DIR="qpl_lib/include/"
LIB_DIR="qpl_lib/lib/"


echo "Compiling $SRC..."
g++ "$SRC" -I"$INCLUDE_DIR" -L"$LIB_DIR" -lqpl -ldl -lpthread -o "$OUT"

echo "Compiling $TEST..."
g++ "$TEST" -I"$INCLUDE_DIR" -L"$LIB_DIR" -lqpl -ldl -lpthread -o "$OUT_TEST"

echo "Running $OUT..."
./"$OUT" software_path dataset/

echo "Running $TEST..."
./"$OUT_TEST" software_path dataset/


