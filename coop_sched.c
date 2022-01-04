#include "coop_sched.h"

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <string.h>

#include "platform.h"

// Main task represents the task from which all other tasks are started
static coop_task_t main_task = {0};
static coop_task_t *tasks_running = &main_task, *current_task = &main_task;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wpacked"

typedef struct context {
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11,   // Context stored explicitly (by software)
        exc_return;                              // Exception return code (lr in exception scope)
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;  // Context stored implicitly (by hardware)
} __attribute__((packed)) context_t;

#pragma GCC diagnostic pop

#define STACK_WATERMARK_MAGIC 0xa5

#define BIT(x) (1 << x)

// Cortex-M4 Devices Generic User Guide, Table 2-17, Exception return behavior
#define EXC_RET_MAGIC 0xFFFFFFFD

// Reset these bits from EXC_RET_MAGIC to assert
#define EXC_RET_FP_BIT BIT(4)
#define EXC_RET_HANDLER_BIT BIT(3)
#define EXC_RET_MSP_BIT BIT(2)

#define EXC_RET_HANDLER (EXC_RET_MAGIC & ~(EXC_RET_HANDLER_BIT | EXC_RET_MSP_BIT))
#define EXC_RET_THREAD_MSP (EXC_RET_MAGIC & ~(EXC_RET_MSP_BIT))
#define EXC_RET_THREAD_PSP (EXC_RET_MAGIC)

void __attribute__((weak)) emergency_print(const char* str) {}

// Prototype to make gcc happy (inline asm doesn't find static functions)
uintptr_t context_switch(uintptr_t sp);

// Context switcher; called from PendSV_Handler
uintptr_t context_switch(uintptr_t sp)
{
    // Save current task's stackpointer
    current_task->sp_current = sp;
    // Simple round-robin scheduling
    current_task = current_task->next;
    if (!current_task) {
        // End of list reached; start again at beginning
        current_task = tasks_running;
    }
    // Return next task's stackpointer
    return current_task->sp_current;
}

// Add task to list of running tasks
static void task_insert(coop_task_t* t, coop_task_t** list)
{
    // Find last task in list
    coop_task_t* tail;
    for (tail = *list; tail->next; tail = tail->next) {
    }
    tail->next = t;
    // This is now the last element in the list
    t->next = NULL;
}

// Remove task from list of running tasks
// This will not work if the task to remove is on top of the list;
// we assume that the main task runs forever and is never removed.
static void task_remove(coop_task_t* t, coop_task_t** list)
{
    coop_task_t* prev;
    for (prev = *list; prev->next && prev->next != t; prev = prev->next) {
    }
    if (prev->next != t) {
        // List element not found
        return;
    }
    // Make previous element point to the one after
    prev->next = t->next;
    // Tie any loose ends for the scheduler
    t->next = NULL;
}

// Run the task function, then remove task and trigger scheduler
static void noreturn task_wrapper(coop_task_t* task, coop_task_fn_t task_fn, void* task_arg)
{
    task_fn(task_arg);

    task_remove(task, &tasks_running);
    sched_yield();

    emergency_print("internal error: return from scheduler\r\n");
    // TODO throw exception
    // make noreturn happy
    while (1) {
    };
}

void sched_create_task(coop_task_t* task,
                       coop_task_fn_t task_fn,
                       void* task_arg,
                       uint8_t* stack,
                       size_t stack_size)
{
#ifdef ENABLE_STACK_WATERMARK
    memset(stack, STACK_WATERMARK_MAGIC, stack_size);
#endif

    // Initialize task struct
    *task = (coop_task_t){
        .stack_bottom = (uintptr_t)stack,
#ifdef ENABLE_STACK_WATERMARK
        .stack_top = (uintptr_t)(stack + stack_size),
#endif
        .sp_current = ((uintptr_t)(stack) + stack_size - sizeof(context_t)) & ~7,  // Align to 8
        .next = NULL,
    };

    // Initialize stack with initial context
    *((context_t*)task->sp_current) = (context_t){
        .xpsr = 1 << 24,               // Thumb state; must be 1
        .pc = (uint32_t)task_wrapper,  // Wrapper fn to call the actual task
        .lr = 0,                       // End of call stack
        .exc_return = EXC_RET_THREAD_PSP,
        .r0 = (uint32_t)task,
        .r1 = (uint32_t)task_fn,
        .r2 = (uint32_t)task_arg,
    };

    // Insert task into list of running tasks
    task_insert(task, &tasks_running);
}

void sched_init(void)
{
    // Set scheduler to lowest priority
    const uint32_t lowest_prio = 255;
    NVIC_SetPriority(PendSV_IRQn, lowest_prio);
}

void sched_yield(void)
{
    // Trigger pendable service; invokes scheduler
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

#ifdef ENABLE_STACK_WATERMARK
size_t get_stack_watermark(coop_task_t* task)
{
    if (task == &main_task) {
        // Watermark not supported for main task
        return 0;
    }
    uint8_t* ptr = (uint8_t*)task->stack_bottom;
    while (*ptr == STACK_WATERMARK_MAGIC && (uintptr_t)ptr < task->stack_top) {
        ptr++;
    }
    return task->stack_top - (uintptr_t)ptr;
}
#endif

__attribute__((naked)) void PendSV_Handler(void)
{
    asm("  tst lr, #0x04              ");  // Check if current task's context lives on MSP or PSP
    asm("  bne store_to_psp           ");
    asm("  push {r4-r11, lr}          ");  // MSP: Store rest of the context; r0-r3/r12-r15 already stored by hw
    asm("  mov r0, sp                 ");  // MSP: Pass stack pointer as argument to context switcher
    asm("  b do_ctx_sw                ");
    asm("store_to_psp:              ");
    asm("  mrs r0, psp                ");  // PSP: Pass stack pointer as argument to context switcher
    asm("  stmdb r0!, {r4-r11, lr}    ");  // PSP: Store rest of the context; r0-r3/r12-r15 already stored by hw
    asm("do_ctx_sw:                 ");
    asm("  ldr r12, =context_switch   ");  // Call context switcher
    asm("  blx r12                    ");  // receives stackpointer of current task; returns stackpointer of next task
    asm("  ldr r1, [r0, #(8*4)]       ");  // Load lr from stored contect
    asm("  tst r1, #0x04              ");  // Check if thread context lives on MSP or PSP
    asm("  bne restore_from_psp       ");
    asm("  mov sp, r0                 ");  // Set new MSP stack pointer
    asm("  pop {r4-r11, lr}           ");  // Restore context of next task
    asm("  bx lr                      ");  // Return from exception
    asm("restore_from_psp:          ");
    asm("  ldmia r0!, {r4-r11, lr}    ");  // Restore context of next task
    asm("  msr psp, r0                ");  // Set new PSP stack pointer
    asm("  bx lr                      ");  // Return from exception
}
