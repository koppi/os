CXX=g++
LD=ld

# No -ffreestanding, no -I .../include/lib: see main.cpp (same reasoning as
# apps/hello-str / apps/hello-map).
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib

LDFLAGS += -melf_i386 -T hello_umap.lds -Map hello_umap.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o helloump ../../usr/lib/cxx_start.o main.o ../../usr/lib/cxxabi.o ../../usr/lib/cxx_hashtable.o ../../usr/lib/stdio.o ../../usr/lib/string.o ../../usr/lib/system_calls.o ../../usr/lib/stdlib.o

clean:
	@rm -f helloump hello_umap.map *.o
