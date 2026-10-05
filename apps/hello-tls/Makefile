CXX=g++
LD=ld

# Real ELF thread-local storage: %gs-relative thread_local variables backed by lib/tls.c and
# the set_thread_area syscall (38). Built against the system <pthread.h> like Qt's threads are
# (lib/pthread_glibc.o), hence no -I ../../include/lib (its stdio.h/stddef.h would shadow the
# compiler's own). -fno-exceptions -fno-rtti -fno-threadsafe-statics: as every C++ app here
# (no unwinder/typeinfo runtime, minimal lib/cxxabi.cpp). Unlike the others this one does NOT
# pass -fno-use-cxa-atexit: with it GCC silently drops the registration of thread_local
# destructors (no __cxa_thread_atexit call at all), which is the thing under test here.
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib

LDFLAGS += -melf_i386 -T hello_tls.lds -Map hello_tls.map

LIB = ../../lib
LIBGCC := $(shell $(CXX) -m32 -print-libgcc-file-name)

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o tlsthr $(LIB)/cxx_start.o main.o $(LIB)/tls.o $(LIB)/cxxabi.o $(LIB)/pthread_glibc.o $(LIB)/mutex.o $(LIB)/stdio.o $(LIB)/string.o $(LIB)/system_calls.o $(LIB)/stdlib.o $(LIB)/libc_ext.o $(LIBGCC)

clean:
	@rm -f tlsthr hello_tls.map *.o

.PHONY: all clean
