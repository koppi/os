CXX=g++
LD=ld

# -fno-exceptions -fno-rtti: no unwinder/typeinfo runtime exists for this
# target (see lib/cxxabi.cpp, shared with every C++ app). -fno-use-cxa-atexit
# and -fno-threadsafe-statics keep global/static destructor and
# guard-variable codegen out of the way of that minimal shim.
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib -I ../../include/lib

LDFLAGS += -melf_i386 -T hello_pthread.lds -Map hello_pthread.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o hellopth ../../lib/cxx_start.o main.o ../../lib/cxxabi.o ../../lib/pthread.o ../../lib/mutex.o ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o

clean:
	@rm -f hellopth hello_pthread.map *.o
