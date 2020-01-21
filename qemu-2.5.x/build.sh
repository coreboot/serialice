#!/bin/sh
./configure --disable-kvm --disable-sdl --enable-serialice \
	    --target-list="x86_64-softmmu, i386-softmmu" \
            --python=$(which python2) --disable-gtk \
	    --disable-smartcard --disable-glusterfs

if [ $? -eq 0 ]; then
   make
fi
