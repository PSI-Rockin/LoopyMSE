#pragma once

namespace SH2
{

void initialize();
void shutdown();

void run();
void raise_exception(int vector_id);
void set_pc(uint32_t new_pc);

}