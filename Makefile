# system platform
ifeq "$(PLATFORM)" ""
PLATFORM := $(shell uname)
endif

# for CPU
ifeq ($(shell uname -m),arm64)
MOPT=
else
ifeq ($(shell uname -m),x86_64)
MOPT=
else
ifeq ($(shell uname -m),armv8l)
MOPT=
else
MOPT=
endif
endif
endif

# for SDL2(default SDL3)
ifdef SDL2
CDEBUGFLAGS = -DSDL2
endif

ifdef YMFM
CDEBUGFLAGS += -DYMFM
endif

# SDL3/2 framework (for macOS)
ifeq "$(PLATFORM)" "Darwin"
ifdef SDL2
PROGRAM = px68k.sdl2
filename = SDL2.framework
filechk = $(shell ls /Library/Frameworks | grep ${filename})
ifeq (${filechk}, ${filename})
SDL_INCLUDE= -F/Library/Frameworks -D_THREAD_SAFE
SDL_LIB= -F/Library/Frameworks -framework SDL2
filename = SDL2_ttf.framework
filechk = $(shell ls /Library/Frameworks | grep ${filename})
ifeq (${filechk}, ${filename})
SDL_TTF_INC = -I/Library/Frameworks/SDL2_ttf.framework/Headers
SDL_TTF_LIB = -F/Library/Frameworks -framework SDL2_ttf
endif
endif
else
PROGRAM = px68k.sdl3
filename = SDL3.framework
filechk = $(shell ls /Library/Frameworks | grep ${filename})
ifeq (${filechk}, ${filename})
SDL_INCLUDE= -F/Library/Frameworks -D_THREAD_SAFE
SDL_LIB= -F/Library/Frameworks -framework SDL3
filename = SDL3_ttf.framework
filechk = $(shell ls /Library/Frameworks | grep ${filename})
ifeq (${filechk}, ${filename})
SDL_TTF_INC = -I/Library/Frameworks/SDL3_ttf.framework/Headers
SDL_TTF_LIB = -F/Library/Frameworks -framework SDL3_ttf
endif
endif
endif
endif

# for SDL3/2 LINK (use pkg-config)
ifndef SDL_INCLUDE
ifdef SDL2
SDL_INCLUDE = $(shell pkg-config sdl2 --cflags)
SDL_LIB     = $(shell pkg-config sdl2 --libs)
# SDL_TTF_INC = $(shell pkg-config SDL2_ttf --cflags)
# SDL_TTF_LIB = $(shell pkg-config SDL2_ttf --libs)
PROGRAM = px68k.sdl2
else
SDL_INCLUDE = $(shell pkg-config sdl3 --cflags)
SDL_LIB     = $(shell pkg-config sdl3 --libs)
SDL_TTF_INC = $(shell pkg-config sdl3-ttf --cflags)
SDL_TTF_LIB = $(shell pkg-config sdl3-ttf --libs)
PROGRAM = px68k.sdl3
endif
endif

ifeq "$(PLATFORM)" "Darwin"
SDL_LIB += -Wl,-rpath,/Library/Frameworks/
else
ifeq "$(PLATFORM)" "Linux"
SDL_LIB +=  -lrt
else
# for CygWin "windows"
ifdef SDL2
PROGRAM = px68k.sdl2.exe
else
PROGRAM = px68k.sdl3.exe
endif
endif
endif

include version.txt


# CC	 = clang -std=c17 -arch x86_64 -arch arm64
# CXX	 = clang++ -std=c++17 -arch x86_64 -arch arm64
CC	 = gcc -std=c11
CXX	 = g++ -std=c++14

CXXLINK	 = $(CXX)
RM	 = rm -rf
TAGS	 = etags
DEPEND	 = gccmakedep
DEPEND_DEFINES =

# for debug
# CDEBUGFLAGS += -g -O0 -fno-strict-aliasing
CDEBUGFLAGS += -O2  -fstrict-aliasing

#
# disable sound
#
# CDEBUGFLAGS+= -DNO_SOUND

#
# disable mercury unit
#
CDEBUGFLAGS+= -DNO_MERCURY

#
# enable RFMDRV(Over TCP/IP)
#
# CDEBUGFLAGS+= -DRFMDRV

#
# CPU Big/little Endianess option
#
# CDEBUGFLAGS+= -D__LITTLE_ENDIAN__
# CDEBUGFLAGS+= -D__BIG_ENDIAN__

#
# c68k (68000 Emurator) option 
#
# CDEBUGFLAGS+= -DC68K_NO_JUMP_TABLE
# CDEBUGFLAGS+= -DC68K_CONST_JUMP_TABLE

#
# for Opt.
#
# CDEBUGFLAGS= -O3
# CDEBUGFLAGS+= -funroll-loops
# CDEBUGFLAGS+= -fomit-frame-pointer
# CDEBUGFLAGS+= -ffast-math

#
# for DEBUG
#
# CDEBUGFLAGS= -O0
# CDEBUGFLAGS+= -g
# CDEBUGFLAGS+= -W -Wall -Wuninitialized
# CDEBUGFLAGS+= -Wunused
# CDEBUGFLAGS+= -Werror
# CDEBUGFLAGS+= -DINLINE=
# CDEBUGFLAGS+= -DUSE_GAS

CDEBUGFLAGS+=-DPX68K_VERSION=$(PX68K_VERSION)

CDEBUGFLAGS+=-DHAVE_STDINT_H

# SDL_INCLUDE=	`$(SDL_CONFIG) --cflags`
# SDL_LIB=		`$(SDL_CONFIG) --libs`

LDLIBS = -lm -lpthread

# MIDI option NO_MIDI=1 or FLUID=1
# macOS:CoreAudio/CoreMIDI Linux:Fluidsynth Win:winmm
#
ifdef NO_MIDI
MIDIOBJS= x68k/midi_non.o
else
ifeq "$(PLATFORM)" "Darwin"
ifdef FLUID
FLUID_INCLUDE=  -I/Library/Frameworks/FluidSynth.framework/Headers
FLUID_LIB= -F/Library/Frameworks -framework FluidSynth
MIDIOBJS= x68k/midi_fluid.o
else
LDLIBS+= -framework Cocoa -framework CoreMIDI
MIDIOBJS= x68k/midi_darwin.o
endif
else
ifeq "$(PLATFORM)" "Linux"
ifdef FLUID
FLUID_LIB=  -lfluidsynth
MIDIOBJS= x68k/midi_fluid.o
else
MIDIOBJS= x68k/midi_alsa.o
endif
# 
else
# Cygwin for MIDI (winmm.lib)
ifdef FLUID
FLUID_LIB=  -lfluidsynth
MIDIOBJS= x68k/midi_fluid.o
else
LDLIBS+= -lwinmm
MIDIOBJS= x68k/midi_win.o
endif
endif
endif
endif

EXTRA_INCLUDES= -I./SDL -I./x68k -I./win32api $(SDL_INCLUDE) $(FLUID_INCLUDE) $(SDL_TTF_INC)

CXXDEBUGFLAGS= $(CDEBUGFLAGS)

CFLAGS= $(MOPT) $(CDEBUGFLAGS) $(EXTRA_INCLUDES)
CXXFLAGS= $(MOPT) $(CXXDEBUGFLAGS) $(EXTRA_INCLUDES)
CXXLDOPTIONS= $(CXXDEBUGFLAGS)


CPUOBJS= x68k/d68k.o m68000/m68000.o
C68KOBJS= m68000/c68k/c68k.o m68000/c68k/c68kexec.o

X68KOBJS= x68k/adpcm.o x68k/bg.o x68k/crtc.o x68k/dmac.o x68k/fdc.o x68k/fdd.o x68k/disk_d88.o x68k/disk_dim.o x68k/disk_xdf.o x68k/gvram.o x68k/ioc.o x68k/irqh.o x68k/mem_wrap.o x68k/mercury.o x68k/mfp.o x68k/palette.o x68k/midi.o x68k/ppi.o x68k/rtc.o x68k/sasi.o x68k/scc.o x68k/scsi.o x68k/sram.o x68k/sysport.o x68k/tvram.o

ifdef YMFM
ifdef SDL2
FMGENOBJS =  SDL/SDL2/ymfm_wrap.o
else
FMGENOBJS =  SDL/SDL3/ymfm_wrap.o
endif
FMGENOBJS += ymfm/src/ymfm_opm.o ymfm/src/ymfm_adpcm.o ymfm/src/ymfm_pcm.o ymfm/src/ymfm_ssg.o ymfm/src/ymfm_opl.o ymfm/src/ymfm_opn.o ymfm/src/ymfm_misc.o
EXTRA_INCLUDES += -I./ymfm/src
else
FMGENOBJS =  fmgen/fmg_wrap.o
FMGENOBJS += fmgen/fmgen.o fmgen/file.o fmgen/fmtimer.o fmgen/opm.o fmgen/opna.o fmgen/psg.o
EXTRA_INCLUDES += -I./fmgen
endif

SDLOBJS= SDL/mouse.o SDL/status.o SDL/timer.o SDL/common.o SDL/prop.o SDL/winui.o SDL/keyboard.o

ifdef SDL2
SDLOBJS += SDL/SDL2/windraw.o SDL/SDL2/GamePad.o SDL/SDL2/dswin.o
SDLCXXOBJS += SDL/SDL2/winx68k.o
EXTRA_INCLUDES += -I./SDL/SDL2
else
SDLOBJS += SDL/SDL3/windraw.o SDL/SDL3/GamePad.o SDL/SDL3/SoundCtrl.o SDL/SDL3/menudraw.o
SDLCXXOBJS += SDL/SDL3/winx68k.o
EXTRA_INCLUDES += -I./SDL/SDL3
endif


WIN32APIOBJS= win32api/dosio.o win32api/fake.o win32api/peace.o
CGROMOBJS=	SDL/mkcgrom.o SDL/tool/create_cgrom.o

COBJS=		$(X68KOBJS) $(SDLOBJS) $(WIN32APIOBJS) $(CPUOBJS) $(C68KOBJS) $(MIDIOBJS)
CXXOBJS=	$(FMGENOBJS) $(SDLCXXOBJS)

CSRCS=		$(COBJS:.o=.c)
CXXSRCS=	$(CXXOBJS:.o=.cpp)
SRCS=		$(CSRCS) $(CXXSRCS)

.SUFFIXES: .c .cpp

OBJDIR		= obj

OBJS		 = $(addprefix $(OBJDIR)/, $(COBJS))
OBJS		+= $(addprefix $(OBJDIR)/, $(CXXOBJS))

MKCGROMOBJS	  = $(addprefix $(OBJDIR)/, $(WIN32APIOBJS))
MKCGROMOBJS	 += $(addprefix $(OBJDIR)/, $(CGROMOBJS))

OBJDIRS		= $(OBJDIR) $(OBJDIR)/m68000 $(OBJDIR)/m68000/c68k \
		  	$(OBJDIR)/fmgen $(OBJDIR)/win32api $(OBJDIR)/ymfm $(OBJDIR)/ymfm/src \
		  	$(OBJDIR)/SDL $(OBJDIR)/SDL/tool $(OBJDIR)/SDL/SDL2 $(OBJDIR)/SDL/SDL3 \
			$(OBJDIR)/x68k

px68kicon = ./macOS

all:: $(OBJDIRS) $(PROGRAM)

$(OBJDIRS):
		-mkdir $@

$(PROGRAM): $(OBJS)
	$(RM) $@
	$(CXXLINK) $(MOPT) -o $(PROGRAM) $(CXXLDOPTIONS) $(OBJS) $(LDLIBS) $(SDL_LIB) $(FLUID_LIB)

$(OBJDIR)/m68000/%.o: m68000/%.c
		$(CC) $(CFLAGS) -o $@ -c $<

$(OBJDIR)/fmgen/%.o: fmgen/%.cpp
		$(CXX) $(CXXFLAGS) -o $@ -c $<

$(OBJDIR)/ymfm/src/%.o: ymfm/src/%.cpp
		$(CXX) $(CXXFLAGS) -o $@ -c $<

$(OBJDIR)/win32api/%.o: win32api/%.c
		$(CC) $(CFLAGS) -o $@ -c $<

$(OBJDIR)/x68k/%.o: x68k/%.c
		$(CC) $(CFLAGS) -o $@ -c $<

$(OBJDIR)/SDL/%.o: SDL/%.c
		$(CC) $(CFLAGS) -o $@ -c $<

$(OBJDIR)/SDL/%.o: SDL/%.cpp
		$(CXX) $(CXXFLAGS) -o $@ -c $<

$(OBJDIR)/SDL/tool/%.o: SDL/tool/%.c
		$(CC) $(CFLAGS) -o $@ -c $<

depend::
	$(DEPEND) -- $(CXXFLAGS) $(DEPEND_DEFINES) -- $(SRCS)

clean::
	$(RM) $(PROGRAM)
	$(RM) $(OBJDIR)
	$(RM) *.CKP *.ln *.BAK *.bak *.o core errs ,* *~ *.a .emacs_* tags TAGS make.log MakeOut   "#"*

tags::
	find . -name "*.h" -o -name "*.c" -o -name "*.cpp" | $(TAGS) -

icon::
	mkdir "$(px68kicon)/AppIcon.iconset/"
	-sips -s format png -z 16 16 ./macOS/x68k.png -s dpiHeight 72.0 -s dpiWidth 72.0 --out "$(px68kicon)/AppIcon.iconset/icon_16.png"
	-sips -s format png -z 32 32 ./macOS/x68k.png -s dpiHeight 144.0 -s dpiWidth 144.0 --out "$(px68kicon)/AppIcon.iconset/icon_16@2x.png"
	-sips -s format png -z 32 32 ./macOS/x68k.png -s dpiHeight 72.0 -s dpiWidth 72.0 --out "$(px68kicon)/AppIcon.iconset/icon_32.png"
	-sips -s format png -z 64 64 ./macOS/x68k.png -s dpiHeight 144.0 -s dpiWidth 144.0 --out "$(px68kicon)/AppIcon.iconset/icon_32@2x.png"
	-sips -s format png -z 128 128 ./macOS/x68k.png -s dpiHeight 72.0 -s dpiWidth 72.0 --out "$(px68kicon)/AppIcon.iconset/icon_128.png"
	-sips -s format png -z 256 256 ./macOS/x68k.png -s dpiHeight 144.0 -s dpiWidth 144.0 --out "$(px68kicon)/AppIcon.iconset/icon_128@2x.png"
	-sips -s format png -z 256 256 ./macOS/x68k.png -s dpiHeight 72.0 -s dpiWidth 72.0 --out "$(px68kicon)/AppIcon.iconset/icon_256.png"
	-sips -s format png -z 512 512 ./macOS/x68k.png -s dpiHeight 144.0 -s dpiWidth 144.0 --out "$(px68kicon)/AppIcon.iconset/icon_256@2x.png"
	-sips -s format png -z 512 512 ./macOS/x68k.png -s dpiHeight 72.0 -s dpiWidth 72.0 --out "$(px68kicon)/AppIcon.iconset/icon_512.png"
	-sips -s format png -z 1024 1024 ./macOS/x68k.png -s dpiHeight 144.0 -s dpiWidth 144.0 --out "$(px68kicon)/AppIcon.iconset/icon_512@2x.png"
	-iconutil -c icns "$(px68kicon)/AppIcon.iconset"
	-rm -rf "$(px68kicon)/AppIcon.iconset"

mac:: $(OBJDIRS) $(PROGRAM) icon
	-rm -rf "$(PROGRAM).app/"
	mkdir -p "$(PROGRAM).app/Contents/MacOS"
	mkdir -p "$(PROGRAM).app/Contents/Resources/ja.lproj"
	echo "APPL????" > "$(PROGRAM).app/Contents/PkgInfo"
	cp -r "macOS/AppIcon.icns" "$(PROGRAM).app/Contents/Resources/AppIcon.icns"
	cp -r "macOS/Info.plist.make" "$(PROGRAM).app/Contents/Info.plist"
	cp $(PROGRAM) "$(PROGRAM).app/Contents/MacOS/px68k"

c68k::
	-rm -rf ./m68000/c68k/gen68k
	-rm -rf ./m68000/c68k/gen68k.exe
	-rm -rf ./m68000/c68k/CMakeCache.txt
	-rm -rf ./m68000/c68k/CMakeFiles
	cmake $(C68KFLAGS) -S ./m68000/c68k -B ./m68000/c68k
	cmake --build ./m68000/c68k

cgrom:: $(OBJDIRS) $(MKCGROMOBJS)
	$(RM) mkcgrom
	$(CXXLINK) $(CXXFLAGS) -o mkcgrom $(MKCGROMOBJS) $(SDL_LIB) $(SDL_TTF_LIB)

#
#	Win用アプリのアイコン　　make win
#
wicon::
	windres $(px68kicon)/icon.rc -o $(OBJDIR)/icon.o

win:: $(OBJDIRS) $(OBJS) wicon
	$(CXXLINK) $(MOPT) -o $(PROGRAM) $(CXXLDOPTIONS) $(OBJS) $(LDLIBS) $(SDL_LIB) $(FLUID_LIB) $(OBJDIR)/icon.o
