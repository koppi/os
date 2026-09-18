CC = gcc
LD = ld
OBJCOPY = objcopy

# Lua customisation - see luaconf.h / lauxlib.h for each knob.
LUACFG = -DLUA_USE_C89 \
         -DPRINTF_ALIAS_STANDARD_FUNCTION_NAMES_SOFT=1 \
         '-Dl_sprintf(s,sz,f,i)=snprintf((s),(sz),(f),(i))' \
         "-Dlua_getlocaledecpoint()='.'" \
         '-Dl_system(cmd)=((cmd)==0?0:-1)' \
         '-DLUA_PATH_DEFAULT="/rd/?.lua;/rd/?/init.lua;/fda/?.lua;./?.lua"' \
         '-DLUA_CPATH_DEFAULT="./?.so"' \
         '-Dlua_writestring(s,l)=__lua_out((s),(l))' \
         '-Dlua_writeline()=__lua_out("\n",1)' \
         '-Dlua_writestringerror(s,p)=__lua_errf((s),(p))'

CFLAGS = -m32 -std=gnu23 -O3 -pipe -ffreestanding -fno-builtin -nostdlib \
         -fno-stack-protector -fno-pic -fno-pie -fno-common -fno-tree-loop-distribute-patterns \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
         -I shim -I src -w $(LUACFG)

LDFLAGS = -melf_i386 -T lua.lds -Map lua.map

LUASRC  = $(filter-out src/linit.c src/liolib.c src/luac.c src/onelua.c, \
                       $(wildcard src/l*.c))
LUAOBJ  = $(LUASRC:.c=.o) src/frontend.o
SHIMOBJ = shim/printf.o shim/string.o shim/stdlib.o shim/stdio.o shim/time.o \
          shim/libm.o shim/libm_s.o shim/port_asm.o shim/arith64.o
OBJ     = $(LUAOBJ) openlibs.o $(SHIMOBJ)

# 64-bit soft arithmetic: build our own copy - the kernel's arith64.o may not
# exist yet when `make` reaches apps/ (apps build before the kernel objects).
LIBOBJ  = ../../lib/stdlib.o ../../lib/system_calls.o ../../lib/unistd.o

all: lua

lua: $(OBJ) lua.lds
	@$(LD) $(LDFLAGS) -o lua $(OBJ) $(LIBOBJ)
	@$(OBJCOPY) --strip-all lua
	@size lua

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@

shim/libm_s.o: shim/libm.S
	@$(CC) -m32 -c $< -o $@

shim/port_asm.o: shim/port_asm.S
	@$(CC) -m32 -c $< -o $@

shim/arith64.o: ../../arith64.c
	@$(CC) -m32 -O3 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
	       -fno-stack-protector -w -c $< -o $@

clean:
	@rm -f lua lua.map $(OBJ)
