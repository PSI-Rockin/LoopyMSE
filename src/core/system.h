#pragma once
#include "core/config.h"

namespace System
{

void initialize(Config::SystemInfo& config);
void shutdown();

void run();

}