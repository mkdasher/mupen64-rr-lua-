add_library(shared
    Config.cpp
    Config.hpp
    messenger.cpp
    messenger.h
    helpers/io_helpers.h
    helpers/string_helpers.h
    services/FrontendService.h
    services/IOService.h
    services/LuaService.h
    types/CoreTypes.h
)

target_link_libraries(shared PRIVATE 
    lib_static
    
    ${CMAKE_SOURCE_DIR}/lib/zlib/zlib-2008-x32.lib
    ${CMAKE_SOURCE_DIR}/lib/libdeflate/deflate.lib
    ${CMAKE_SOURCE_DIR}/lib/lua/lua54.lib
)

target_include_directories(shared PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})