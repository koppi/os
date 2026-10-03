# ChipNomad Tracker for koppi-os.
#
# Three source trees go into one ring-3 ELF:
#
#   third_party/chipnomad/   the vendored tracker and its engine, upstream's
#                            cpp-migration branch (see its VENDORED-COMMIT)
#   bridge/                  the part of upstream's own C++ migration that
#                            branch has not finished; ../README.md explains
#                            why it is needed at all
#   koppios/, shim/          this port: the platform backend and the C library
#                            surface the tracker needs beyond lib/
#
# Built against the real system headers and linked against this kernel's own
# runtime, never host glibc -- the same arrangement as apps/hello-qt, and for
# the same reason: the tracker uses std::function, and libstdc++'s headers
# only work with libstdc++'s idea of <stdint.h> and friends.

CN   = ../../third_party/chipnomad
LIB  = ../../lib
DOOM = ../doom

CXX = g++
CC  = gcc
LD  = ld

TRACKER = $(CN)/tracker
ENGINE  = $(CN)/chipnomad_lib

INC = -I bridge -I koppios -I shim \
      -I $(TRACKER)/src -I $(TRACKER)/src/corelib -I $(TRACKER)/src/import \
      -I $(TRACKER)/src/screens -I $(TRACKER)/platforms/shared \
      -I $(ENGINE) -I $(ENGINE)/chips -I $(ENGINE)/external/ayumi -I $(ENGINE)/export

# -fno-exceptions: this target has no working unwinder -- see ../README.md
#   ("No C++ exceptions") and the note at the top of chipnomad_lib/project_io.cpp,
#   which is the one file that threw.
# -fno-rtti: nothing needs typeid, and the type_info tables would be dead weight.
# -include chipnomad_prelude.h: fixes two order dependencies in the vendored
#   headers in one place rather than thirty; bridge/chipnomad_prelude.h says which.
# -ffunction-sections + --gc-sections: the tracker links its exporters and
#   importers whether or not a given build reaches them.
CXXFLAGS = -m32 -std=gnu++17 -Os -w -nostdlib \
           -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics \
           -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
           -ffunction-sections -fdata-sections \
           -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
           -include chipnomad_prelude.h $(INC)

# The vendored Ayumi emulator is Peter Sovietov's, carried by upstream, and
# needs no bridging; the prelude is deliberately not forced into it (its
# ayumi_filters.cpp redeclares FIR_SIZE locally, which clashes with ayumi.h
# once anything else has included that).
AYUMICXXFLAGS = -m32 -std=gnu++17 -Os -w -nostdlib -fno-exceptions -fno-rtti \
                -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
                -ffunction-sections -fdata-sections \
                -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(INC)

CFLAGS = -m32 -Os -w -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I shim

LDFLAGS = -melf_i386 -T chipnomad.lds -Map chipnomad.map --gc-sections -s

BUILD = build

TRACKER_SRC = $(wildcard $(TRACKER)/src/*.cpp) \
              $(wildcard $(TRACKER)/src/import/*.cpp) \
              $(wildcard $(TRACKER)/src/screens/*.cpp) \
              $(TRACKER)/platforms/shared/file_system.cpp \
              $(TRACKER)/platforms/shared/font_manager.cpp \
              $(TRACKER)/platforms/shared/font12x16.cpp \
              $(TRACKER)/platforms/shared/font16x24.cpp \
              $(TRACKER)/platforms/shared/font24x36.cpp \
              $(TRACKER)/platforms/shared/font32x48.cpp \
              $(TRACKER)/platforms/shared/font48x54.cpp

ENGINE_SRC = $(wildcard $(ENGINE)/*.cpp) \
             $(wildcard $(ENGINE)/chips/*.cpp) \
             $(wildcard $(ENGINE)/export/*.cpp)

AYUMI_SRC = $(wildcard $(ENGINE)/external/ayumi/*.cpp)

PORT_SRC = bridge/bridge.cpp bridge/tracker_app.cpp \
           koppios/gfx_koppios.cpp koppios/audio_device_koppios.cpp \
           koppios/input_utils_koppios.cpp koppios/mainloop_koppios.cpp \
           koppios/file_system_koppios.cpp \
           main_koppios.cpp

TRACKER_OBJ = $(addprefix $(BUILD)/,$(notdir $(TRACKER_SRC:.cpp=.o)))
ENGINE_OBJ  = $(addprefix $(BUILD)/,$(notdir $(ENGINE_SRC:.cpp=.o)))
AYUMI_OBJ   = $(addprefix $(BUILD)/,$(notdir $(AYUMI_SRC:.cpp=.o)))
PORT_OBJ    = $(addprefix $(BUILD)/,$(notdir $(PORT_SRC:.cpp=.o)))

# This port's C library: the FILE/printf/sscanf surface lib/ does not have,
# plus lib/libc_ext.c rebuilt with that surface left out (-DKOPPIOS_APP_STDIO).
# printf.c is the vendored mpaland printf apps/doom already carries -- compiled
# here into this app's own object, not shared with doom's build.
SHIM_OBJ = $(BUILD)/stdio_koppios.o $(BUILD)/scanf_koppios.o \
           $(BUILD)/misc_koppios.o $(BUILD)/printf.o $(BUILD)/libc_ext.o

# lib/pthread_glibc.o is here for __errno_location() and clock_gettime(),
# which glibc's headers turn `errno` and the sem_timedwait in libc_ext.c into;
# its pthread surface comes along but nothing in the tracker calls it, and
# --gc-sections drops what is unreachable.
KERNEL_OBJ = $(LIB)/cxx_start.o $(LIB)/cxxabi.o $(LIB)/string.o $(LIB)/stdlib.o \
             $(LIB)/system_calls.o $(LIB)/libm.o $(LIB)/pthread_glibc.o \
             $(LIB)/pthread.o $(LIB)/mutex.o

LIBGCC := $(shell $(CXX) -m32 -print-libgcc-file-name)

VPATH = $(TRACKER)/src:$(TRACKER)/src/import:$(TRACKER)/src/screens:\
$(TRACKER)/platforms/shared:$(ENGINE):$(ENGINE)/chips:$(ENGINE)/export:\
$(ENGINE)/external/ayumi:bridge:koppios:shim:.

all: chipnomad

chipnomad: $(BUILD) $(TRACKER_OBJ) $(ENGINE_OBJ) $(AYUMI_OBJ) $(PORT_OBJ) $(SHIM_OBJ) chipnomad.lds
	@$(LD) $(LDFLAGS) -o chipnomad $(LIB)/cxx_start.o \
		$(PORT_OBJ) $(TRACKER_OBJ) $(ENGINE_OBJ) $(AYUMI_OBJ) $(SHIM_OBJ) \
		$(LIB)/cxxabi.o $(LIB)/string.o $(LIB)/stdlib.o \
		$(LIB)/system_calls.o $(LIB)/libm.o \
		$(LIB)/pthread_glibc.o $(LIB)/mutex.o $(LIBGCC)
	@size chipnomad

$(BUILD):
	@mkdir -p $(BUILD)

# A change to the flags above changes what every object means (the prelude and
# -fno-exceptions in particular), so they all depend on the makefile.
$(TRACKER_OBJ) $(ENGINE_OBJ) $(AYUMI_OBJ) $(PORT_OBJ) $(SHIM_OBJ): Makefile

$(BUILD)/ayumi.o: $(ENGINE)/external/ayumi/ayumi.cpp | $(BUILD)
	@$(CXX) $(AYUMICXXFLAGS) -c $< -o $@

$(BUILD)/ayumi_filters.o: $(ENGINE)/external/ayumi/ayumi_filters.cpp | $(BUILD)
	@$(CXX) $(AYUMICXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp | $(BUILD)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/printf.o: $(DOOM)/shim/printf.c | $(BUILD)
	@$(CC) $(CFLAGS) -I $(DOOM)/shim -c $< -o $@

$(BUILD)/libc_ext.o: $(LIB)/libc_ext.c | $(BUILD)
	@$(CC) $(CFLAGS) -DKOPPIOS_APP_STDIO -c $< -o $@

clean:
	@rm -rf $(BUILD) chipnomad chipnomad.map

.PHONY: all clean
