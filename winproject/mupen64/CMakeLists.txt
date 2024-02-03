cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(mupen64 C CXX)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)

################################################################################
# Global compiler options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_C_FLAGS "")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
endif()

################################################################################
# Global linker options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_EXE_LINKER_FLAGS "")
    set(CMAKE_MODULE_LINKER_FLAGS "")
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set(CMAKE_STATIC_LINKER_FLAGS "")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
endif()

################################################################################
# Nuget packages function stub.
################################################################################
function(use_package TARGET PACKAGE VERSION)
    message(WARNING "No implementation of use_package. Create yours. "
                    "Package \"${PACKAGE}\" with version \"${VERSION}\" "
                    "for target \"${TARGET}\" is ignored!")
endfunction()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Sub-projects
################################################################################



























set(PROJECT_NAME mupen64)

################################################################################
# Source groups
################################################################################
set(Headers
    "../../lua/LuaConsole.h"
    "../../lua/modules/avi.h"
    "../../lua/modules/d2d.h"
    "../../lua/modules/emu.h"
    "../../lua/modules/global.h"
    "../../lua/modules/input.h"
    "../../lua/modules/iohelper.h"
    "../../lua/modules/joypad.h"
    "../../lua/modules/memory.h"
    "../../lua/modules/movie.h"
    "../../lua/modules/savestate.h"
    "../../lua/modules/wgui.h"
    "../../lua/Recent.h"
    "../../main/debughook.h"
    "../../main/disasm.h"
    "../../main/ffmpeg_capture/ffmpeg_capture.hpp"
    "../../main/guifuncs.h"
    "../../main/helpers/collection_helpers.h"
    "../../main/helpers/io_helpers.h"
    "../../main/helpers/math_helpers.h"
    "../../main/helpers/string_helpers.h"
    "../../main/helpers/win_helpers.h"
    "../../main/lib/argh.h"
    "../../main/lib/ini.h"
    "../../main/md5.h"
    "../../main/Plugin.hpp"
    "../../main/rom.h"
    "../../main/savestates.h"
    "../../main/speex/arch.h"
    "../../main/speex/speex_resampler.h"
    "../../main/vcr.h"
    "../../main/vcr_compress.h"
    "../../main/vcr_resample.h"
    "../../main/win/Commandline.h"
    "../../main/win/Config.hpp"
    "../../main/win/configdialog.h"
    "../../main/win/features/CoreDbg.h"
    "../../main/win/features/CrashHelper.h"
    "../../main/win/features/RomBrowser.hpp"
    "../../main/win/features/Statusbar.hpp"
    "../../main/win/features/Toolbar.hpp"
    "../../main/win/main_win.h"
    "../../main/win/timers.h"
    "../../main/win/wrapper/PersistentPathDialog.h"
    "../../memory/dma.h"
    "../../memory/flashram.h"
    "../../memory/memory.h"
    "../../memory/pif.h"
    "../../memory/pif2.h"
    "../../memory/summercart.h"
    "../../memory/tlb.h"
    "../../r4300/cop1_helpers.h"
    "../../r4300/exception.h"
    "../../r4300/interrupt.h"
    "../../r4300/macros.h"
    "../../r4300/ops.h"
    "../../r4300/r4300.h"
    "../../r4300/recomp.h"
    "../../r4300/recomph.h"
    "../../r4300/x86/assemble.h"
    "../../r4300/x86/gcop1_helpers.h"
    "../../r4300/x86/interpret.h"
    "../../r4300/x86/regcache.h"
    "../resource.h"
)
source_group("Headers" FILES ${Headers})

set(Sources
    "../../lua/LuaConsole.cpp"
    "../../lua/Recent.cpp"
#    "../../main/adler32.c"
    "../../main/debughook.c"
    "../../main/disasm.cpp"
    "../../main/ffmpeg_capture/manager_win.cpp"
    "../../main/ffmpeg_capture/readscreen_win.cpp"
    "../../main/helpers/io_helpers.cpp"
    "../../main/md5.c"
    "../../main/Plugin.cpp"
    "../../main/rom.c"
    "../../main/savestates.cpp"
    "../../main/speex/speex_resampler.c"
    "../../main/vcr.cpp"
    "../../main/vcr_resample.c"
    "../../main/win/Commandline.cpp"
    "../../main/win/Config.cpp"
    "../../main/win/configdialog.cpp"
    "../../main/win/features/CoreDbg.cpp"
    "../../main/win/features/CrashHelper.cpp"
    "../../main/win/features/rombrowser.cpp"
    "../../main/win/features/Statusbar.cpp"
    "../../main/win/features/Toolbar.cpp"
    "../../main/win/guifuncs.cpp"
    "../../main/win/main_win.cpp"
    "../../main/win/timers.cpp"
    "../../main/win/vcr_compress.c"
    "../../main/win/wrapper/PersistentPathDialog.cpp"
    "../../memory/dma.cpp"
    "../../memory/flashram.cpp"
    "../../memory/memory.cpp"
    "../../memory/pif.cpp"
    "../../memory/summercart.cpp"
    "../../memory/tlb.cpp"
    "../../r4300/bc.c"
    "../../r4300/compare_core.c"
    "../../r4300/cop0.c"
    "../../r4300/cop1.c"
    "../../r4300/cop1_d.c"
    "../../r4300/cop1_helpers.cpp"
    "../../r4300/cop1_l.c"
    "../../r4300/cop1_s.c"
    "../../r4300/cop1_w.c"
    "../../r4300/exception.c"
    "../../r4300/interrupt.c"
    "../../r4300/profile.c"
    "../../r4300/pure_interp.c"
    "../../r4300/r4300.c"
    "../../r4300/recomp.c"
    "../../r4300/regimm.c"
    "../../r4300/special.c"
    "../../r4300/tlb.c"
    "../../r4300/x86/assemble.c"
    "../../r4300/x86/debug.c"
    "../../r4300/x86/gbc.c"
    "../../r4300/x86/gcop0.c"
    "../../r4300/x86/gcop1.c"
    "../../r4300/x86/gcop1_d.c"
    "../../r4300/x86/gcop1_helpers.cpp"
    "../../r4300/x86/gcop1_l.c"
    "../../r4300/x86/gcop1_s.c"
    "../../r4300/x86/gcop1_w.c"
    "../../r4300/x86/gr4300.c"
    "../../r4300/x86/gregimm.c"
    "../../r4300/x86/gspecial.c"
    "../../r4300/x86/gtlb.c"
    "../../r4300/x86/regcache.c"
    "../../r4300/x86/rjump.c"
)
source_group("Sources" FILES ${Sources})

set(ALL_FILES
    ${Headers}
    ${Sources}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE mupen64)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Target name
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "${PROJECT_NAME}_debug$ENV{TargetExt}"
    TARGET_NAME_RELEASE "${TARGET_NAME}$ENV{TargetExt}"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    PDB_OUTPUT_DIRECTORY_DEBUG   "${OUTPUT_DIRECTORY}"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
)
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
    $<$<CONFIG:Debug>:
        MultiThreadedDebug
    >
    $<$<CONFIG:Release>:
        MultiThreaded
    >
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

################################################################################
# Include directories
################################################################################
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lua;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../zlib;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../main;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libdeflate;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../xxhash"
)

################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "_DEBUG"
    ">"
    "$<$<CONFIG:Release>:"
        "NDEBUG"
    ">"
    "WIN32;"
    "_WIN32;"
    "_WINDOWS;"
    "__WIN32__;"
    "X86;"
    "_CRT_SECURE_NO_WARNINGS;"
    "_MBCS"
)

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /Gm-;
            /Od;
            /RTC1;
            /arch:SSE2;
            /ZI;
            /FI;
            "../main/debughook.h"
        >
        $<$<CONFIG:Release>:
            /GL;
            /Ox;
            /Ob2;
            /Oi;
            /GT;
            /arch:IA32;
            /Oy;
            /Gy;
            /Zi;
            /Ot;
            /fp:strict;
            /GS
        >
        /std:c++latest;
        /W3;
        /TP;
        /wd4018;
        /wd4244;
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    string(CONCAT FILE_CL_OPTIONS
        "$<$<CONFIG:Debug>:"
            "/TP"
        ">"
    )
    source_file_compile_options(../../main/adler32.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/md5.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/Plugin.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/rom.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/savestates.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/vcr.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/vcr_resample.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/speex/speex_resampler.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/Commandline.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/Config.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/configdialog.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/guifuncs.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/main_win.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/timers.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../main/win/vcr_compress.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/bc.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/compare_core.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/cop0.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/cop1.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/cop1_d.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/cop1_l.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/cop1_s.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/cop1_w.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/exception.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/interrupt.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/profile.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/pure_interp.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/r4300.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/recomp.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/regimm.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/special.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/tlb.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/assemble.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/debug.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gbc.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop0.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop1.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop1_d.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop1_helpers.cpp ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop1_l.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop1_s.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gcop1_w.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gr4300.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gregimm.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gspecial.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/gtlb.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/regcache.c ${FILE_CL_OPTIONS})
    string(CONCAT FILE_CL_OPTIONS
        "/TP"
    )
    source_file_compile_options(../../r4300/x86/rjump.c ${FILE_CL_OPTIONS})
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /INCREMENTAL
        >
        $<$<CONFIG:Release>:
            /OPT:REF;
            /LTCG;
            /OPT:ICF
        >
        /MANIFEST:NO;
        /DEBUG;
        /MACHINE:X86;
        /SUBSYSTEM:WINDOWS;
        /NXCOMPAT:NO;
        /DYNAMICBASE:NO
    )
endif()

################################################################################
# Dependencies
################################################################################
set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "Shlwapi;"
    "vfw32;"
    "winmm;"
    "propsys;"
    "Comctl32;"
    "zlib-2008-x32;"
    "uxtheme;"
    "Msimg32;"
    "Gdiplus;"
    "d2d1;"
    "dwrite;"
    "deflate;"
    "Dbghelp;"
    "lua54"
)
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

target_link_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../zlib;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../lua;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libdeflate;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../xxhash"
)

