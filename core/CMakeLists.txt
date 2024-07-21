add_library(core
    memory/dma.cpp
    memory/dma.h
    memory/flashram.cpp
    memory/flashram.h
    memory/memory.cpp
    memory/memory.h
    memory/pif.cpp
    memory/pif.h
    memory/pif2.h
    memory/savestates.cpp
    memory/savestates.h
    memory/summercart.cpp
    memory/summercart.h
    memory/tlb.cpp
    memory/tlb.h

    r4300/bc.cpp
    r4300/compare_core.cpp
    r4300/cop0.cpp
    r4300/cop1_d.cpp
    r4300/cop1_helpers.cpp
    r4300/cop1_helpers.h
    r4300/cop1_l.cpp
    r4300/cop1_s.cpp
    r4300/cop1_w.cpp
    r4300/cop1.cpp
    r4300/debugger.cpp
    r4300/debugger.h
    r4300/disasm.cpp
    r4300/disasm.h
    r4300/exception.cpp
    r4300/exception.h
    r4300/gameshark.cpp
    r4300/gameshark.h
    r4300/interrupt.cpp
    r4300/interrupt.h
    r4300/macros.h
    r4300/ops.h
    r4300/Plugin.cpp
    r4300/Plugin.hpp
    r4300/profile.cpp
    r4300/pure_interp.cpp
    r4300/r4300.cpp
    r4300/r4300.h
    r4300/recomp.cpp
    r4300/recomp.h
    r4300/recomph.h
    r4300/regimm.cpp
    r4300/rom.cpp
    r4300/rom.h
    r4300/special.cpp
    r4300/timers.cpp
    r4300/timers.h
    r4300/tlb.cpp
    r4300/tracelog.cpp
    r4300/tracelog.h
    r4300/vcr.cpp
    r4300/vcr.h
    r4300/x86/assemble.cpp
    r4300/x86/assemble.h
    r4300/x86/debug.cpp
    r4300/x86/gbc.cpp
    r4300/x86/gcop0.cpp
    r4300/x86/gcop1_d.cpp
    r4300/x86/gcop1_helpers.cpp
    r4300/x86/gcop1_helpers.h
    r4300/x86/gcop1_l.cpp
    r4300/x86/gcop1_s.cpp
    r4300/x86/gcop1_w.cpp
    r4300/x86/gcop1.cpp
    r4300/x86/gr4300.cpp
    r4300/x86/gregimm.cpp
    r4300/x86/gspecial.cpp
    r4300/x86/gtlb.cpp
    r4300/x86/interpret.h
    r4300/x86/regcache.cpp
    r4300/x86/regcache.h
    r4300/x86/rjump.cpp
)

target_link_libraries(core PRIVATE 
    lib_static 
    shared
    
    ${CMAKE_SOURCE_DIR}/lib/zlib/zlib-2008-x32.lib
    ${CMAKE_SOURCE_DIR}/lib/libdeflate/deflate.lib
    ${CMAKE_SOURCE_DIR}/lib/lua/lua54.lib
)

target_link_options(core PRIVATE 
    /SUBSYSTEM:WINDOWS
    /MACHINE:x86
    /NXCOMPAT:NO
)

# FIXME: Make this into a platform conditional
set_property(SOURCE r4300\tlb.cpp APPEND_STRING PROPERTY COMPILE_OPTIONS "/Fo r4300_tlb.o")

target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(core PRIVATE WIN32=1 _WIN32=1 _WINDOWS=1 __WIN32__=1 X86=1 _CRT_SECURE_NO_WARNINGS=1)