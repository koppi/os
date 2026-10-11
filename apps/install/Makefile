# install - the graphical installer for koppi-os, in ring 3 on microui.
# The shared rules (which objects, which include paths and why) are in
# ../microui/mui.mk. engine.c is the part that does the work and has no idea it is
# in ring 3; host.c is the same engine on the build machine (tools/testing/install-host-test.sh).

APP = install
OBJ = main.o engine.o sys.o

include ../microui/mui.mk
