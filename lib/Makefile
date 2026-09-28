CXX=g++

CFLAGS += -m32
CFLAGS += -c -I.. -I../include -ffreestanding -fno-builtin -nostdlib
CFLAGS += -Werror -Wall -Wextra
CFLAGS += -Wno-pointer-to-int-cast
# The syscall ABI passes arguments in %ebx; PIC codegen steals %ebx for the GOT
# base, so userspace must be built position-dependent.
CFLAGS += -fno-pic -fno-pie

# cxx_start.c is plain C (no STL), so it builds like the rest of this
# directory. cxxabi.cpp deliberately does NOT get -ffreestanding: it exists
# to make real libstdc++ headers (<vector>, <string>, ...) usable, and
# -ffreestanding is what makes those headers refuse to compile at all (see
# cxxabi.cpp). -fno-builtin already covers what -ffreestanding would have
# bought us here (no wrong-optimization assumptions about our libc's
# semantics).
CXXFLAGS += -m32 -c -I.. -I../include -std=gnu++20 -fno-builtin -nostdlib
CXXFLAGS += -Werror -Wall -Wextra
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector

# cxx_rbtree.cpp only needs <map> (for the real std::_Rb_tree_node_base
# layout) -- no -I.. -I../include, since include/stddef.h (a kernel-internal
# placeholder, unrelated to this) shadows the compiler's own <stddef.h> via
# that search path and breaks max_align_t inside <bits/memory_resource.h>.
CXXFLAGS_RBTREE = -m32 -c -std=gnu++20 -fno-builtin -nostdlib
CXXFLAGS_RBTREE += -Werror -Wall -Wextra
CXXFLAGS_RBTREE += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS_RBTREE += -fno-pic -fno-pie -fno-stack-protector

all:
	@$(CC) $(CFLAGS) unistd.c
	@$(CC) $(CFLAGS) string.c
	@$(CC) $(CFLAGS) stdio.c
	@$(CC) $(CFLAGS) stdlib.c
	@$(CC) $(CFLAGS) system_calls.c
	@$(CC) $(CFLAGS) cxx_start.c
	@$(CXX) $(CXXFLAGS) cxxabi.cpp
	@$(CXX) $(CXXFLAGS_RBTREE) cxx_rbtree.cpp

clean:
	@rm -f *.o
