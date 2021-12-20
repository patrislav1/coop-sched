#include "handshake.h"

#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

// platform specific includes
#include "sam4l.h"

// Main task represents the task from which all other tasks are started
static hs_task_t main_task = {0};
static hs_task_t *tasks_running = &main_task, *current_task = &main_task;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wpacked"

typedef struct context {
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;   // Context stored explicitly (by software)
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;  // Context stored implicitly (by hardware)
} __attribute__((packed)) context_t;

#pragma GCC diagnostic pop

// Prototype to make gcc happy (inline asm doesn't find static functions)
uintptr_t hs_context_switch(uintptr_t sp);

// Context switcher; called from PendSV_Handler
uintptr_t hs_context_switch(uintptr_t sp)
{
    current_task->sp_current = sp;
    current_task = current_task->next;
    if (!current_task) {
        // End of list reached; start again at beginning
        current_task = tasks_running;
    }
    return current_task->sp_current;
}

// Add task to list of running tasks
static void hs_task_insert(hs_task_t* t, hs_task_t** list)
{
    // Find last task in list
    hs_task_t* tail;
    for (tail = *list; tail->next; tail = tail->next) {
    };
    tail->next = t;
    // This is now the last element in the list
    t->next = NULL;
}

// Remove task from list of running tasks
static void hs_task_remove(hs_task_t* t, hs_task_t** list)
{
    hs_task_t* prev;
    for (prev = *list; prev->next && prev->next != t; prev = prev->next) {
    };
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
static void noreturn hs_task_wrapper(hs_task_t* task, task_fn_t task_fn, void* task_arg)
{
    task_fn(task_arg);

    hs_task_remove(task, &tasks_running);
    hs_yield();

    while (1) {
    };
}

// Create task and add it to the list of running tasks
void hs_task_create(hs_task_t* task,
                    task_fn_t task_fn,
                    void* task_arg,
                    uint8_t* stack,
                    size_t stack_size)
{
    // Initialize task struct
    *task = (hs_task_t){
        .stack_bottom = (uintptr_t)stack,
        .sp_current = ((uintptr_t)(stack) + stack_size - sizeof(context_t)) & ~7,
        .next = NULL,
    };

    // Initialize stack with initial context
    *((context_t*)task->sp_current) = (context_t){
        .xpsr = 1 << 24,  // set T-bit
        .pc = (uint32_t)hs_task_wrapper,
        .lr = 0,  // end of call stack
        .r0 = (uint32_t)task,
        .r1 = (uint32_t)task_fn,
        .r2 = (uint32_t)task_arg,
    };

    // Insert task into list of running tasks
    hs_task_insert(task, &tasks_running);
}

void hs_init(void)
{
    const uint32_t handler_prio = 255;  // lowest priority
    NVIC_SetPriority(PendSV_IRQn, handler_prio);
}

void hs_yield(void)
{
    // Invoke scheduler
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm(
        ".syntax unified            \n"
        "mrs r0, msp                \n"  // Save stack pointer
        "stmdb r0!, {r4-r11}        \n"  // Store rest of the context
        "ldr r12, =hs_context_switch\n"
        "blx r12                    \n"
        "ldmia r0!, {r4-r11}        \n"
        "mvn lr, #~0xfffffff9       \n"
        "msr msp, r0                \n"
        "bx lr                      \n"
        ".syntax divided            \n");
}
