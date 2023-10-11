#include <core/loopy_io.h>
#include <unordered_map>
#include "input/input.h"

namespace Input
{

static std::unordered_map<int, PadButton> key_bindings;

void initialize()
{
	//Indicate the controller is connected
	LoopyIO::update_pad(PAD_PRESENCE, true);
}

void shutdown()
{
	//nop
}

void set_key_state(int key, bool pressed)
{
	auto binding = key_bindings.find(key);
	if (binding == key_bindings.end())
	{
		return;
	}

	PadButton button = binding->second;
	LoopyIO::update_pad(button, pressed);
}

void add_key_binding(int code, PadButton pad_button)
{
	key_bindings.emplace(code, pad_button);
}

}