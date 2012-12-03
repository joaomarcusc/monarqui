monarqui
========

A FileSystem monitor.

Compiling
========

You will need the following packages:


    sudo apt-get install libxml2-dev libglib2.0-dev libgtk2.0-dev lua5.1 liblua5.1-dev liblua5.1-filesystem0 liblua5.1-posix1 cmake

Enter the source directory and run build.sh:

    # sh build.sh

It will (hopefully) generate two binaries: out/monarqui_cli and out/monarqui_gui. For now, the monarqui_gui is non-functional, only the CLI version works.

