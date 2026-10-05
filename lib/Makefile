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

# cxx_rbtree.cpp / cxx_hashtable.cpp / cxx_chrono.cpp / cxx_pmr.cpp /
# cxx_condvar.cpp / cxx_list.cpp only need real libstdc++ headers
# (<map> / <unordered_map> / <chrono> / <memory_resource> /
# <condition_variable> / <list>, for their real out-of-line ABI symbols)
# -- no -I.. -I../include, since include/stddef.h (a kernel-internal
# placeholder, unrelated to either) shadows the compiler's own
# <stddef.h> via that search path and breaks max_align_t inside
# <bits/memory_resource.h>.
CXXFLAGS_STLIMPL = -m32 -c -std=gnu++20 -fno-builtin -nostdlib
CXXFLAGS_STLIMPL += -Werror -Wall -Wextra
CXXFLAGS_STLIMPL += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS_STLIMPL += -fno-pic -fno-pie -fno-stack-protector

# pthread_glibc.c / libc_ext.c need the real system <pthread.h>/
# <sched.h>/<unistd.h>/<time.h>/<semaphore.h>/<setjmp.h> for their
# actual type layouts (see each file's own comment) -- same -I
# exclusion as CXXFLAGS_STLIMPL above, and deliberately no
# -ffreestanding either (that's what makes glibc's own headers refuse
# to compile, same reasoning as cxxabi.cpp).
CFLAGS_GLIBC = -m32 -c -std=gnu11 -fno-builtin -nostdlib
CFLAGS_GLIBC += -Werror -Wall -Wextra
CFLAGS_GLIBC += -fno-pic -fno-pie -fno-stack-protector

# libc_ext.c's _setjmp() reads the caller's saved ebp/return-address
# straight out of its own stack frame (see that function's own
# comment) -- needs a real frame pointer to do that, so this is the
# one file in this directory that must NOT omit it.
CFLAGS_GLIBC_FP = $(CFLAGS_GLIBC) -fno-omit-frame-pointer

all:
	@$(CC) $(CFLAGS) unistd.c
	@$(CC) $(CFLAGS) string.c
	@$(CC) $(CFLAGS) stdio.c
	@$(CC) $(CFLAGS) stdlib.c
	@$(CC) $(CFLAGS) system_calls.c
	@$(CC) $(CFLAGS) mutex.c
	@$(CC) $(CFLAGS) pthread.c
	@$(CC) $(CFLAGS_GLIBC) pthread_glibc.c
	@$(CC) $(CFLAGS_GLIBC_FP) libc_ext.c
	@$(CC) $(CFLAGS_GLIBC) emutls.c
	@$(CC) $(CFLAGS) cxx_start.c
	@$(CC) $(CFLAGS) tls.c
	@$(CC) $(CFLAGS) libm.c
	@$(CXX) $(CXXFLAGS) cxxabi.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_string.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_rbtree.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_hashtable.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_chrono.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_pmr.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_condvar.cpp
	@$(CXX) $(CXXFLAGS_STLIMPL) cxx_list.cpp

clean:
	@rm -f *.o
