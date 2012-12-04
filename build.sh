#!/bin/sh
mkdir -p out && cd build && cmake -DCMAKE_BUILD_TYPE:STRING=Debug -G "Unix Makefiles" && make
