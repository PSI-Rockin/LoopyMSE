add_library (core STATIC
			 "config.h"
			 "memory.cpp"
			 "memory.h"
			 "system.cpp"
			 "system.h"
			 "timing.cpp"
			 "timing.h"

			 "sh2/sh2.cpp"
			 "sh2/sh2.h"
			 "sh2/sh2_bus.cpp"
			 "sh2/sh2_bus.h"
			 "sh2/sh2_interpreter.cpp"
			 "sh2/sh2_interpreter.h"
			 "sh2/sh2_local.h"
			 
			 "sh2/peripherals/sh2_dmac.cpp"
			 "sh2/peripherals/sh2_dmac.h"
			 "sh2/peripherals/sh2_intc.cpp"
			 "sh2/peripherals/sh2_intc.h"
			 "sh2/peripherals/sh2_ocpm.cpp"
			 "sh2/peripherals/sh2_ocpm.h"
			 "sh2/peripherals/sh2_timers.cpp"
			 "sh2/peripherals/sh2_timers.h")

target_link_libraries(core PRIVATE common video)