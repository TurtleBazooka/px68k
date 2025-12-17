#
#  cmake for px68k  X68000 Emulator
#  for macOS and Linux and MinGW
#  written by kameya  2023/6/2
#

cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "DO NOT BUILD in-tree.")
endif()

# == Set Project version ==
set(PROJECT_VERSION
 0.15.2
)

# == Project Name ==
project(px68k VERSION ${PROJECT_VERSION})

# == Get TimeStamp ==
string(TIMESTAMP TIMESTAMP "%Y%m%d")

# == option ==
option(YMFM "Use YMFM"        OFF)
option(FLUID "Use fluidsynth" OFF)
option(SDL2 "Use SDL2"        OFF)
option(NO_MIDI "No MIDI Play" OFF)
option(NO_MERCURY "No Mercury" ON)

# ==Set compile version ==
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

# ==Set compile options ==
add_compile_options(
#	-Wall
#	-g
	-O0
    -fstrict-aliasing
  )

# == Option definition ==
add_definitions(
#	-DNO_SOUND
	-DPX68K_VERSION=${PROJECT_VERSION}+K
	-DHAVE_STDINT_H
	)

if(NO_MERCURY)
add_definitions(
	-DNO_MERCURY
	)
endif()

if(YMFM)
  message(" - define YMFM")
add_definitions(
	-DYMFM
	)
endif()

# == Find SDL3/2 library and add to linker flags ==
if(SDL2)
  message(" - define SDL2")
 add_definitions( -DSDL2 )
 find_package(SDL2 REQUIRED)
# find_package(SDL2_ttf REQUIRED)
else()
  message(" - define SDL3")
 find_package(SDL3 REQUIRED)
# find_package(SDL3_ttf REQUIRED)
endif()

if(APPLE)
 find_library(CORESERVICES_LIB CoreServices)
 find_library(COREAUDIO_LIB CoreAudio)
 find_library(COREFOUNDATION_LIB CoreFoundation)
 set(LINKLIBS ${CORESERVICES_LIB} ${COREAUDIO_LIB} ${COREFOUNDATION_LIB})
endif()
if(UNIX AND NOT APPLE)
 list(APPEND LINKLIBS -lrt)
endif()
if (WIN32)
#  
endif()

# == add MIDI option ==
if(NO_MIDI)
 message(" - No MIDI")
 list(APPEND MIDI_SRC "x68k/midi_non.c")
else()
if(FLUID)
 message(" - fluidsynth MIDI")
 list(APPEND MIDI_SRC "x68k/midi_fluid.c")
 option(px68k_midi_fluid "Compile with fluidsynth support." ${FLUID})
else()
if(UNIX AND NOT APPLE)
  message (" - Linux ALSA MIDI")
  option(px68k_midi_alsa "Compile with ALSA support." ${UNIX})
else()
if (APPLE)
  message (" - macOS CoreMIDI")
  option(px68k_midi_core "Compile with CoreMIDI support." ${APPLE})
endif()
endif()
if (WIN32)
  message (" - WIN32 winmm MIDI")
  option(px68k_midi_winmm "Compile with WINMM support." ${WIN32})
endif()
endif()
endif()

# == Set source files ==
set(x68k_sources
  x68k/mem_wrap.c
  x68k/irqh.c
  x68k/ioc.c
  x68k/gvram.c
  x68k/tvram.c
  x68k/d68k.c
  x68k/sysport.c
  x68k/sram.c
  x68k/scsi.c
  x68k/sasi.c
  x68k/scc.c
  x68k/disk_xdf.c
  x68k/disk_dim.c
  x68k/disk_d88.c
  x68k/fdd.c
  x68k/fdc.c
  x68k/dmac.c
  x68k/crtc.c
  x68k/rtc.c
  x68k/ppi.c
  x68k/mfp.c
  x68k/palette.c
  x68k/bg.c
  x68k/adpcm.c
  x68k/mercury.c
  x68k/midi.c
  win32api/dosio.c
  win32api/fake.c
  win32api/peace.c
)

if(YMFM)
if(SDL2)
set(fmgen_sources
  SDL/SDL2/ymfm_wrap.cpp
)
else()
set(fmgen_sources
  SDL/SDL3/ymfm_wrap.cpp
)
endif()
list(APPEND fmgen_sources
  ymfm/src/ymfm_opm.cpp
  ymfm/src/ymfm_adpcm.cpp
  ymfm/src/ymfm_pcm.cpp
  ymfm/src/ymfm_ssg.cpp
  ymfm/src/ymfm_opl.cpp
  ymfm/src/ymfm_opn.cpp
  ymfm/src/ymfm_misc.cpp
)
else()
# FMGEN  OPM:YM2151 OPN3-L:YMF288
set(fmgen_sources
  fmgen/fmg_wrap.cpp
  fmgen/fmgen.cpp
  fmgen/file.cpp
  fmgen/fmtimer.cpp
  fmgen/opm.cpp
  fmgen/opna.cpp
  fmgen/psg.cpp
)
endif()

set(SDL_sources
#  SDL/mkcgrom.c
  SDL/winui.c
  SDL/keyboard.c
  SDL/timer.c
  SDL/prop.c
  SDL/status.c
  SDL/mouse.c
  SDL/common.c
)


if(SDL2)
  list(APPEND SDL_sources
  SDL/SDL2/winx68k.cpp
  SDL/SDL2/windraw.c
  SDL/SDL2/GameController.c
  SDL/SDL2/dswin.c
  )
  list(APPEND SDL_INCLUDE_DIR
  SDL/SDL2
  )
else()
  list(APPEND SDL_sources
  SDL/SDL3/winx68k.cpp
  SDL/SDL3/windraw.c
  SDL/SDL3/GamePad.c
  SDL/SDL3/SoundCtrl.c
  SDL/SDL3/menudraw.c
  )
  list(APPEND SDL_INCLUDE_DIR
  SDL/SDL3
  )
endif()

set(m68k_sources
  m68000/m68000.c
  m68000/c68k/c68k.c
  m68000/c68k/c68kexec.c
)

# == Set Windows icon==
if (WIN32)
  list(APPEND m68k_sources macOS/icon.rc)
endif()

# == WinMM(windows) ==
if(px68k_midi_winmm)
  list(APPEND LINKLIBS winmm)
  list(APPEND MIDI_SRC "x68k/midi_win.c")
endif()

# == CoreMIDI(macOS) ==
if(px68k_midi_core)
  find_library(COREMIDI_LIB CoreMIDI)
  list(APPEND LINKLIBS ${COREMIDI_LIB})
  list(APPEND MIDI_SRC "x68k/midi_darwin.c")
endif()

# == ALSA(Linux) ==
if(px68k_midi_alsa)
  set(NEED_PTHREAD ON)
  find_package(ALSA)
  if (NOT ALSA_FOUND)
    message(FATAL_ERROR "ALSA API requested but no ALSA dev libraries found")
  endif()
  list(APPEND MIDI_SRC "x68k/midi_alsa.c")
  list(APPEND LINKLIBS ALSA::ALSA)
  list(APPEND PACKAGE_DEPENDENCIES "find_dependency(ALSA)")
endif()

# == fluidsynth(macOS/Linux/win) ==
if(px68k_midi_fluid)
  list(APPEND MIDI_SRC "x68k/midi_fluid.c")
 if (APPLE)
  find_library(FLUID_LIBRARY FluidSynth)
  list(APPEND FLUID_INCLUDE_DIR "${FLUID_LIBRARY}/Headers")
  list(APPEND LINKLIBS "${FLUID_LIBRARY}")
 else()
  find_package(FluidSynth)
  list(APPEND LINKLIBS fluidSynth)
 endif()
endif()

# == Include directories ==
include_directories(
    ${FLUID_INCLUDE_DIR}
    ${ALSA_INCLUDE_DIR}
    m68000
    m68000/c68k
    x68k
    win32api
    fmgen
	ymfm/src
    SDL
    ${SDL_INCLUDE_DIR}
    SDL/tool
)

# == add execute ==
if(SDL2)
  set(BIN_TARGET "px68k.sdl2")
else()
  set(BIN_TARGET "px68k.sdl3")
endif()

if(UNIX AND NOT APPLE)
 add_executable(${BIN_TARGET} ${x68k_sources} ${fmgen_sources} ${m68k_sources} ${SDL_sources} ${MIDI_SRC})
endif()
if(APPLE)
 add_executable(${BIN_TARGET} MACOSX_BUNDLE ${x68k_sources} ${fmgen_sources} ${m68k_sources} ${SDL_sources} ${MIDI_SRC} macOS/AppIcon.icns)
endif()
if(WIN32)
 add_executable(${BIN_TARGET} WIN32 ${x68k_sources} ${fmgen_sources} ${m68k_sources} ${SDL_sources} ${MIDI_SRC})
endif()

if(APPLE)
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/macOS/AppIcon.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(${BIN_TARGET} PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER "TurtleBazooka/px68k(${TIMESTAMP})"
      MACOSX_BUNDLE_BUNDLE_NAME ${BIN_TARGET}
      MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${TIMESTAMP}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}(${TIMESTAMP})"
      MACOSX_BUNDLE_ICON_FILE "AppIcon"
  )
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13.0")
  option(MACOSX_STANDALONE_APP_BUNDLE "Generate a portable app bundle to use on other devices (requires sudo)" OFF)

  install_targets( ./bin ${BIN_TARGET} )

  if(MACOSX_STANDALONE_APP_BUNDLE)
      install(CODE "
        include(BundleUtilities)
        fixup_bundle(${CMAKE_BINARY_DIR}/${MACOSX_BUNDLE_BUNDLE_NAME}.app \"\" \"\")
        "
        COMPONENT Runtime)
  endif()

endif()

set(CPACK On)

# == Set link options ==
  target_link_libraries(${BIN_TARGET} PRIVATE ${LINKLIBS}
                        ${SDL3_LIBRARIES}
                        ${SDL3_TTF_LIBRARIES}
                        ${SDL2_LIBRARIES}
                        ${SDL2_TTF_LIBRARIES}
  )

# == Excute Packaging ==
if(CPACK)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(TOLOWER ${PROJECT_NAME} project_name)
    set(CPACK_PACKAGE_NAME ${project_name})
    set(CPACK_GENERATOR "7Z")
    # Common *nix files
    set(CPACK_STRIP_FILES TRUE)
    install(TARGETS ${BIN_TARGET} DESTINATION bin)
    set(desktop_file "${PROJECT_SOURCE_DIR}/Linux/${project_name}.desktop")

    find_program(DFI desktop-file-install)
    if(DFI)
      execute_process(COMMAND ${DFI} --dir=${CMAKE_BINARY_DIR} ${desktop_file})
      set(desktop_file "${CMAKE_BINARY_DIR}/${project_name}.desktop")
    endif()

    install(FILES "${desktop_file}"
      DESTINATION "share/applications"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Linux/icon.png"
      DESTINATION "share/icons/hicolor/512x512/apps"
      RENAME "${project_name}.png"
    )

    # -G DEB
    set(CPACK_PACKAGE_CONTACT "bubio")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "px68k Portable X68000 emulator - 'px68k' based on keropi")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl3-1.0-0")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # -G RPM
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

    find_program(RPMBUILD rpmbuild)
    if(RPMBUILD)
      list(APPEND CPACK_GENERATOR "RPM")
    endif()
    find_program(DPKG dpkg)
    if(DPKG)
      list(APPEND CPACK_GENERATOR "DEB")
    endif()

  elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "px68k")
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK "ON")
    set(CPACK_STRIP_FILES TRUE)
    set(CPACK_GENERATOR "DragNDrop")
  endif()

  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  include(CPack)
endif()