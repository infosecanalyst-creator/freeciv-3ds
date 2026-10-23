#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source
DATA		:=
INCLUDES	:=	include
GRAPHICS	:=	gfx
ROMFS		:=	romfs
GFXBUILD	:=	$(ROMFS)/gfx

APP_TITLE	:=	Freeciv 3DS
APP_DESCRIPTION	:=	Freeciv turn-based strategy game
APP_AUTHOR	:=	xPsycho999
ICON		:=	../freeciv/data/icons/48x48/freeciv-client.png
#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections -fno-short-enums \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -Wl,--wrap=exit -Wl,--wrap=abort

LIBS	:=

FREECIV_BUILD := $(TOPDIR)/../freeciv/build-3ds

FREECIV_LIBS := \
	$(FREECIV_BUILD)/server/.libs/libfreeciv-srv.a \
	$(FREECIV_BUILD)/server/advisors/.libs/libadvisors.a \
	$(FREECIV_BUILD)/server/generator/.libs/libgenerator.a \
	$(FREECIV_BUILD)/server/ruleset/.libs/libruleload.a \
	$(FREECIV_BUILD)/server/savegame/.libs/libsavegame.a \
	$(FREECIV_BUILD)/server/scripting/.libs/libscripting_fcdb.a \
	$(FREECIV_BUILD)/server/scripting/.libs/libscripting_server.a \
	$(FREECIV_BUILD)/ai/default/.libs/libdefaultai.a \
	$(FREECIV_BUILD)/ai/classic/.libs/libclassicai.a \
	$(FREECIV_BUILD)/ai/tex/.libs/libtexai.a \
	$(FREECIV_BUILD)/ai/.libs/libaicommon.a \
	$(FREECIV_BUILD)/common/aicore/.libs/libaicore.a \
	$(FREECIV_BUILD)/common/networking/.libs/libfcivnetwork.a \
	$(FREECIV_BUILD)/common/scriptcore/.libs/libscriptcore.a \
	$(FREECIV_BUILD)/common/.libs/libfreeciv.a \
	$(FREECIV_BUILD)/utility/.libs/libcivutility.a \
	$(FREECIV_BUILD)/dependencies/lua-5.5/src/.libs/liblua.a \
	$(FREECIV_BUILD)/dependencies/tolua-5.2/src/lib/.libs/libtolua.a \
	$(FREECIV_BUILD)/dependencies/cvercmp/.libs/libcvercmp.a

LIBS	+= $(FREECIV_LIBS) -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(CTRULIB)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export ROMFS_T3XFILES	:=	$(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES		:=	$(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(addsuffix .h,$(subst .,_,$(BINFILES))) $(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD) \
			-I$(TOPDIR)/../freeciv/common \
			-I$(TOPDIR)/../freeciv/server \
			-I$(TOPDIR)/../freeciv/ai \
			-I$(TOPDIR)/../freeciv/utility \
			-I$(TOPDIR)/../freeciv/client \
			-I$(TOPDIR)/../freeciv/build-3ds \
			-I$(TOPDIR)/../freeciv/build-3ds/gen_headers \
			-I$(TOPDIR)/../freeciv/gen_headers \
			-I$(TOPDIR)/../freeciv/gen_headers/enums \
			-I$(TOPDIR)/../freeciv/common/networking \
			-I$(TOPDIR)/../freeciv/common/aicore \
			-I$(TOPDIR)/../freeciv/common/scriptcore \
			-I$(TOPDIR)/../freeciv/server/advisors \
			-I$(TOPDIR)/../freeciv/server/generator \
			-I$(TOPDIR)/../freeciv/server/ruleset \
			-I$(TOPDIR)/../freeciv/server/savegame \
			-I$(TOPDIR)/../freeciv/server/scripting \
			-I$(TOPDIR)/../freeciv/ai/classic \
			-I$(TOPDIR)/../freeciv/ai/default \
			-I$(TOPDIR)/../freeciv/ai/stub \
			-I$(TOPDIR)/../freeciv/ai/tex \
			-I$(TOPDIR)/../freeciv/dependencies/lua-5.5/src \
			-I$(TOPDIR)/../freeciv/dependencies/tolua-5.2/src/lib

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

IMAGEMAGICK	:=	$(shell which convert)

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean

#---------------------------------------------------------------------------------
all: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD)

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x	$(BUILD)/%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(NO_SMDH)),)
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh
else
$(OUTPUT).3dsx	:	$(OUTPUT).elf
endif

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
