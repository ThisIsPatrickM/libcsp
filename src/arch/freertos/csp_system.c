#include <csp/csp_hooks.h>

#include <FreeRTOS.h>
#include <task.h>

__attribute__((weak)) uint32_t csp_memfree_hook(void) {
#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
	return (uint32_t)xPortGetFreeHeapSize();
#else
	return 0;
#endif
}
