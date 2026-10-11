CXX=g++
LD=ld

# No -ffreestanding here (unlike apps/hello-cpp): this app includes the real
# <vector>/<algorithm> headers, and -ffreestanding makes bits/c++config.h set
# _GLIBCXX_HOSTED=0, which walls those headers off behind a #error. See
# usr/lib/cxxabi.cpp for the rest of the story (why they still link without a
# compiled libstdc++.a).
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib -I ../../usr/include/lib

LDFLAGS += -melf_i386 -T hello_stl.lds -Map hello_stl.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(LD) $(LDFLAGS) -o hellostl ../../usr/lib/cxx_start.o main.o ../../usr/lib/cxxabi.o ../../usr/lib/stdio.o ../../usr/lib/string.o ../../usr/lib/system_calls.o ../../usr/lib/stdlib.o

clean:
	@rm -f hellostl hello_stl.map *.o
