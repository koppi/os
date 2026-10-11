CXX=g++
LD=ld

# No -ffreestanding, no -I .../include/lib: see main.cpp (same reasoning as
# apps/hello-str / apps/hello-map).
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib

LDFLAGS += -melf_i386 -T hello_set.lds -Map hello_set.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o helloset ../../usr/lib/cxx_start.o main.o ../../usr/lib/cxxabi.o ../../usr/lib/cxx_rbtree.o ../../usr/lib/stdio.o ../../usr/lib/string.o ../../usr/lib/system_calls.o ../../usr/lib/stdlib.o

clean:
	@rm -f helloset hello_set.map *.o
