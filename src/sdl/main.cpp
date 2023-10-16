#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <SDL.h>
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

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Args: [game ROM] [BIOS]\n");
        return 1;
    }

    SDL::initialize();

    char* cart_name = argv[1];
    char* bios_name = argv[2];

    Config::SystemInfo config = {};

    std::ifstream cart_file(cart_name, std::ios::binary);
    if (!cart_file.is_open())
    {
        printf("Failed to open %s\n", cart_name);
        return 1;
    }

    config.cart_rom.assign(std::istreambuf_iterator<char>(cart_file), {});
    cart_file.close();

    std::ifstream bios_file(bios_name, std::ios::binary);
    if (!bios_file.is_open())
    {
        printf("Failed to open %s\n", bios_name);
        return 1;
    }

    config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
    bios_file.close();

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
