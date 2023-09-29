#pragma once
#include <cstdint>

namespace SDL
{

void initialize();
void shutdown();

void update(uint16_t* display_output);

}