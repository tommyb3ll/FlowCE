# Host (x86-64 Linux) build of KhiCAS's giac with AddressSanitizer + LeakSanitizer.
# Driven by build.sh: T = prepared source tree (repo copy + host.patch [+ fix.patch]),
# OBJ = object directory, EXE = executable. tommath is compiled from the repo, unchanged.
HG := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO := $(abspath $(HG)/../..)

# the calculator's defines (makefile: CXXFLAGS, FEATURE_DEFS for APPLANG=en VARIANT=normal),
# plus HOSTGIAC for the 64-bit host changes of host.patch
DEFS := -DHOSTGIAC -DTICE -DHAVE_CONFIG_H -DXLIGHT -DCOMPILE_FOR_STABILITY -DMP_16BIT \
        -DWITH_LAPLACE -DWITH_DESOLVE -DWITH_EQW -DWITH_PLOT
SAN ?= -fsanitize=address -fno-omit-frame-pointer
OPT ?= -O1
# CEdev: -fno-exceptions -fno-threadsafe-statics, its own small C++ headers (here: include/).
# -fno-sized-deallocation: delete must reach uSTL's inline operator delete (free), like operator new.
CXXFLAGS := -std=c++14 -nostdinc++ -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-sized-deallocation -g $(OPT) $(SAN) \
            $(DEFS) -I$(T)/src -I$(T)/ustl -I$(REPO)/src/tommath -isystem $(HG)/include -w -MMD -MP
CFLAGS := -g $(OPT) $(SAN) -DMP_16BIT -I$(REPO)/src/tommath -w -MMD -MP

# KDISPLAY=1 (default): kdisplay.cc too (merge_sqrt, graph view: the driver's -k option, which
# runs src/answer.cc, do_run's steps), with
# src/ui_gfx.c, ui_font.c, ui_fontdata.c drawing off screen and stubs_ui.cc
KDISPLAY ?= 1
ifeq ($(KDISPLAY),1)
GIAC_SRC := $(wildcard $(T)/giac/*.cc)
UI_SRC := $(REPO)/src/ui_gfx.c $(REPO)/src/ui_font.c $(REPO)/src/ui_fontdata.c
HOST_EXTRA := $(OBJ)/host/stubs_ui.o $(OBJ)/host/khicas_post.o $(OBJ)/khicas/answer.o
else
GIAC_SRC := $(filter-out $(T)/giac/kdisplay.cc,$(wildcard $(T)/giac/*.cc))
endif
USTL_SRC := $(wildcard $(T)/ustlsrc/*.cc)
TOM_SRC := $(wildcard $(REPO)/src/tommath/*.c)
# KhiCAS's input pre-pass (TI implicit multiplication), used by the driver's -i option
KHI_SRC := $(REPO)/src/tiinput.cc $(REPO)/src/tiinput_glue.cc
OBJS := $(patsubst $(T)/giac/%.cc,$(OBJ)/giac/%.o,$(GIAC_SRC)) \
        $(patsubst $(T)/ustlsrc/%.cc,$(OBJ)/ustl/%.o,$(USTL_SRC)) \
        $(patsubst $(REPO)/src/tommath/%.c,$(OBJ)/tommath/%.o,$(TOM_SRC)) \
        $(patsubst $(REPO)/src/%.cc,$(OBJ)/khicas/%.o,$(KHI_SRC))         $(patsubst $(REPO)/src/%.c,$(OBJ)/ui/%.o,$(UI_SRC)) $(HOST_EXTRA)         $(OBJ)/host/stubs.o $(OBJ)/host/driver.o

$(EXE): $(OBJS)
	@mkdir -p $(dir $@)
	g++ $(SAN) -g -o $@ $(OBJS) -lm

$(OBJ)/giac/%.o: $(T)/giac/%.cc
	@mkdir -p $(dir $@)
	@echo "  CXX $<"
	@g++ $(CXXFLAGS) -c $< -o $@

$(OBJ)/ustl/%.o: $(T)/ustlsrc/%.cc
	@mkdir -p $(dir $@)
	@echo "  CXX $<"
	@g++ $(CXXFLAGS) -c $< -o $@

$(OBJ)/tommath/%.o: $(REPO)/src/tommath/%.c
	@mkdir -p $(dir $@)
	@gcc $(CFLAGS) -c $< -o $@

# the Focus drawing code, in C, without TICE (no eZ80 blitter)
$(OBJ)/ui/%.o: $(REPO)/src/%.c
	@mkdir -p $(dir $@)
	@gcc -g $(OPT) $(SAN) -isystem $(HG)/include -I$(T)/src -w -MMD -MP -c $< -o $@

$(OBJ)/khicas/%.o: $(REPO)/src/%.cc
	@mkdir -p $(dir $@)
	@echo "  CXX $<"
	@g++ $(CXXFLAGS) -I$(T) -c $< -o $@

$(OBJ)/host/%.o: $(HG)/%.cc
	@mkdir -p $(dir $@)
	@echo "  CXX $<"
	@g++ $(CXXFLAGS) -iquote $(T)/giac -I$(T) -c $< -o $@

-include $(OBJS:.o=.d)
