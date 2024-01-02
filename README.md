# C++ abstract layer for FreeRTOS

## Doxygen documentation:
https://virviglaz.github.io/FreeRTOS_abstract/

## Task handling
### Static allocation:
~~~cpp
FreeRTOS::Task<> task1([](void *) {
	std::cout << "Task started" << std::endl;
	while (1) { .... your code .... }
});
~~~

### Dynamic allocation:
~~~cpp
	FreeRTOS::Task<> *task3 = new FreeRTOS::Task<>([](void *) {
		while (1) {
			FreeRTOS::Delay_ms(250);
			std::cout << "Task 3 working..." << std::endl;
		}
	});
~~~
### Manual allocation:
You can allocate memory for task (stack + internal data)
from any place you want. This is handy for embedded systems.
~~~cpp
	/* Allocate using FreeRTOS malloc */
	void *ptr = FreeRTOS::malloc(configMINIMAL_STACK_SIZE + 70);
	FreeRTOS::Task<> *task4 = new(ptr) FreeRTOS::Task<>([](void *) {
		while (1) {
			FreeRTOS::Delay_ms(250);
			std::cout << "Task 4 working..." << std::endl;
		}
	});

	/* Dynamic allocation using static memory */
	static char ptr2[configMINIMAL_STACK_SIZE + 70];
	FreeRTOS::Task<> *task5 = new(ptr2) FreeRTOS::Task<>([](void *) {
		while (1) {
			FreeRTOS::Delay_ms(250);
			std::cout << "Task 5 working..." << std::endl;
		}
	});
~~~

## Queue
~~~cpp
FreeRTOS::Queue<std::string, 10> q;

std::string test_msg { "Test string" };
if (q.TryPushBack(&test_msg))
			std::cout << "Data transmitted" << std::endl;

auto ret = q.TryPop();
		if (ret) {
			std::cout << "Data: " << *ret << std::endl;
		}
~~~

## Locks
~~~cpp
FreeRTOS::Mutex lock;
FreeRTOS::BinarySemaphore waiter;
FreeRTOS::CountingSemaphore counter;

lock.Lock();
lock.Unlock();
~~~

## Timer
~~~cpp
FreeRTOS::Timer tim([](TimerHandle_t t) {
	(void)t;
	std::cout << "Timer callback" << std::endl;
}, 1000); /* every 1s */

tim.Start();
~~~

## Misc
~~~cpp
/* Start kernel scheduller */
FreeRTOS::StartScheduller();

/* ms delays */
FreeRTOS::Task::Delay_ms(1000);
~~~

## Testing
~~~cpp
/* Standard includes. */
#include <stdio.h>
#include <iostream>
#include <stdio.h>
#include <errno.h>

#include "FreeRTOS_abstract.h"

FreeRTOS::Queue<std::string, 10> q;
FreeRTOS::Mutex lock;
int task_err = ENOMSG;
int tim_err = ENOMSG;

FreeRTOS::Task<> task1([](void *) {
	std::cout << "Task transmitted started" << std::endl;
	while (1) {
		FreeRTOS::Delay_ms(100);
		std::string test_msg { "Test string" };
		if (q.TryPush(&test_msg)) {
			lock.Lock();
			std::cout << "Data transmitted" << std::endl;
			lock.Unlock();
		}
	}
});

FreeRTOS::Task<> task2([](void *) {
	std::cout << "Task receiver started" << std::endl;
	while(1) {
		auto ret = q.TryPop(1000);
		if (ret) {
			lock.Lock();
			std::cout << "Data: " << *ret << std::endl;
			task_err = 0;
			lock.Unlock();
		}
	}
});

FreeRTOS::Timer tim([](TimerHandle_t t) {
	(void)t;
	lock.Lock();
	std::cout << "Timer callback" << std::endl;
	tim_err = 0;
	lock.Unlock();
}, 100);

FreeRTOS::Timer job_done([](TimerHandle_t t) {
	(void)t;
	FreeRTOS::StopScheduller();
}, 500);

int main()
{
	FreeRTOS::Task<> *task3 = new FreeRTOS::Task<>([](void *) {
		while (1) {
			FreeRTOS::Delay_ms(250);
			std::cout << "Task 3 working..." << std::endl;
		}
	});

	tim.Start();
	job_done.Start();
	FreeRTOS::StartScheduller();

	if (task_err)
		std::cerr << "Consumer task never executed" << std::endl;

	if (tim_err)
		std::cerr << "Timer handler never called" << std::endl;

	return task_err | tim_err;
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
		StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
		StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
#endif
~~~