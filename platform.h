#pragma once

// platform-specific includes

#if defined(__ATSAM4LS8C__) || defined(__SAM4LS8C__)
/* Atmel SAM4L */
#include "sam4l.h"
#elif defined(STM32L433xx)
/* STM32L433 */
#include "stm32l433xx.h"
#else
#error "no platform defined"
#endif
