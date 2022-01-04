#pragma once

// platform-specific includes

/* Atmel SAM4L */
#if defined(__ATSAM4LS8C__) || defined(__SAM4LS8C__)
#include "sam4l.h"
#elif defined(STM32L433xx)
#include "stm32l433xx.h"
#else
#error "no platform defined"
#endif
