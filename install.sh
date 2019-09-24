#!/bin/bash

if [[ `uname -s` != 'Linux' ]] ; then
    echo "[*] Only linux is supported"
    exit 1
fi


if [[ -z $(which protoc-c) ]] ; then
    echo "[*] Install protoc-c library from https://github.com/protobuf-c/protobuf-c";
    exit 1
fi

echo "[*] protoc-c installed"
protoc-c --c_out=src/ chatmessage.proto
echo "[*] proto-files created"

if [[ ! -d bin/ ]] ; then
    mkdir -p bin/
    echo "[*] bin/ created"
fi

echo "[*] Compilation starts"
make
echo "[*] Compilation done.check bin/"

