# Shared build rules for the ring-3 microui apps (apps/calc, apps/clock).
# Include this from an app's own Makefile after setting APP and OBJ:
#
#   APP = calc
#   OBJ = main.o
#   include ../microui/mui.mk
#
# What goes into one of these binaries:
#
#   ../../lib/microui.c     the vendored toolkit, the same file the kernel's
#                           own desktop is built from -- compiled a second
#                           time for ring 3 rather than forked
#   ../microui/mui.c        this port: the grab, the paletted renderer, the
#                           input pump and the frame loop
#   ../doom/shim/printf.c   the vendored freestanding printf. The kernel's own
#                           lib/printf.c cannot be reused here: it takes con_lock
#                           (linux/spinlock.h) around every line, which is a
#                           kernel-only facility. Doom's copy is the same
#                           upstream, built for ring 3 already.
#   ../../usr/lib/string.o      memset/memcpy/strlen for both of the above
#
# Include paths, in order, and why each is needed: microui.c includes
# <asm/io.h>, <linux/stdlib.h> and <linux/printf.h>, which ../microui/shim stands in
# for (the kernel's versions are for ring 0), and <linux/string.h>, which is the
# kernel's own (../../include) -- its declarations are the userland's too.
# <linux/printf.h> in the shim is the one whose declarations match the printf.c
# compiled in here. ../../usr/include/lib (unprefixed, for <stdlib.h> and friends)
# and ../../usr/include (<lib/string.h>) are the userland C library's headers;
# the kernel's headers are only ever reached through their <linux/...> names.

CC = gcc
LD = ld

MUI = ../microui

CFLAGS  = -m32 -std=gnu23 -O2 -pipe -ffreestanding -fno-builtin -nostdlib \
          -fno-pic -fno-pie -fno-stack-protector -fno-common \
          -Wall -Wextra -Werror -Wno-pointer-to-int-cast \
          -I $(MUI) -I $(MUI)/shim -I ../../usr/include/lib -I ../../usr/include -I ../../include

# The vendored toolkit is upstream's; it is not this tree's job to keep it
# warning-clean under flags upstream never saw.
MUFLAGS = $(filter-out -Werror,$(CFLAGS))

# printf.c is built exactly as apps/doom and apps/chipnomad build it: against
# Doom's shim headers only, which is where its own <printf.h> and the
# printf_config.h beside it live.
PRINTFFLAGS = -m32 -std=gnu23 -O2 -pipe -ffreestanding -fno-builtin -nostdlib \
              -fno-pic -fno-pie -fno-stack-protector -w -I ../doom/shim

LDFLAGS = -melf_i386 -T $(APP).lds -Map $(APP).map

MUI_OBJ = mui.o microui.o printf.o

# string.o is memset/memcpy/strlen; an app adds to this (APP_LIB_OBJ) when it
# needs more of lib/ -- ../clock takes libm.o for sin/cos. libgcc supplies the
# 64-bit divide helpers the vendored printf's integer formatting calls, the
# same way apps/chipnomad picks it up.
LIB_OBJ   = ../../usr/lib/string.o $(APP_LIB_OBJ)
LIBGCC   := $(shell $(CC) -m32 -print-libgcc-file-name)

all: $(APP)

$(APP): $(OBJ) $(MUI_OBJ) $(APP).lds
	@$(LD) $(LDFLAGS) -o $(APP) $(OBJ) $(MUI_OBJ) $(LIB_OBJ) $(LIBGCC)
	@size $(APP) 2>/dev/null | tail -1 || true

# The font table is generated, not committed: it is a slice of unifont.sfn,
# which is already in the tree (lib/fonts/) and is what the kernel console draws with, so
# baking it here would be a second copy of the same glyphs that could drift.
# mkfont runs on the build machine -- -m32 because it parses the font through
# the kernel's <linux/types.h> (see mkfont.c).
$(MUI)/mkfont: $(MUI)/mkfont.c ../../include/linux/ssfn.h
	@gcc -m32 -O1 -Wall -Wextra -I $(MUI)/hostinc -o $@ $<

# Via a temporary: two apps share this one generated header, and a half-written
# one left behind by an interrupted build would be a confusing thing to debug.
$(MUI)/mui_font.h: $(MUI)/mkfont ../../lib/fonts/unifont.sfn
	@$(MUI)/mkfont ../../lib/fonts/unifont.sfn $@.tmp && mv -f $@.tmp $@

mui.o: $(MUI)/mui.c $(MUI)/mui.h $(MUI)/mui_font.h ../../include/linux/microui.h
	@$(CC) $(CFLAGS) -c $< -o $@

microui.o: ../../lib/microui.c ../../include/linux/microui.h
	@$(CC) $(MUFLAGS) -c $< -o $@

printf.o: ../doom/shim/printf.c
	@$(CC) $(PRINTFFLAGS) -c $< -o $@

%.o: %.c $(MUI)/mui.h
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f $(APP) $(APP).map *.o $(MUI)/mkfont $(MUI)/mui_font.h

.PHONY: all clean
