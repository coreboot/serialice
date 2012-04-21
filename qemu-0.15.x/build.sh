#!/bin/sh
./configure --extra-ldflags="-ldl" --disable-kvm --disable-sdl --enable-serialice \
	    --target-list="x86_64-softmmu, i386-softmmu"

make
