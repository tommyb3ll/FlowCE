# ----------------------------------------
# Makefile Options for build configuration
# ----------------------------------------
#
# APPLANG=en (English, default)
# APPLANG=fr (French)
#
# VARIANT=normal (default)
# VARIANT=l2 (L2 variant with quad support instead of the periodic table)

APPLANG ?= en
VARIANT ?= normal

# This fork drops the spreadsheet (WITH_SHEET) and the periodic table (WITH_PERIODIC): about
# 160 KB of flash for the input/display work (user's choice, 2026-10-05), and the variation tables
# (WITH_TABVAR: tabvar, domain; ~87 KB) for the Focus interface's fonts (2026-10-05, notes/ui).
FEATURE_DEFS = -DWITH_UNITS -DWITH_LAPLACE -DWITH_DESOLVE -DWITH_EQW -DWITH_PLOT

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

CFLAGS = -Wall -Wextra -Oz -DMP_16BIT -DTICE -Isrc/tommath -Wno-unused-command-line-argument $(LANG_DEF)
CXXFLAGS = -std=c++14 -Wall -Wextra -Oz -Iustl -Isrc/tommath -DMP_16BIT -DTICE -DFLOWCE -DHAVE_CONFIG_H -DXLIGHT -DCOMPILE_FOR_STABILITY \
    -Wno-unknown-pragmas -Wno-unneeded-internal-declaration -Wno-sign-compare -Wno-unused-function -Wno-unused-parameter \
    -Wno-unused-variable -Wno-unused-but-set-variable -Wno-deprecated-register -Wno-reorder-ctor -Wno-inline-new-delete \
    -Wno-deprecated-copy-with-user-provided-copy $(LANG_DEF) $(FEATURE_DEFS)

# The Focus drawing code works per pixel: compiled for speed (with -Oz, shifts and multiplies
# become helper calls). Small files: a few KB of flash.
%ui_font.c.bc %ui_gfx.c.bc: CFLAGS := $(subst -Oz,-O2,$(CFLAGS))

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
