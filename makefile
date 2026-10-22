# ----------------------------------------
# Makefile Options for build configuration
# ----------------------------------------
#
# APPLANG=en (English, default)
# APPLANG=fr (French)
#
# VARIANT=normal (default)
# VARIANT=l2 (L2 variant with quad support instead of the periodic table)
#
# UNITS=1 (physical units instead of Python syntax, see below)
# STEPS=1 (step-by-step solutions instead of Python syntax: neither Python nor units)
# TILOOK=1 (the TI-84 look: the interface drawn like the TI-84 Plus CE's own, with its fonts)
# KHICAS=1 (with STEPS=0: KhiCAS's arrangement, menus, keys and applications, drawn in FlowCE's look)

APPLANG ?= en
VARIANT ?= normal
UNITS ?= 0
STEPS ?= 0
TILOOK ?= 0
KHICAS ?= 0

# This fork drops the spreadsheet (WITH_SHEET) and the periodic table (WITH_PERIODIC): about
# 160 KB of flash for the input/display work (user's choice, 2026-10-05), and the variation tables
# (WITH_TABVAR: tabvar, domain; ~87 KB) for the Focus interface's fonts (2026-10-05, notes/ui).
# Physical units (WITH_UNITS, 34.6 KB) are traded for giac's Python syntax (its translator,
# 28.4 KB; B. Parisse's suggestion, 2026-10-08): both do not fit. UNITS=1 builds units and no
# Python syntax, as up to v1.7 (released as FlowCE-units.b84 since v1.8).
FEATURE_DEFS = -DWITH_LAPLACE -DWITH_DESOLVE -DWITH_EQW
ifeq ($(UNITS),1)
    FEATURE_DEFS += -DWITH_UNITS
endif
# The step-by-step build (FlowCE.b84 from 2.0): no Python syntax. DeltaCE: with units too (STEPS=1
# UNITS=1: they fit together with CE toolchain v15, 2026-10-15)
# DeltaCE (the steps build) draws its graphs on its own screen (src/focus_graph.cc: functions,
# equations, parametric, polar, slope fields): KhiCAS's plot commands and graph viewer (WITH_PLOT,
# 146 KB: cobweb, histograms, bar charts, point lists, geometry figures) are out (2026-10-17)
# DeltaCE has giac's statistics (mean, median, quartiles, standard deviation, correlation,
# regressions: WITH_STATS, kmisc.cc) without the spreadsheet they came with (WITH_SHEET); its graph
# screen draws the data (2026-10-18)
ifeq ($(STEPS),1)
    FEATURE_DEFS += -DFLOWCE_STEPS -DWITH_STATS
else ifeq ($(KHICAS),1)
    # KhiCAS's applications back: the spreadsheet and its statistics, the variation tables, units
    # and constants, the periodic table; FlowCE's graph screen (focus_graph.cc) replaces KhiCAS's
    # plot viewer (WITH_PLOT, 146 KB): it is what makes them fit
    FEATURE_DEFS += -DWITH_EDITOR -DWITH_SHEET -DWITH_TABVAR -DWITH_UNITS -DWITH_PERIODIC
else
    FEATURE_DEFS += -DWITH_EDITOR -DWITH_PLOT # (DeltaCE, the steps build: no script editor, 17 KB, 2026-10-17)
endif

# The TI-84 look (2026-10-22): the interface looks like the calculator's own (home screen, status
# bar, MathPrint), its text drawn with the OS's fonts read at runtime (src/ui_font_ti.c) instead
# of the Focus fonts (ui_fontdata.c, ~65 KB). C files see it too (CFLAGS).
ifeq ($(TILOOK),1)
    VARIANT_DEFS += -DFLOWCE_TILOOK
    FEATURE_DEFS += -DFLOWCE_TILOOK
endif
# The KhiCAS-like build (2026-10-22): the Python build arranged as Bernard Parisse's KhiCAS (its
# console, F-key menus, catalog, keys, spreadsheet, tables of variations, units, periodic table),
# drawn in FlowCE's look, with FlowCE's graph screen
ifeq ($(KHICAS),1)
    VARIANT_DEFS += -DFLOWCE_KHICAS
    FEATURE_DEFS += -DFLOWCE_KHICAS
endif

ifeq ($(APPLANG),fr)
    LANG_DEF = -DFRANCAIS
else ifneq ($(APPLANG),en)
    $(error APPLANG must be either 'en' (default) or 'fr')
endif

ifeq ($(VARIANT),l2)
    FEATURE_DEFS += -DWITH_QUAD
else ifeq ($(VARIANT),normal)
    FEATURE_DEFS +=
else
    $(error VARIANT must be either 'normal' (default) or 'l2')
endif

NAME = DEMO
COMPRESSED = NO
ARCHIVED = NO

CFLAGS = -Wall -Wextra -Oz -DMP_16BIT -DTICE -Isrc/tommath -Wno-unused-command-line-argument $(LANG_DEF) $(VARIANT_DEFS)
CXXFLAGS = -std=c++14 -fno-rtti -fno-threadsafe-statics -Wall -Wextra -Oz -Iustl -Isrc/tommath -DMP_16BIT -DTICE -DFLOWCE -DHAVE_CONFIG_H -DXLIGHT -DCOMPILE_FOR_STABILITY \
    -Wno-unknown-pragmas -Wno-unneeded-internal-declaration -Wno-sign-compare -Wno-unused-function -Wno-unused-parameter \
    -Wno-unused-variable -Wno-unused-but-set-variable -Wno-deprecated-register -Wno-reorder-ctor -Wno-inline-new-delete \
    -Wno-deprecated-copy-with-user-provided-copy $(LANG_DEF) $(FEATURE_DEFS) $(INLFILE)
# The LTO code generation (CEdev: LTOFLAGS ?= CFLAGS) merges identical functions (uSTL vectors of
# same-size types, copy constructors...): 3.8 KB less; a function whose address is used keeps it.
# It inlines less: a small helper called from many places (context accessors, gen(vecteur),
# evalf...) stays a call, smaller than its copies on the eZ80: 33 KB less
LTOFLAGS = $(CFLAGS) -Xclang -fmerge-functions -mllvm -inline-threshold=-10

# CE toolchain v15 (clang 19, GNU as/ld; tools/dev/wsl_build.sh *-v15): src/toolchain_compat.h first
# in every C++ file (giac's #define std ustl and v15's headers). At the LTO step, the frame pointer
# bias, new in v15: in a function with a big frame, ix points inside it rather than at its top,
# so more locals are reached with a one-byte offset (ix-129 and beyond take several instructions):
# giac's big functions shrink by 10%. The LTO step keeps the default inlining: the 898 problems
# on the emulator (same answers) take 1.0% less time than with v14.2 for an app 5.6% smaller
# (2,561,255 B vs 2,712,385); inline thresholds -10 and -20 save 56 and 76 KB more but cost
# 4.7% and 9.6% of time (calls to small helpers such as unary_function_ptr::operator==).
# DeltaCE 1.2: threshold 0 saves 29 KB at no measurable cost (the quick suite's median time 1.00,
# the same answers); -5 saves 36 KB but costs 4%.
CEDEV_MAJOR := $(firstword $(subst ., ,$(patsubst v%,%,$(shell cedev-config --version 2>/dev/null))))
ifeq ($(shell [ "0$(CEDEV_MAJOR)" -ge 15 ] && echo y),y)
CXXFLAGS += -include src/toolchain_compat.h
LTOFLAGS = $(CFLAGS) -Xclang -fmerge-functions -mllvm -z80-frame-bias-policy=auto -mllvm -inline-threshold=0
endif

# giac's files inline as little as the LTO step (a small helper stays a call on the eZ80): 24 KB
# less; FlowCE's own files keep the default inlining, except (DeltaCE 2.1) the ones not drawing
# per pixel: answer, console, the catalog, the menus, main, menufr, the steps page (0.9 KB less;
# file.cc stays: with less inlining v15 miscompiled load_console_state_smem, v15_biascheck.py)
INLFILE = -mllvm -inline-threshold=-10
%/fake.cc.bc %/file.cc.bc %/focus.cc.bc %/focus_edit.cc.bc %/focus_graph.cc.bc %/mathedit.cc.bc %/mathinput.cc.bc %/mathinput_glue.cc.bc %/textGUI.cc.bc %/tiinput.cc.bc %/tiinput_glue.cc.bc %/ui_math.cc.bc: INLFILE :=

# The Focus drawing code works per pixel: compiled for speed (with -Oz, shifts and multiplies
# become helper calls). Small files: a few KB of flash.
%ui_font.c.bc %ui_font_ti.c.bc %ui_gfx.c.bc: CFLAGS := $(subst -Oz,-O2,$(CFLAGS))

CPP_EXTENSION = cc
APP_NAME = FlowCE
APP_VERSION = 5.0.0.0000
DESCRIPTION = FlowCE - calculus CAS - GNU GPL

EXTRA_LDFLAGS = -e 9999
ALLOCATOR = CUSTOM
SKIP_LIBRARY_LDFLAGS = YES
PREFER_OS_CRT = YES
PREFER_OS_LIBC = NO
LIBLOAD_OPTIONAL = fileioc

# ----------------------------

include app_tools/makefile
