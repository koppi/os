CC=gcc
CXX=g++
LD=ld

CFLAGS += -Wall -g -Wextra -Werror -fno-builtin -nodefaultlibs -nostartfiles -nostdlib -fno-pic -fno-pie -m32 -std=gnu23 -I ../../include/lib

# -fno-exceptions -fno-rtti: no unwinder/typeinfo runtime exists for this
# target (see cxxabi.cpp). -fno-use-cxa-atexit and -fno-threadsafe-statics
# keep global/static destructor and guard-variable codegen out of the way of
# the minimal shim below.
CXXFLAGS += -Wall -g -Wextra -Werror -std=gnu++20 -m32 -ffreestanding -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
CXXFLAGS += -fno-pic -fno-pie -fno-stack-protector -nostdlib -I ../../include/lib

LDFLAGS += -melf_i386 -T hello_cpp.lds -Map hello_cpp.map

all:
	@$(CXX) $(CXXFLAGS) -c main.cpp
	@$(CXX) $(CXXFLAGS) -c cxxabi.cpp
	@$(CC) $(CFLAGS) -c cxx_start.c
	@$(LD) $(LDFLAGS) -o hellocpp cxx_start.o main.o cxxabi.o ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o

clean:
	@rm -f hellocpp hello_cpp.map *.o
