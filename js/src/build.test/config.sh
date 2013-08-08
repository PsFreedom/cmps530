#!/bin/bash

#../configure --disable-methodjit --disable-monoic --disable-optimize --disable-tests --enable-debug-symbols
#../configure --disable-methodjit --disable-monoic --disable-optimize --disable-tests --enable-debug-symbols --enable-threadsafe
../configure --disable-methodjit --disable-monoic --disable-optimize --disable-tests --enable-debug-symbols --enable-threadsafe --with-nspr-cflags="-I/usr/include/nspr4/" --with-nspr-libs="/usr/lib64/libnspr4.a /usr/lib64/libplds4.a /usr/lib64/libplc4.a"
#../configure --disable-methodjit --disable-monoic --disable-optimize --disable-tests --enable-debug-symbols --enable-threadsafe --with-nspr-cflags="-I/home/bank/workspace/charles/git/cmps530/js/src/build.test/config/system_wrappers_js/"
#../configure --disable-methodjit --disable-monoic --disable-optimize --disable-tests --enable-debug-symbols --with-pthreads --enable-threadsafe


