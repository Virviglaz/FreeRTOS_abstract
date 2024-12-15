#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"

#if (configSUPPORT_STATIC_ALLOCATION == 1)
__attribute__((used))
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
		StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
	static StaticTask_t xIdleTaskTCBBuffer;
	static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = xIdleStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */

#if defined(configASSERT)
void assert_failed(void)
{
	while (1) { }
}
#endif /* configASSERT */

void *__wrap_malloc(size_t size) {
	return pvPortMalloc(size);
}

void __wrap_free(void *ptr) {
	vPortFree(ptr);
}
