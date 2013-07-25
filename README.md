monarqui
========

A Linux FileSystem monitor.

Compiling
========

You will need the following packages:

    sudo apt-get install libxml2-dev libglib2.0-dev libgtk2.0-dev lua5.1 liblua5.1-dev liblua5.1-filesystem0 liblua5.1-posix1 cmake

You'll also need the ZeroMQ 3 libraries and headers. Unfortunately, Ubuntu only provides ZeroMQ 2, packages so, if you want to build 
from the sources, you'll need to download the sources and build it yourself.

Enter the source directory and run build.sh:

    # sh build.sh

It will (hopefully) generate two binaries: out/monarqui`_cli and out/monarqui`_gui. 
