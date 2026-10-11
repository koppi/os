CXX=g++
LD=ld

# -fno-exceptions -fno-rtti: no unwinder/typeinfo runtime exists for this
# target (see usr/lib/cxxabi.cpp, shared with every C++ app). -fno-use-cxa-atexit
# and -fno-threadsafe-statics keep global/static destructor and
# guard-variable codegen out of the way of that minimal shim.
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib -I ../../usr/include/lib

LDFLAGS += -melf_i386 -T hello_theap.lds -Map hello_theap.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o thrheap ../../usr/lib/cxx_start.o main.o ../../usr/lib/cxxabi.o ../../usr/lib/stdio.o ../../usr/lib/string.o ../../usr/lib/system_calls.o ../../usr/lib/stdlib.o

clean:
	@rm -f thrheap hello_theap.map *.o
