# --------------------- INFORMATION --------------------------------

# This the DDS Makefile for MacOS and the clang compiler.
# It creates a dynamically linked (shared) library, libdds.so.

# --------------------- CONFIGURATION ------------------------------

# You can configure the following:

# 1. The threading systems that you want in the library.
# You will always get single-threading.  If you have multiple
# threading systems, the default will be the multi-threading one
# with the lowest number (see System.cpp).  All that matters is
# CC_THREADING.

# GCD and WINAPI don't work on Windows.
THR_GCD		= -DDDS_THREADS_GCD
THR_OPENMP	= -DDDS_THREADS_OPENMP
THR_WINAPI	= -DDDS_THREADS_WINAPI
THR_STL		= -DDDS_THREADS_STL

THREADING	= $(THR_GCD) $(THR_STL)

# If you need to add something for a threading system, this is
# the place.

THREAD_COMPILE	=
THREAD_LINK	=

# 2. Debugging options.  (There are more granular options in debug.h.)

DEBUG_ALL	= -DDDS_DEBUG_ALL 
TIMING		= -DDDS_TIMING
SCHEDULER	= -DDDS_SCHEDULER

# All that matters from no. 2 and no. 3 is the following.  Here you
# can add $(SMALL_MEMORY) etc.

HOST_ARCH	:= $(shell uname -m 2>/dev/null)
ifeq ($(HOST_ARCH),arm64)
M1_MAX_BUILD	?=	1
else
M1_MAX_BUILD	?=	0
endif

M1_MAX_FLAGS	=
ifeq ($(M1_MAX_BUILD),1)
M1_MAX_FLAGS	+= -DDDS_TARGET_APPLE_M1_MAX
endif

PGO_MODE	?=	none
PGO_PROFILE_DIR	?=	$(CURDIR)/build-pgo-generate/pgo-data
PGO_PROFILE_DATA	?=	$(PGO_PROFILE_DIR)/default.profdata
PGO_COMPILE_FLAGS	=
PGO_LINK_FLAGS	=

DDS_BEHAVIOR	= $(M1_MAX_FLAGS)

# ----------------------- OFTEN OK    ------------------------------

# From here on you you don't have to change anything to CONFIGURE
# the compilation.  But you may well have to change something to 
# get it to compile.

INCL_SOURCE	= Makefiles/sources.txt

# If your compiler name is not given here, change it.
CC		= clang++

PROFILE_BUILD	?=	0

# We compile with aggressive warnings, but we have to turn off some
# of them as they appear in libraries in great numbers...

WARN_FLAGS	= 		\
	-Wshadow 		\
	-Wsign-conversion 	\
	-pedantic -Wall -Wextra  \
	-Wcast-align -Wcast-qual \
	-Wctor-dtor-privacy 	\
	-Wdisabled-optimization \
	-Winit-self 		\
	-Wmissing-declarations 	\
	-Wmissing-include-dirs 	\
	-Wcomment 		\
	-Wold-style-cast 	\
	-Woverloaded-virtual 	\
	-Wredundant-decls 	\
	-Wsign-promo 		\
	-Wstrict-overflow=1 	\
	-Wswitch-default -Wundef \
	-Werror 		\
	-Wno-unused 		\
	-Wno-unknown-pragmas 	\
	-Wno-long-long		\
	-Wno-format

RELEASE_OPT_FLAGS	= -O3 -flto
PROFILE_OPT_FLAGS	= -O2 -g -fno-omit-frame-pointer

ifeq ($(PROFILE_BUILD),1)
OPT_FLAGS	= $(PROFILE_OPT_FLAGS)
	BASE_BUILD_DIR	= build-profile
else ifeq ($(PGO_MODE),generate)
OPT_FLAGS	= $(RELEASE_OPT_FLAGS) -fprofile-instr-generate
	BASE_BUILD_DIR	= build-pgo-generate
PGO_LINK_FLAGS	= -fprofile-instr-generate
else ifeq ($(PGO_MODE),use)
	OPT_FLAGS	= $(RELEASE_OPT_FLAGS) -fprofile-instr-use=$(PGO_PROFILE_DATA) -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date
	BASE_BUILD_DIR	= build-pgo-use
PGO_LINK_FLAGS	= -fprofile-instr-use=$(PGO_PROFILE_DATA)
else
OPT_FLAGS	= $(RELEASE_OPT_FLAGS)
	BASE_BUILD_DIR	= build
endif

BUILD_DIR	?=	$(BASE_BUILD_DIR)
EXTRA_COMPILE_FLAGS ?=
EXTRA_LINK_FLAGS ?=

COMPILE_FLAGS	= -fPIC $(OPT_FLAGS) -mtune=generic -std=c++11 \
		$(WARN_FLAGS) 	\
		$(DDS_BEHAVIOR) $(THREAD_COMPILE) $(THREADING) $(EXTRA_COMPILE_FLAGS)

DLLBASE		= dds

LINK_FLAGS	=		\
	-shared			\
	$(THREAD_LINK)		\
	$(PGO_LINK_FLAGS)	\
	-fPIC $(EXTRA_LINK_FLAGS)

include $(INCL_SOURCE)

OBJ_DIR		= $(BUILD_DIR)/obj
SHARED_LIB	= $(BUILD_DIR)/lib$(DLLBASE).so
LEGACY_SHARED_LIB = lib$(DLLBASE).so

O_FILES 	= $(addprefix $(OBJ_DIR)/,$(subst .cpp,.o,$(SOURCE_FILES)))
DEP_FILES	= $(subst .o,.d,$(O_FILES))


macos:	$(O_FILES)
	$(CC) -o $(SHARED_LIB) $(O_FILES) $(LINK_FLAGS)

macos_profile:
	$(MAKE) PROFILE_BUILD=1 macos

macos_pgo_generate:
	$(MAKE) PGO_MODE=generate macos

macos_pgo_use:
	$(MAKE) PGO_MODE=use macos

pgo_merge:
	mkdir -p $(PGO_PROFILE_DIR)
	xcrun llvm-profdata merge -output=$(PGO_PROFILE_DATA) $(PGO_PROFILE_DIR)/*.profraw

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o:	%.cpp | $(OBJ_DIR)
	$(CC) $(COMPILE_FLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

%.o:	$(OBJ_DIR)/%.o
	@:

depend:
	@echo "Dependencies are generated automatically in $(OBJ_DIR)/*.d"

clean:
	rm -rf $(BUILD_DIR) *.o *.d $(LEGACY_SHARED_LIB)

install:
	test -d ../test || mkdir ../test
	test -d ../examples || mkdir ../examples
	cp $(SHARED_LIB) ../test
	cp $(SHARED_LIB) ../examples

-include $(DEP_FILES)

