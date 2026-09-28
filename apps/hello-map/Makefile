CXX=g++
LD=ld

# No -ffreestanding, no -I .../include/lib: see main.cpp (same reasoning as
# apps/hello-str).
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib

LDFLAGS += -melf_i386 -T hello_map.lds -Map hello_map.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o hellomap ../../lib/cxx_start.o main.o ../../lib/cxxabi.o ../../lib/cxx_rbtree.o ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o

clean:
	@rm -f hellomap hello_map.map *.o
