add_executable(view
    capture/EncodingManager.cpp
    capture/EncodingManager.h
    capture/Resampler.cpp
    capture/Resampler.h
    capture/encoders/Encoder.h
    capture/encoders/AVIEncoder.cpp
    capture/encoders/AVIEncoder.h
    capture/encoders/FFmpegEncoder.cpp
    capture/encoders/FFmpegEncoder.h
    gui/Commandline.cpp
    gui/Commandline.h
    gui/FrontendService.cpp
    gui/IOService.cpp
    gui/main_win.cpp
    gui/main_win.h
    gui/resource.h
    gui/rsrc.rc
    gui/features/Cheats.cpp
    gui/features/Cheats.h
    gui/features/ConfigDialog.cpp
    gui/features/ConfigDialog.h
    gui/features/CoreDbg.cpp
    gui/features/CoreDbg.h
    gui/features/CrashHelper.cpp
    gui/features/CrashHelper.h
    gui/features/Dispatcher.cpp
    gui/features/Dispatcher.h
    gui/features/MGECompositor.cpp
    gui/features/MGECompositor.h
    gui/features/MovieDialog.cpp
    gui/features/MovieDialog.h
    gui/features/rombrowser.cpp
    gui/features/RomBrowser.hpp
    gui/features/Runner.cpp
    gui/features/Runner.h
    gui/features/Seeker.cpp
    gui/features/Seeker.h
    gui/features/Statusbar.cpp
    gui/features/Statusbar.hpp
    gui/wrapper/PersistentPathDialog.cpp
    gui/wrapper/PersistentPathDialog.h
    helpers/IOHelpers.h
    helpers/MathHelpers.h
    helpers/StringHelpers.h
    helpers/WinHelpers.h
    lua/LuaConsole.cpp
    lua/LuaConsole.h
    lua/LuaService.cpp
    lua/modules/avi.h
    lua/modules/d2d.h
    lua/modules/emu.h
    lua/modules/global.h
    lua/modules/input.h
    lua/modules/iohelper.h
    lua/modules/joypad.h
    lua/modules/memory.h
    lua/modules/movie.h
    lua/modules/savestate.h
    lua/modules/wgui.h
)
target_sources(view PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gui/rsrc.rc)

target_include_directories(view PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(view PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(view PRIVATE
    lib_static
    core
    shared

    Shlwapi.lib
    vfw32.lib
    winmm.lib
    propsys.lib
    Comctl32.lib
    uxtheme.lib
    Msimg32.lib
    Gdiplus.lib
    d2d1.lib
    dwrite.lib
    Dbghelp.lib
    Dcomp.lib
    D3D11.lib
    DXGI.lib

    ${CMAKE_SOURCE_DIR}/lib/zlib/zlib-2008-x32.lib
    ${CMAKE_SOURCE_DIR}/lib/libdeflate/deflate.lib
    ${CMAKE_SOURCE_DIR}/lib/lua/lua54.lib
)

set_target_properties(view PROPERTIES
    WIN32_EXECUTABLE ON
    OUTPUT_NAME "mupen64"
    SUFFIX ".exe"
)

target_link_options(view PRIVATE 
    /SUBSYSTEM:WINDOWS
    /MACHINE:x86
    /NXCOMPAT:NO
)

set(MAIN_PROJECT_NAME "Mupen64")
set(WINDOWS_APP_NAME "Mupen64")
