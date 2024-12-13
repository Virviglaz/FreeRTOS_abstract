#ifndef __FREERTOS_ABSTRACT__
#define __FREERTOS_ABSTRACT__

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "croutine.h"
#include "FreeRTOSConfig.h"

/* LIBC */
#include <stdint.h>
#include <string.h>
#include <type_traits>
#include <utility>

/**
 * @Note
 * To use FreeRTOS memory allocations:
 * 1. Add linker flag: -Wl,--wrap,malloc,--wrap,free
 * 2. Add wrapper functions: (use extern "C" if placed in cpp file)
 *		void *__wrap_malloc(size_t size) { return pvPortMalloc(size); }
 *		void __wrap_free(void *ptr) { vPortFree(ptr); }
 *
 * if configSUPPORT_STATIC_ALLOCATION is set, add this function:
 *	void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
 *		StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
 *	{
 *		static StaticTask_t xIdleTaskTCBBuffer;
 *		static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];
 *		*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
 *		*ppxIdleTaskStackBuffer = xIdleStack;
 *		*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
 *	}
 */

#ifndef FREERTOS_DEFAULT_TASK_STACK_SIZE
#define FREERTOS_DEFAULT_TASK_STACK_SIZE		configMINIMAL_STACK_SIZE
#endif

#define WAIT_MAX					portMAX_DELAY

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define FREERTOS_NODISCARD [[nodiscard]]
#endif
#endif
#ifndef FREERTOS_NODISCARD
#define FREERTOS_NODISCARD
#endif

namespace FreeRTOS
{
	/**
	 * @brief Starts the RTOS scheduler.
	 *
	 * After calling the RTOS kernel has control
	 * over which tasks are executed and when.
	 */
	inline void StartScheduler() {
		vTaskStartScheduler();
	}

	/**
	 * @brief Stops the RTOS kernel tick.
	 *
	 * All created tasks will be automatically deleted and multitasking
	 * (either preemptive or cooperative) will stop.
	 * Execution then resumes from the point where vTaskStartScheduler()
	 * was called, as if vTaskStartScheduler() had just returned.
	 */
	inline void StopScheduler() {
		vTaskEndScheduler();
	}

	/**
	 * @brief Implement locking mechanism between tasks to protect shared
	 * resources against race conditions.
	 */
	class Mutex
	{
	protected:
#if (configSUPPORT_STATIC_ALLOCATION == 1)
		StaticSemaphore_t buffer;
#endif /* STATIC_ALLOCATION */
		SemaphoreHandle_t handle = nullptr;
	public:
		/**
		 * @brief Creates an instance of mutex.
		 */
		Mutex() {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
			handle = xSemaphoreCreateMutexStatic(&buffer);
#else /* STATIC_ALLOCATION */
			handle = xSemaphoreCreateMutex();
			configASSERT(handle != nullptr);
#endif /* STATIC_ALLOCATION */
		}

		/**
		 * @brief Deletes the instance and release allocated memory.
		 */
		~Mutex() {
			if (handle)
				vSemaphoreDelete(handle);
			handle = nullptr;
		}

		/**
		 * @brief Obtain a mutex.
		 *
		 * @param[in] wait_ms	[Optional] Amount of milliseconds to
		 *			wait for resource to be available.
		 *
		 * @return true if success, false on timeout.
		 */
		bool Lock(size_t wait_ms = WAIT_MAX) {
			return xSemaphoreTake(handle,
			                      pdMS_TO_TICKS(wait_ms)) == pdTRUE;
		}

		/**
		 * @brief Release an obtained mutex.
		 *
		 * @return true if success,
		 * false if no mutex was obtained before.
		 */
		bool Unlock() {
			return xSemaphoreGive(handle) == pdTRUE;
		}

		/**
		 * @brief Prevent class to be copied.
		 */
		Mutex(const Mutex &) = delete;

		/**
		 * @brief Obtain a mutex from interrupt context.
		 *
		 * @return true if success, false if failed.
		 */
		bool LockFromISR() {
			return xSemaphoreTakeFromISR(handle, nullptr) == pdTRUE;
		}

		/**
		 * @brief Release an obtained mutex from interrupt context.
		 *
		 * @return true if success,
		 * false if no mutex was obtained before.
		 */
		bool UnlockFromISR() {
			return xSemaphoreGiveFromISR(handle, nullptr) == pdTRUE;
		}
	};

	/**
	 * @brief Create binary semaphore.
	 */
	class BinarySemaphore : public Mutex
	{
	private:
		using Mutex::Mutex;
		using Mutex::Lock;
		using Mutex::LockFromISR;
		using Mutex::Unlock;
		using Mutex::UnlockFromISR;
	public:
		/**
		 * @brief Create a binary semaphore.
		 */
		BinarySemaphore() {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
			handle = xSemaphoreCreateBinaryStatic(&buffer);
#else /* STATIC_ALLOCATION */
			handle = xSemaphoreCreateBinary();
			configASSERT(handle != nullptr);
#endif /* STATIC_ALLOCATION */
		}

		/**
		 * @brief Give semaphore.
		 *
		 * @return true if given, false if already given before.
		 */
		inline bool Give() {
			return Mutex::Unlock();
		}

		/**
		 * @brief Wait for semaphore to be given.
		 *
		 * @param[in] wait_ms	[Optional] Time to wait in [ms].
		 *
		 * @return true if semaphore obtained successfully, false
		 * on timeout.
		 */
		inline bool Take(size_t wait_ms = WAIT_MAX) {
			return Mutex::Lock(wait_ms);
		}

		/**
		 * @brief Give semaphore from ISR.
		 *
		 * @return true if given, false if already given before.
		 */
		inline bool GiveFromISR() {
			return Mutex::UnlockFromISR();
		}

		/**
		 * @brief Wait for semaphore to be given from ISR.
		 *
		 * @return true if semaphore obtained successfully, false
		 * on timeout.
		 */
		inline bool TakeFromISR() {
			return Mutex::LockFromISR();
		}
	};

	/**
	 * @brief Create counting semaphore.
	 */
	class CountingSemaphore : public BinarySemaphore
	{
	public:
		/**
		 * @brief Create counting semaphore with initial count and max
		 * value.
		 *
		 * @param[in] initial		Initial count value.
		 * @param[in] max		The maximum count value that can
		 *				be reached. When the semaphore
		 *				reaches this value it can no
		 *				longer be 'given'.
		 */
		CountingSemaphore(size_t initial = 0, size_t max = 100) {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
			handle = xSemaphoreCreateCountingStatic(max, initial,
			                                        &buffer);
#else /* STATIC_ALLOCATION */
			handle = xSemaphoreCreateCounting(max, initial);
			configASSERT(handle != nullptr);
#endif /* STATIC_ALLOCATION */
		}

		/**
		 * @brief Read current count value.
		 *
		 * @return Count value.
		 */
		FREERTOS_NODISCARD
		size_t GetCount() {
			return uxSemaphoreGetCount(handle);
		}
	};

	/**
	 * @brief Responsible for creating, control and delete FreeRTOS tasks.
	 *
	 * @tparam stack_size		[Optional] The number of words
	 *				(not bytes) for use as the task's stack.
	 */
	template <const size_t stack_size = FREERTOS_DEFAULT_TASK_STACK_SIZE>
	class Task
	{
	protected:
		TaskHandle_t handle = nullptr;
#if (configSUPPORT_STATIC_ALLOCATION == 1)
		StackType_t xStack[stack_size];
		StaticTask_t xTaskBuffer;
#endif /* STATIC_ALLOCATION */
	public:
		/**
		 * @brief Create a new task and
		 * add it to the list of tasks that are ready to run.
		 *
		 * @param[in] handler	Pointer to the task entry function.
		 * @param[in] params	[Optional] A value that is passed as the
		 *			parameter to the created task.
		 * @param[in] name	[Optional] Task name string.
		 * @param[in] priority	[Optional] The priority at which the
		 *			task will execute.
		 */
		Task(void (*handler)(void *),
			void *params = nullptr,
			const char *name = nullptr,
			int priority = tskIDLE_PRIORITY + 1) {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
			handle = xTaskCreateStatic(handler,
			                           name,
			                           stack_size,
			                           params,
			                           tskIDLE_PRIORITY + priority,
			                           xStack,
			                           &xTaskBuffer);
			configASSERT(handle != nullptr);
#else /* STATIC_ALLOCATION */
			configASSERT(xTaskCreate(handler,
			                         name,
			                         stack_size,
			                         params,
			                         tskIDLE_PRIORITY + priority,
			                         &handle) == pdPASS);
#endif /* STATIC_ALLOCATION */
		}

		~Task() { Delete(); }

#if (INCLUDE_vTaskDelete == 1)
		/**
		 * @brief Delete current task.
		 */
		void Delete() {
			if (handle)
				vTaskDelete(handle);
			handle = nullptr;
		}

		static void SelfDelete() {
			vTaskDelete(nullptr);
		}
#endif /* INCLUDE_vTaskDelete */

#if (INCLUDE_vTaskSuspend == 1)
		/**
		 * @brief Put current task into idle state.
		 */
		void Suspend() {
			vTaskSuspend(handle);
		}
#endif /* INCLUDE_vTaskSuspend */

#if (INCLUDE_vTaskSuspend == 1)
		/**
		 * @brief Wake up current task.
		 */
		void Resume() {
			vTaskResume(handle);
		}
#endif /* INCLUDE_vTaskSuspend */

#if (INCLUDE_eTaskGetState == 1)
		/**
		 * @brief Get current task state.
		 */
		enum TaskState { Ready, Running, Blocked, Suspended, Deleted };
		FREERTOS_NODISCARD
		TaskState GetState() {
			eTaskState state = eTaskGetState(handle);
			return (TaskState)state;
		}
#endif /* INCLUDE_eTaskGetState */

		/**
		 * @brief Suspends the scheduler.
		 *
		 * Suspending the scheduler prevents a context switch from
		 * occurring but leaves interrupts enabled. If an interrupt
		 * requests a context switch while the scheduler is suspended,
		 * then the request is held pending and is performed only when
		 * the scheduler is resumed (un-suspended).
		 */
		static inline void SuspendAll() {
			vTaskSuspendAll();
		}

		/**
		 * @brief Resumes the scheduler after it was suspended using
		 * a call to vTaskSuspendAll().
		 */
		static inline void ResumeAll() {
			xTaskResumeAll();
		}

		/**
		 * @brief taskYIELD() is used to request a context switch to
		 * another task. However, if there are no other tasks at a
		 * higher or equal priority to the task that calls taskYIELD()
		 * then the RTOS scheduler will simply select the task that
		 * called taskYIELD() to run again.
		 */
		static inline void DoYield() {
			taskYIELD();
		}

		/**
		 * @brief Critical sections are entered by calling
		 * taskENTER_CRITICAL(), and subsequently exited by calling
		 * taskEXIT_CRITICAL().
		 */
		static inline void EnterCritical() {
			taskENTER_CRITICAL();
		}

		/**
		 * @brief Critical sections are entered by calling
		 * taskENTER_CRITICAL(), and subsequently exited by calling
		 * taskEXIT_CRITICAL().
		 *
		 * The taskENTER_CRITICAL() and taskEXIT_CRITICAL() macros
		 * provide a basic critical section implementation that works
		 * by simply disabling interrupts, either globally, or up to a
		 * specific interrupt priority level. See the vTaskSuspendAll()
		 * RTOS API function for information on creating a critical
		 * section without disabling interrupts.
		 */
		static inline void ExitCritical() {
			taskEXIT_CRITICAL();
		}

		/**
		 * @brief Prevent class to be copied.
		 */
		Task(const Task &) = delete;

#if (configUSE_TASK_NOTIFICATIONS == 1)
		/**
		 * @brief Send notify to task.
		 *
		 * @param[in] index		[Optional] he index within the
		 *				target task's array of
		 *				notification values to which the
		 *				notification is to be sent.
		 */
		void NotifyGive(int index = -1) {
			if (index >= 0) {
				configASSERT(xTaskNotifyGiveIndexed(handle,
					index) == pdPASS);
			} else {
				configASSERT(xTaskNotifyGive(handle) == pdPASS);
			}
		}

		/**
		 * @brief Send notify to task from ISR.
		 *
		 * @param[in] index		[Optional] he index within the
		 *				target task's array of
		 *				notification values to which the
		 *				notification is to be sent.
		 */
		void NotifyGiveFromISR(int index = -1) {
			if (index >= 0) {
				vTaskNotifyGiveIndexedFromISR(handle, index,
				                              nullptr);
			} else {
				vTaskNotifyGiveFromISR(handle, nullptr);
			}
		}

		/**
		 * @brief Wait for notify.
		 *
		 * @param wait_ms		Time to wait in [ms].
		 * @param reset			[Optional] Reset notify after.
		 * @param index			[Optional] Index to wait for.
		 *
		 * @return The value of the task's notification value before it
		 * is decremented or cleared.
		 */
		uint32_t NotifyTake(size_t wait_ms,
		                    bool reset = true,
		                    int index = -1) {
			if (index >= 0) {
				return ulTaskNotifyTakeIndexed(index, reset,
					pdMS_TO_TICKS(wait_ms));
			} else {
				return ulTaskNotifyTake(reset,
					pdMS_TO_TICKS(wait_ms));
			}
		}
#endif /* configUSE_TASK_NOTIFICATIONS */

		/**
		 * Critical section control can be used from interrupt context.
		 */
		class Isr
		{
		protected:
			static inline UBaseType_t uxSavedInterruptStatus;
		public:
			/**
			 * @brief See Task::EnterCritical()
			 */
			static void EnterCritical() {
				uxSavedInterruptStatus =
						taskENTER_CRITICAL_FROM_ISR();
			}

			/**
			 * @brief See Task::ExitCritical()
			 */
			static void ExitCritical() {
				taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
			}
		};

#if (INCLUDE_vTaskDelete == 1)
		/**
		 * Class to execute code asynchronously.
		 */
		class DoAsync
		{
		protected:
			void (*async_job)();
			BinarySemaphore done;
#if (configSUPPORT_STATIC_ALLOCATION == 1)
			StackType_t xStack[stack_size];
			StaticTask_t xTaskBuffer;
#endif /* STATIC_ALLOCATION */
			static void handler(void *args) {
				DoAsync *c = static_cast<DoAsync *>(args);
				c->async_job();
				c->done.Give();
				Task::SelfDelete();
			}
		public:
			/**
			 * @brief Calls pointer to function from new task.
			 * After job is done, task is self deleted.
			 *
			 * @param job		Callback function to call
			 *			asynchronously.
			 * @param priority	[Optional] Priority
			 */
			DoAsync(void (*job)(),
			        size_t priority = 1) {
				async_job = job;
#if (configSUPPORT_STATIC_ALLOCATION == 1)
				configASSERT(xTaskCreateStatic(handler,
			                           nullptr,
			                           stack_size,
			                           this,
			                           tskIDLE_PRIORITY + priority,
			                           xStack,
			                           &xTaskBuffer) != nullptr);
#else /* STATIC_ALLOCATION */
				configASSERT(xTaskCreate(handler,
			                         nullptr,
			                         stack_size,
			                         this,
			                         tskIDLE_PRIORITY + priority,
			                         nullptr) == pdPASS);
#endif /* STATIC_ALLOCATION */
			}
			~DoAsync() {
				done.Take(WAIT_MAX);
			}
		};
#endif /* INCLUDE_vTaskDelete */
	};

#if (INCLUDE_vTaskDelay == 1)
	/**
	 * @brief Suspend the task execution for given amount of milliseconds.
	 *
	 * @param[in] ms		Amount of milliseconds to wait.
	 */
	static inline void Delay_ms(size_t ms) {
		vTaskDelay(pdMS_TO_TICKS(ms));
	}
#endif /* INCLUDE_vTaskDelay */

#if (INCLUDE_vTaskDelayUntil == 1)
	/**
	 * @brief Enables accurate periodic execution.
	 */
	class TimerDelay
	{
	protected:
		TickType_t xLastWakeTime = 0;
	public:
		/**
		 * @brief Constructor of TimerDelay class.
		 *
		 * @param start		Initiate at the moment of creation if enabled.
		 */
		TimerDelay(bool start = true) {
			if (start)
				Reset();
		}

		/**
		 * @brief Reset the time stamp value.
		 */
		void Reset() {
			xLastWakeTime = xTaskGetTickCount();
		}

		/**
		 * Wait given amount ms since last call.
		 *
		 * @param ms Maximum waiting time in [ms].
		 */
		void Wait(size_t ms) {
			vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(ms));
		}
	};
#endif

	/**
	 * @brief Creates a new FreeRTOS queue instance.
	 *
	 * @Note This is MPSC queue (multi producers, single consumer queue).
	 * If you need to use MPMC queue, you have to block the kernel until
	 * popped pointer is processed or copied.
	 *
	 * @tparam T		Type of object to be enqueued.
	 * @tparam size		Maximum queue size
	 *			(maximum amount of object to store).
	 */
	template <class T, size_t size = 2>
	class Queue
	{
	protected:
#if (configSUPPORT_STATIC_ALLOCATION == 1)
		StaticQueue_t xStaticQueue;
#endif /* STATIC_ALLOCATION */
		CountingSemaphore semaphore {0, size};
		T buffer[size];
		size_t rd_idx = 0;
		size_t wr_idx = 0;
	public:
		/**
		 * @brief Default constructor.
		 */
		Queue() {}

		/**
		 * @brief Prevent class to be copied or moved.
		 */
		Queue(const Queue&) = delete;
		Queue(Queue&&) = delete;

		/**
		 * @brief Construct an item at the back place of a queue.
		 *
		 * This function must not be called from an interrupt service
		 * routine.
		 *
		 * @param[in] args	T constructor arguments.
		 * @return true if item posted in the queue,
		 * false when no space left.
		 */
		template <typename... Args>
		bool TryEmplaceBack(Args &&...args) noexcept (
			std::is_nothrow_constructible<T, Args &&...>::value) {
			static_assert(
				std::is_constructible<T, Args &&...>::value,
				"T must be constructible with Args&&...");

			size_t next_wr_idx = wr_idx + 1;
			if (next_wr_idx == size)
				next_wr_idx = 0;

			if (next_wr_idx == rd_idx)
				return false;

			new (&buffer[wr_idx]) T(std::forward<Args>(args)...);
			semaphore.Give();
			wr_idx = next_wr_idx;
			return true;
		}

		/**
		 * @brief Post an item to the back of a queue.
		 *
		 * This function must not be called from an interrupt service
		 * routine.
		 *
		 * @param[in] item	Pointer to item to post.
		 *
		 * @return true if item posted in the queue, false when
		 * no space left.
		 */
		bool TryPushBack(const T &item) noexcept {
			return TryEmplaceBack(item);
		}

		/**
		 * @brief Receive an item from a queue.
		 *
		 * @param[in] wait_ms	[Optional] The maximum amount of time
		 *			the task should block waiting for an
		 *			item to appear in the queue.
		 *
		 * @return Pointer to item in the buffer or nullptr if queue is
		 * empty.
		 */
		T* Front(size_t wait_ms = 0) noexcept {
			if (wr_idx == rd_idx) {
				if (wait_ms) {
					if (!semaphore.Take(wait_ms))
						return nullptr;
				} else
					return nullptr;
			}

			return &buffer[rd_idx];
		}

		void Pop() noexcept {
			static_assert(std::is_nothrow_destructible<T>::value,
					"T must be nothrow destructible");
			buffer[rd_idx].~T();
			rd_idx++;
			if (rd_idx == size)
				rd_idx = 0;
		}
	};

#if (configUSE_TIMERS == 1)
	/**
	 * @brief FreeRTOS software timer.
	 */
	class Timer
	{
	protected:
		TimerHandle_t handle = nullptr;
#if (configSUPPORT_STATIC_ALLOCATION == 1)
		StaticTimer_t xTimerBuffers;
#endif
	public:
		/**
		 * @brief Creates instance of software timer.
		 *
		 * @param[in] callback		The function to call when the
		 *				timer expires.
		 * @param[in] period_ms		Timer expire time in [ms].
		 * @param[in] autoreload	false to single shot, true for
		 *				repeat.
		 * @param[in] name		Timer name string for debugging
		 *				purpose.
		 * @param[in] timer_id		An identifier that is assigned
		 *				to the timer being created.
		 *				Typically this would be used
		 *				in the timer callback function
		 *				to identify which timer expired
		 *				when the same callback function
		 *				is assigned to more than one
		 *				timer.
		 */
		Timer(void (*callback)(TimerHandle_t handle),
			size_t period_ms,
			bool autoreload = true,
			const char *name = nullptr,
			void *timer_id = nullptr) {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
			handle = xTimerCreateStatic(name,
				pdMS_TO_TICKS(period_ms),
				autoreload, timer_id,
				callback, &xTimerBuffers);
#else /* STATIC_ALLOCATION */
			handle = xTimerCreate(name,
				pdMS_TO_TICKS(period_ms),
				autoreload, timer_id,
				callback);
#endif /* STATIC_ALLOCATION */
			configASSERT(handle != nullptr);
		}

		/**
		 * @brief Delete timer and release allocated memory.
		 */
		~Timer() {
			Delete();
		}

		/**
		 * @brief Prevent class to be copied.
		 */
		Timer(const Timer &) = delete;

		/**
		 * @brief Delete timer and release allocated memory.
		 */
		void Delete(size_t wait_ms = 0) {
			if (handle)
				configASSERT(xTimerDelete(handle,
					pdMS_TO_TICKS(wait_ms)) == pdPASS);
			handle = nullptr;
		}

		/**
		 * @brief Start the timer.
		 *
		 * wait_ms		[Optional] Time after the timer should
		 *			start.
		 */
		void Start(size_t wait_ms = 0) {
			configASSERT(xTimerStart(handle,
				pdMS_TO_TICKS(wait_ms)) == pdPASS);
		}

		/**
		 * @brief Stop the timer.
		 *
		 * @param[in] wait_ms		[Optional] Specifies the time,
		 *				in [ms], that the calling task
		 *				should be held in the Blocked
		 *				state to wait for the stop
		 *				command to be successfully.
		 */

		void Stop(size_t wait_ms = 0) {
			configASSERT(xTimerStop(handle,
				pdMS_TO_TICKS(wait_ms)) == pdPASS);
		}
	};
#endif /* configUSE_TIMERS */
}

#endif /* __FREERTOS_ABSTRACT__ */
