#!/bin/sh
mkdir bin && cd build && cmake -DCMAKE_BUILD_TYPE:STRING=Debug -G "Unix Makefiles" && make
