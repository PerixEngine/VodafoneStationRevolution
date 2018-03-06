/****************************************************************************
 * Copyright  (C) 2000 - 2008 Jungo Ltd. All Rights Reserved.
 * 
 *  rg/vendor/broadcom/bcm9636x/linux-2.6/include/linux/gpio.h * 
 * 
 * This file is Jungo's confidential and proprietary property. 
 * This file may not be copied, 
 * distributed or otherwise used in any way without 
 * the express prior approval of Jungo Ltd. 
 * For information contact info@jungo.com
 * 
 * 
 */

#ifndef _BCM9636X_GPIO_H_
#define _BCM9636X_GPIO_H_

#include <vendor/broadcom/bcm9636x/board.h>

static inline void gpio_set_value(unsigned gpio, int value)
{
    kerSysSetGpio(gpio, value? kGpioActive : kGpioInactive);
}

static inline int gpio_get_value(unsigned gpio)
{
    return (kerSysGetGpio(gpio) == kGpioActive);
}

static inline int gpio_direction_input(unsigned gpio)
{
	return 0;
}

static inline int gpio_direction_output(unsigned gpio, int value)
{
	return 0;
}

static inline void gpio_free(unsigned gpio)
{
}

static inline int gpio_request(unsigned gpio, const char *label)
{
	return 0;
}

#endif

