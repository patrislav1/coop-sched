#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct hs_task {
    uintptr_t sp_current;
    uintptr_t stack_bottom;
    struct hs_task* next;
} hs_task_t;

typedef void (*task_fn_t)(void* arg);

void hs_init(void);

void hs_task_create(hs_task_t* task,
                    task_fn_t task_fn,
                    void* task_arg,
                    uint8_t* stack,
                    size_t stack_size);

void hs_yield(void);
