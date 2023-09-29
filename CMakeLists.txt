cmake_minimum_required (VERSION 3.8)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

project (Rupi LANGUAGES C CXX ASM)

string( REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

add_compile_definitions (SDL_MAIN_HANDLED) # Prevents SDL from touching main()

set (SDL_STATIC ON CACHE BOOL "" FORCE)
set (SDL_SHARED OFF CACHE BOOL "" FORCE)
set (SDL_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory (src)
add_subdirectory (externals/SDL)
