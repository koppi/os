CXX=g++
LD=ld

# No -ffreestanding, no -I .../include/lib: see main.cpp for why this app
# does not include our own stdio.h alongside the real <string>.
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib

LDFLAGS += -melf_i386 -T hello_str.lds -Map hello_str.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o hellostr ../../lib/cxx_start.o main.o ../../lib/cxxabi.o ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o

clean:
	@rm -f hellostr hello_str.map *.o
