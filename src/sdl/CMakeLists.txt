add_library (sdl STATIC
			 "sdl.cpp"
			 "sdl.h")

target_link_libraries (sdl PRIVATE SDL2-static)