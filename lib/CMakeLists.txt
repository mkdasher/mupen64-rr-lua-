add_library(lib_static STATIC
    argh.h
    ini.h
    md5.c
    md5.h
    microlru.h
    libdeflate/libdeflate.h
    lua/include/lauxlib.h
    lua/include/lua.h
    lua/include/lua.hpp
    lua/include/luaconf.h
    lua/include/lualib.h
    speex/arch.h
    speex/speex_resampler.c
    speex/speex_resampler.h
    xxhash/xxh64.h
    zlib/zlib.h
)
target_include_directories(lib_static PUBLIC ${CMAKE_SOURCE_DIR}/lib)