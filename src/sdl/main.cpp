#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <SDL.h>

#include <common/bswp.h>
#include <core/config.h>
#include <core/system.h>
#include <input/input.h>
#include <video/video.h>

namespace SDL
{

using Video::DISPLAY_HEIGHT;
using Video::DISPLAY_WIDTH;

struct Screen
{
	SDL_Renderer* renderer;
	SDL_Window* window;
	SDL_Texture* texture;
};

static Screen screen;

void initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Failed to initialize SDL2: %s\n", SDL_GetError());

        exit(0);
    }

    //Try synchronizing drawing to VBLANK
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    //Set up SDL screen
    SDL_CreateWindowAndRenderer(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, &screen.window, &screen.renderer);
    SDL_SetWindowTitle(screen.window, "Rupi");
    SDL_SetWindowSize(screen.window, 2 * DISPLAY_WIDTH, 2 * DISPLAY_HEIGHT);
    SDL_SetWindowResizable(screen.window, SDL_FALSE);
    SDL_RenderSetLogicalSize(screen.renderer, 2 * DISPLAY_WIDTH, 2 * DISPLAY_HEIGHT);

    screen.texture = SDL_CreateTexture(screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void shutdown() {
    //Destroy window, then kill SDL2
    SDL_DestroyTexture(screen.texture);
    SDL_DestroyRenderer(screen.renderer);
    SDL_DestroyWindow(screen.window);

    SDL_Quit();
}

void update(uint16_t* display_output)
{
    // Draw screen
    SDL_UpdateTexture(screen.texture, NULL, display_output, sizeof(uint16_t) * DISPLAY_WIDTH);
    SDL_RenderCopy(screen.renderer, screen.texture, NULL, NULL);
    SDL_RenderPresent(screen.renderer);
}

}

std::string remove_extension(std::string file_path)
{
    auto pos = file_path.find(".");
    if (pos == std::string::npos)
    {
        return file_path;
    }

    return file_path.substr(0, pos);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Args: [game ROM] [BIOS]\n");
        return 1;
    }

    SDL::initialize();

    std::string cart_name = argv[1];
    std::string bios_name = argv[2];

    Config::SystemInfo config = {};

    std::ifstream cart_file(cart_name, std::ios::binary);
    if (!cart_file.is_open())
    {
        printf("Failed to open %s\n", cart_name.c_str());
        return 1;
    }

    config.cart.rom.assign(std::istreambuf_iterator<char>(cart_file), {});
    cart_file.close();

    std::ifstream bios_file(bios_name, std::ios::binary);
    if (!bios_file.is_open())
    {
        printf("Failed to open %s\n", bios_name.c_str());
        return 1;
    }

    config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
    bios_file.close();

    //Determine the size of SRAM from the cartridge header
    uint32_t sram_start, sram_end;
    memcpy(&sram_start, config.cart.rom.data() + 0x10, 4);
    memcpy(&sram_end, config.cart.rom.data() + 0x14, 4);
    uint32_t sram_size = Common::bswp32(sram_end) - Common::bswp32(sram_start) + 1;

    //Attempt to load SRAM from a file
    config.cart.sram_file_path = remove_extension(cart_name) + ".sav";
    std::ifstream sram_file(config.cart.sram_file_path, std::ios::binary);
    if (!sram_file.is_open())
    {
        printf("Warning: SRAM not found\n");
    }
    else
    {
        printf("Successfully found SRAM\n");
        config.cart.sram.assign(std::istreambuf_iterator<char>(sram_file), {});
        sram_file.close();
    }

    //Ensure SRAM is at the proper size. If no file is loaded, it will be filled with 0xFF.
    //If a file was loaded but was smaller than the SRAM size, the uninitialized bytes will be 0xFF.
    //If the file was larger, then the vector size is clamped
    config.cart.sram.resize(sram_size, 0xFF);

    //Initialize the emulator and all of its subprojects
    System::initialize(config);

    //All subprojects have been initialized, so it is safe to reference them now
    Input::add_key_binding(SDLK_RETURN, Input::PAD_START);

    Input::add_key_binding(SDLK_z, Input::PAD_A);
    Input::add_key_binding(SDLK_x, Input::PAD_B);
    Input::add_key_binding(SDLK_a, Input::PAD_C);
    Input::add_key_binding(SDLK_s, Input::PAD_D);

    Input::add_key_binding(SDLK_q, Input::PAD_L1);
    Input::add_key_binding(SDLK_w, Input::PAD_R1);

    Input::add_key_binding(SDLK_LEFT, Input::PAD_LEFT);
    Input::add_key_binding(SDLK_RIGHT, Input::PAD_RIGHT);
    Input::add_key_binding(SDLK_UP, Input::PAD_UP);
    Input::add_key_binding(SDLK_DOWN, Input::PAD_DOWN);
    
    bool has_quit = false;
    while (!has_quit)
    {
        System::run();
        SDL::update(System::get_display_output());

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                has_quit = true;
                break;
            case SDL_KEYDOWN:
                Input::set_key_state(e.key.keysym.sym, true);
                break;
            case SDL_KEYUP:
                Input::set_key_state(e.key.keysym.sym, false);
                break;
            }
        }
    }
    
    System::shutdown();
    SDL::shutdown();

    return 0;
}
