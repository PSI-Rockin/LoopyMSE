add_library (core STATIC
			 "config.h"
			 "memory.cpp"
			 "memory.h"
			 "system.cpp"
			 "system.h"

			 "sh2/sh2.cpp"
			 "sh2/sh2.h"
			 "sh2/sh2_bus.cpp"
			 "sh2/sh2_bus.h"
			 "sh2/sh2_interpreter.cpp"
			 "sh2/sh2_interpreter.h"
			 "sh2/sh2_local.h")