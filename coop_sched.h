#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct coop_task {
    uintptr_t sp_current;
    uintptr_t stack_bottom;
    struct coop_task* next;
} coop_task_t;

typedef void (*coop_task_fn_t)(void* arg);

// Initialize scheduler
void sched_init(void);

// Create task and add it to the list of running tasks
void sched_create_task(coop_task_t* task,
                       coop_task_fn_t task_fn,
                       void* task_arg,
                       uint8_t* stack,
                       size_t stack_size);

// Trigger scheduler
void sched_yield(void);

// Optional function to be defined by the user
void emergency_print(const char* str);
