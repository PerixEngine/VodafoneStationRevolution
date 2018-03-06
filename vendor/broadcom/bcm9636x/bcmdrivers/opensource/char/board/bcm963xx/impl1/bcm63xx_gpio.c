/*
<:copyright-BRCM:2002:GPL/GPL:standard
 Copyright 2010 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/
/***************************************************************************
* File Name  : bcm63xx_gpio.c
*
* Description: This file contains functions related to GPIO access.
*     See GPIO register defs in shared/broadcom/include/bcm963xx/xxx_common.h
*
*
***************************************************************************/

/*
 * Access to _SHARED_ GPIO registers should go through common functions
 * defined in board.h.  These common functions will use a spinlock with
 * irq's disabled to prevent concurrent access.
 * Functions which don't want to call the common gpio access functions must
 * acquire the bcm_gpio_spinlock with irq's disabled before accessing the
 * shared GPIO registers.
 * The GPIO registers that must be protected are:
 * GPIO->GPIODir
 * GPIO->GPIOio
 * GPIO->GPIOMode
 *
 * Note that many GPIO registers are dedicated to some driver or sub-system.
 * In those cases, the driver/sub-system can use its own locking scheme to
 * ensure serial access to its GPIO registers.
 *
 * DEFINE_SPINLOCK is the new, recommended way to declaring a spinlock.
 * See spinlock_types.h
 *
 * I am using a spin_lock_irqsave/spin_lock_irqrestore to lock and unlock
 * so that GPIO's can be accessed in interrupt context.
 */
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/bcm_assert_locks.h>
#include <bcm_map_part.h>
#include "board.h"
#include <boardparms.h>

#include "bcm_OS_Deps.h"


DEFINE_SPINLOCK(bcm_gpio_spinlock);
EXPORT_SYMBOL(bcm_gpio_spinlock);


void kerSysSetGpioStateLocked(unsigned short bpGpio, GPIO_STATE_t state)
{
    BCM_ASSERT_V(bpGpio != BP_NOT_DEFINED);
    BCM_ASSERT_V((bpGpio & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX);
    BCM_ASSERT_HAS_SPINLOCK_V(&bcm_gpio_spinlock);

    kerSysSetGpioDirLocked(bpGpio);

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
    /* Take over high GPIOs from WLAN block */
    if ((bpGpio & BP_GPIO_NUM_MASK) > 31)
        GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(bpGpio - 32);
#endif

    if((state == kGpioActive && !(bpGpio & BP_ACTIVE_LOW)) ||
        (state == kGpioInactive && (bpGpio & BP_ACTIVE_LOW)))
        GPIO->GPIOio |= GPIO_NUM_TO_MASK(bpGpio);
    else
        GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(bpGpio);

}

void kerSysSetGpioState(unsigned short bpGpio, GPIO_STATE_t state)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    kerSysSetGpioStateLocked(bpGpio, state);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}



void kerSysSetGpioDirLocked(unsigned short bpGpio)
{
    BCM_ASSERT_V(bpGpio != BP_NOT_DEFINED);
    BCM_ASSERT_V((bpGpio & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX);
    BCM_ASSERT_HAS_SPINLOCK_V(&bcm_gpio_spinlock);

    GPIO->GPIODir |= GPIO_NUM_TO_MASK(bpGpio);
}

void kerSysSetGpioDir(unsigned short bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    kerSysSetGpioDirLocked(bpGpio);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}


/* Set gpio direction to input. Parameter gpio is in boardparms.h format. */
int kerSysSetGpioDirInput(unsigned bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
  //  GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(bpGpio);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    return(0);
}

/* Return a gpio's value, 0 or 1. Parameter gpio is in boardparms.h format. */
int kerSysGetGpioValue(unsigned bpGpio)
{
    return((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(bpGpio)) >>
        (bpGpio & BP_GPIO_NUM_MASK)));
}


#if defined(CONFIG_I2C_GPIO) && !defined(CONFIG_GPIOLIB)
/* GPIO bit functions to support drivers/i2c/busses/i2c-gpio.c */

#if defined(CONFIG_BCM96828)
/* Assign GPIO to BCM6828 PERIPH block from EPON 8051. */
#define EPON_GPIO_START             41
#define GPIO_TAKE_CONTROL(G) \
    do \
    { \
    if( (G) >= EPON_GPIO_START ) \
    { \
        GPIO->GPIOCtrl |= (GPIO_NUM_TO_MASK((G) - EPON_GPIO_START) << \
            GPIO_EPON_PERIPH_CTRL_S); \
    } \
    } while(0)
#else
#define GPIO_TAKE_CONTROL(G) 
#endif

/* noop */
int gpio_request(unsigned bpGpio, const char *label)
{
    return(0); /* success */
}

/* noop */
void gpio_free(unsigned bpGpio)
{
}

/* Assign a gpio's value. Parameter gpio is in boardparms.h format. */
void gpio_set_value(unsigned bpGpio, int value)
{
    unsigned long flags;

    /* value should be either 0 or 1 */
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    if( value == 0 )
        GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(bpGpio);
    else
        if( value == 1 )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(bpGpio);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

/* Set gpio direction to input. Parameter gpio is in boardparms.h format. */
int gpio_direction_input(unsigned bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(bpGpio);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    return(0);
}

/* Set gpio direction to output. Parameter gpio is in boardparms.h format. */
int gpio_direction_output(unsigned bpGpio, int value)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    GPIO->GPIODir |= GPIO_NUM_TO_MASK(bpGpio);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    gpio_set_value(bpGpio, value);
    return(0);
}

/* Return a gpio's value, 0 or 1. Parameter gpio is in boardparms.h format. */
int gpio_get_value(unsigned bpGpio)
{
    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);
    GPIO_TAKE_CONTROL(bpGpio & BP_GPIO_NUM_MASK);
    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    return((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(bpGpio)) >>
        (bpGpio & BP_GPIO_NUM_MASK)));
}
#endif


//PegatronCVP, Frederick, 20140106 add PS I2C GPIO bitbanging mechanism

#define UNIHAN_I2C

#ifdef UNIHAN_I2C



static unsigned short prox_gpio_sda = BP_NOT_DEFINED;
static unsigned short prox_gpio_scl = BP_NOT_DEFINED;
static int prox_gpio_initialized = 0;

#define MCU_GPIO_SDA	BP_GPIO_1_AH
#define MCU_GPIO_SCL	BP_GPIO_0_AH
#define MCU_GPIO_ISP    BP_GPIO_21_AH


#define I2C_ACK   1
#define I2C_NO_ACK   0
#define GPIO_HIGH	1
#define GPIO_LOW	0
#define I2C_DELAY_TIME_INTRV	10
#define I2C_DELAY_TIME	 15

void I2C_start(void) 
{

 /* I2C start sequence is defined as 

  * a High to Low Transition on the data

  * line as the CLK pin is high */

	
 	unsigned long flags;
 
 	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_sda); 
	GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_scl); 


	GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_sda); /* SDA: High */
	GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */

	udelay(I2C_DELAY_TIME_INTRV); 

	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_sda);
	udelay(I2C_DELAY_TIME_INTRV);

	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl);

	udelay(I2C_DELAY_TIME);

	spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);	

}

void I2C_start_repeat(void) 
{

    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_scl);

	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl);

	udelay(I2C_DELAY_TIME);

	spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);	
}

void I2C_init(void) 
{
 	unsigned long flags;

	if (!prox_gpio_initialized)
	{
	    if (BpGetProximitySensorSDAGpio(&prox_gpio_sda) != BP_SUCCESS)
		return;

	    if (BpGetProximitySensorSCLGpio(&prox_gpio_scl) != BP_SUCCESS)
		return;

	    prox_gpio_initialized = 1;
	}

	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

//for proximity sensor
	 GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_sda); 
	 GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_scl);

	 /* Set SDA & SCL to High */
	 GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_sda); /* SDA: High */
	 GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */	 

	 spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
	 
}


void I2C_stop(void) 
{  

 /* I2C stop sequence is defined as 

  * data pin is low, then CLK pin is high,

  * finally data pin is high. */
//	unsigned long flags=0;
 unsigned long flags;
 
	 spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if 1
	 	 
	 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_sda); /*SDA LOW*/
	
	 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl); /*SCL LOW*/

	udelay(I2C_DELAY_TIME_INTRV);  
#endif	

	GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */	

	udelay(I2C_DELAY_TIME_INTRV);

	GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_sda); /* SDA: High */

	udelay(I2C_DELAY_TIME); 

	spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

}


void I2C_write(unsigned char data) 
{

 unsigned long flags;
 unsigned int i;
 unsigned char tempVal;
 spin_lock_irqsave(&bcm_gpio_spinlock, flags);


 /* An I2C output byte is bits 7-0

   * (MSB to LSB).  Shift one bit at a time

  * to the MDO output, and then clock the

  * data to the I2C Slave */

 

 /* Write to slave */
 GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_sda); 
 GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_scl);
 

 for(i = 0; i < 8; i++) {

	if (data&0x80){
			GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_sda); /* SDA: High */

		}
	else
		{
			GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_sda); /*SDA LOW*/

		}
	
// 	gpio_set_value(prox_gpio_sda,(data&0x80)?1:0); /* Send data bit */
	
	data <<= 1;   /* Shift one bit */
	
	udelay(I2C_DELAY_TIME_INTRV);

	GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */

	udelay(I2C_DELAY_TIME_INTRV);
  	
	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl); /*SCL LOW*/
  	
	udelay(I2C_DELAY_TIME_INTRV/2);

 }

 /* Read ACK bit from slave */
 
 GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(prox_gpio_sda);
 tempVal = ((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(prox_gpio_sda)) >>
        (prox_gpio_sda & BP_GPIO_NUM_MASK)));

 udelay(I2C_DELAY_TIME_INTRV);

 
 GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */
 udelay(I2C_DELAY_TIME_INTRV);
 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl); /*SCL LOW*/
 udelay(I2C_DELAY_TIME_INTRV);

 
 spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

}


unsigned char I2C_read(unsigned char send_ack) 
{  
 unsigned char data;
 unsigned int i; 
 unsigned long flags;

  data = 0x00;
 
 spin_lock_irqsave(&bcm_gpio_spinlock, flags);

  GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(prox_gpio_sda);

 GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_scl);

 /* Read from slave */

 for(i = 0; i < 8; i++) {

  data <<= 1;   /* Shift one bit */
  data |= ((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(prox_gpio_sda)) >>
        (prox_gpio_sda & BP_GPIO_NUM_MASK))); /* Read data bit */

  GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */
  udelay(I2C_DELAY_TIME_INTRV);

  GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl); /*SCL LOW*/
  udelay(I2C_DELAY_TIME_INTRV);

 }

 GPIO->GPIODir |= GPIO_NUM_TO_MASK(prox_gpio_sda); 

 /* Send ACK bit to slave */
 if(send_ack == I2C_ACK)
	 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_sda); /*SDA LOW*/
 else	 	
 	GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_sda); /* SDA: High */ 	
 

 GPIO->GPIOio |= GPIO_NUM_TO_MASK(prox_gpio_scl); /* SCL: High */
 udelay(I2C_DELAY_TIME_INTRV);
 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(prox_gpio_scl); /*SCL LOW*/
 udelay(I2C_DELAY_TIME_INTRV);

 spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);


 return data;

}


void I2C_WriteByte (unsigned char slv_addr, unsigned char reg_addr, unsigned char reg_val)

{

  if (!prox_gpio_initialized)
      return;

  slv_addr <<= 1;	/* Shift one bit for 7bit slave address */

  I2C_start();

  I2C_write(slv_addr);  

  I2C_write(reg_addr);

  I2C_write(reg_val);

  I2C_stop();
  
}


unsigned char I2C_ReadByte (unsigned char slv_addr, unsigned char reg_addr)

{

unsigned char tempVal=0;

if (!prox_gpio_initialized)
    return 0;

slv_addr <<= 1;   /* Shift one bit for 7bit slave address*/

I2C_start();
I2C_write(slv_addr);  
I2C_write(reg_addr);  

I2C_start();

I2C_write(slv_addr + 1);  //set to read mode

tempVal = I2C_read(I2C_NO_ACK);

I2C_stop();

return tempVal;


}




//PegatronCVP, Prada, 20140113 add MCU I2C GPIO bitbanging mechanism

void I2C_MCU_start(void) 
{

 /* I2C start sequence is defined as 

  * a High to Low Transition on the data

  * line as the CLK pin is high */

	
 	unsigned long flags;
 
 	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); 
	GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); 


	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /* SDA: High */
	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */

	udelay(I2C_DELAY_TIME_INTRV); 

	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA);
	udelay(I2C_DELAY_TIME_INTRV); //need to delay more so MCU will receive it
	udelay(I2C_DELAY_TIME_INTRV);

	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL);

	udelay(I2C_DELAY_TIME);

	spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);	

}

void I2C_MCU_start_repeat(void) 
{

    unsigned long flags;

    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL);

	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL);

	udelay(I2C_DELAY_TIME);

	spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);	
}

void I2C_MCU_init(void) 
{
 	unsigned long flags;
	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); 
	 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL);

	 /* Set SDA & SCL to High */
	 GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /* SDA: High */
	 GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */	 

	 spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
	 
}


void I2C_MCU_stop(void) 
{  

 /* I2C stop sequence is defined as 

  * data pin is low, then CLK pin is high,

  * finally data pin is high. */
//	unsigned long flags=0;
 unsigned long flags;
 
	 spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if 1
	 	 
	 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /*SDA LOW*/
	
	 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /*SCL LOW*/

	udelay(I2C_DELAY_TIME_INTRV);  
#endif	

	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */	

	udelay(I2C_DELAY_TIME_INTRV);

 	udelay(I2C_DELAY_TIME_INTRV);  //need to delay more so MCU will receive it

	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /* SDA: High */

	udelay(I2C_DELAY_TIME); 

	spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

}


void I2C_MCU_write(unsigned char data) 
{

 unsigned long flags;
 unsigned int i;
 unsigned char tempVal;
 spin_lock_irqsave(&bcm_gpio_spinlock, flags);


 /* An I2C output byte is bits 7-0

   * (MSB to LSB).  Shift one bit at a time

  * to the MDO output, and then clock the

  * data to the I2C Slave */

 

 /* Write to slave */
 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); 
 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL);
 

 for(i = 0; i < 8; i++) {

	if (data&0x80){
			GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /* SDA: High */

		}
	else
		{
			GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /*SDA LOW*/

		}
	
// 	gpio_set_value(MCU_GPIO_SDA,(data&0x80)?1:0); /* Send data bit */
	
	data <<= 1;   /* Shift one bit */
	
	udelay(I2C_DELAY_TIME_INTRV);

	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */

	udelay(I2C_DELAY_TIME_INTRV);
  	
	GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /*SCL LOW*/
  	
	udelay(I2C_DELAY_TIME_INTRV/2);

 }

 /* Read ACK bit from slave */
 
 GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA);
 tempVal = ((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(MCU_GPIO_SDA)) >>
        (MCU_GPIO_SDA & BP_GPIO_NUM_MASK)));

//virkin, we should set SDA to low
 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); 
 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /*SDA LOW*/

 udelay(I2C_DELAY_TIME_INTRV);
 
 GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */
 udelay(I2C_DELAY_TIME_INTRV);
 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /*SCL LOW*/
 udelay(I2C_DELAY_TIME_INTRV);

  udelay(100);



 
 spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

}


unsigned char I2C_MCU_read(unsigned char send_ack) 
{  
 unsigned char data;
 unsigned int i; 
 unsigned long flags;

  data = 0x00;
 
 spin_lock_irqsave(&bcm_gpio_spinlock, flags);

  GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA);

 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL);

 /* Read from slave */

 for(i = 0; i < 8; i++) {

  data <<= 1;   /* Shift one bit */
  data |= ((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(MCU_GPIO_SDA)) >>
        (MCU_GPIO_SDA & BP_GPIO_NUM_MASK))); /* Read data bit */

  GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */
  udelay(I2C_DELAY_TIME_INTRV);

  GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /*SCL LOW*/
  udelay(I2C_DELAY_TIME_INTRV);

 }

 GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); 

 /* Send ACK bit to slave */
 if(send_ack == I2C_ACK)
	 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /*SDA LOW*/
 else	 	
 	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SDA); /* SDA: High */ 	
 
 udelay(I2C_DELAY_TIME_INTRV);  //need to delay more so MCU will receive it

 GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /* SCL: High */
 udelay(I2C_DELAY_TIME_INTRV);
 GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_SCL); /*SCL LOW*/
 udelay(I2C_DELAY_TIME_INTRV);

 udelay(100);  //need to delay more so MCU will receive it

 spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);


 return data;

}

void I2C_MCU_WriteByte (unsigned char slv_addr, unsigned char reg_prop, unsigned char reg_led, unsigned char reg_data)
{

    slv_addr <<= 1;	/* Shift one bit for 7bit slave address */

    I2C_MCU_start();

    I2C_MCU_write(slv_addr);  

    I2C_MCU_write(reg_prop);

    I2C_MCU_write(reg_led);
  
    I2C_MCU_write(reg_data);

    I2C_MCU_stop();

}

void I2C_MCU_ReadByte (unsigned char slv_addr, unsigned char reg_addr, BOARD_I2C_MCU_PARMS *i2cMcuParms)
{

    slv_addr <<= 1;   /* Shift one bit for 7bit slave address*/

    I2C_MCU_start();
    I2C_MCU_write(slv_addr);  
    I2C_MCU_write(reg_addr);  

    I2C_MCU_start();

    I2C_MCU_write(slv_addr + 1);  //set to read mode

    i2cMcuParms->result[0] = I2C_MCU_read(I2C_ACK);
    i2cMcuParms->result[1] = I2C_MCU_read(I2C_ACK);
    i2cMcuParms->result[2] = I2C_MCU_read(I2C_ACK);
    i2cMcuParms->result[3] = I2C_MCU_read(I2C_ACK);
    i2cMcuParms->result[4] = I2C_MCU_read(I2C_ACK);
    i2cMcuParms->result[5] = I2C_MCU_read(I2C_ACK);

    I2C_MCU_stop();

}

void P21_init(void) 
{
 	unsigned long flags;
	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_ISP); 
    
	GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_ISP); /* High */

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
	 
}

void P21_Pull_Low(void)
{
 	unsigned long flags;
 
 	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

 	GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_ISP); 

    GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(MCU_GPIO_ISP); /* LOW*/

    udelay(I2C_DELAY_TIME);

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

void P21_Pull_High(void)
{
 	unsigned long flags;
 
 	spin_lock_irqsave(&bcm_gpio_spinlock, flags);

 	GPIO->GPIODir |= GPIO_NUM_TO_MASK(MCU_GPIO_ISP); 

    GPIO->GPIOio |= GPIO_NUM_TO_MASK(MCU_GPIO_ISP); /*  High */

    udelay(I2C_DELAY_TIME);

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

void P21_Pull(unsigned char cmd)
{
    P21_init();

    if (cmd == TRUE)
    {
        P21_Pull_High();
    }
    else if (cmd == FALSE)
    {
        P21_Pull_Low();
    }
}

void I2C_ISP_Write_start(unsigned char slv_addr) 
{
    slv_addr <<= 1;	/* Shift one bit for 7bit slave address */

    I2C_MCU_start();

    I2C_MCU_write(slv_addr); 

}

void I2C_ISP_Read_start(unsigned char slv_addr) 
{
    slv_addr <<= 1; /* Shift one bit for 7bit slave address */

    I2C_MCU_start();

    I2C_MCU_write(slv_addr + 1);  //set to read mode

}

void I2C_ISP_stop() 
{

    I2C_MCU_stop();

}

void I2C_ISP_WriteByte (unsigned char cmd)
{

    I2C_MCU_write(cmd);
    
}

unsigned char I2C_ISP_ReadByte ()

{

    unsigned char tempVal=0;

    tempVal = I2C_MCU_read(I2C_ACK);

    return tempVal;

}


#endif



EXPORT_SYMBOL(kerSysSetGpioState);
EXPORT_SYMBOL(kerSysSetGpioStateLocked);
EXPORT_SYMBOL(kerSysSetGpioDir);
EXPORT_SYMBOL(kerSysSetGpioDirLocked);
EXPORT_SYMBOL(kerSysSetGpioDirInput);
EXPORT_SYMBOL(kerSysGetGpioValue);

//PegatronCVP, Frederick, 20140106 add PS I2C GPIO bitbanging mechanism
EXPORT_SYMBOL(I2C_WriteByte);
EXPORT_SYMBOL(I2C_ReadByte);
EXPORT_SYMBOL(I2C_init);
//PegatronCVP, Prada, 20140113 add MCU I2C GPIO bitbanging mechanism
EXPORT_SYMBOL(I2C_MCU_WriteByte);
EXPORT_SYMBOL(I2C_MCU_ReadByte);
EXPORT_SYMBOL(I2C_MCU_init);
//PegatronCVP, Prada, 20140205 add functions for ISP MCU upgrade
EXPORT_SYMBOL(P21_Pull);
EXPORT_SYMBOL(I2C_ISP_WriteByte);
EXPORT_SYMBOL(I2C_ISP_ReadByte);
EXPORT_SYMBOL(I2C_ISP_Write_start);
EXPORT_SYMBOL(I2C_ISP_Read_start);
EXPORT_SYMBOL(I2C_ISP_stop);
/*BEGIN: added by Huawei*/

#define TCA6507_LED_ADDR        0x45
#define TCL2672_SENSOR_ADDR     0x39

#define GPIO_DELAY      10
#define GPIO_DIR_OUTPUT 1
#define GPIO_DIR_INPUT  0
#define GPIO_DATA_HIGH  1
#define GPIO_DATA_LOW   0

static UINT16 g_sdaGpio = 0;
static UINT16 g_sclGpio = 0;

static spinlock_t hw_gpioI2cLock;

static void i2c_gpio_set_val(unsigned int gpio, int state)
{
    unsigned long flags;
    
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	if (state)
        GPIO->GPIOio |= GPIO_NUM_TO_MASK(gpio);
	else
        GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(gpio);

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

    return;
}

/* Toggle SCL by changing the direction of the pin. */
static void i2c_gpio_set_dir(unsigned int gpio, int state)
{
    unsigned long flags;
    
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

	if (GPIO_DIR_OUTPUT == state)
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(gpio);
	else
        GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(gpio);

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);

    return;
}

static void gpio_i2c_start(void)  
{       
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);    
    udelay(GPIO_DELAY);  
    i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);  
    
    udelay(GPIO_DELAY);  
    i2c_gpio_set_val(g_sdaGpio, GPIO_DATA_LOW);  
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_OUTPUT);  
    udelay(GPIO_DELAY);      
            
    i2c_gpio_set_val(g_sclGpio, GPIO_DATA_LOW);    
    udelay(GPIO_DELAY);      
    
    return;
}  

static void gpio_i2c_stop(void)  
{   
    i2c_gpio_set_val(g_sdaGpio, GPIO_DATA_LOW);
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_OUTPUT);    
    udelay(GPIO_DELAY);  

    i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);
    
    udelay(GPIO_DELAY); 
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);    
    udelay(GPIO_DELAY);  

    return;
}  

static void gpio_i2c_send_ack(unsigned char ack)  
{  
    // 0 ACK   1 NACK
    if (ack)  
    {
        i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);
        udelay(GPIO_DELAY);  
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH); 
        
        udelay(GPIO_DELAY);  
        
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_LOW);  
        udelay(GPIO_DELAY);    
    }
    else   
    {
        i2c_gpio_set_val(g_sdaGpio, GPIO_DATA_LOW); 
        i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_OUTPUT);    
        
        udelay(GPIO_DELAY);  
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);  
        udelay(GPIO_DELAY);  
        
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_LOW);  
        udelay(GPIO_DELAY);           
    }

    return;
}  

static int gpio_get_value(unsigned short bpGpio)
{
    return((int) ((GPIO->GPIOio & GPIO_NUM_TO_MASK(bpGpio)) >>
        (bpGpio & BP_GPIO_NUM_MASK)));
}

static unsigned char gpio_i2c_receive_ack(void)  
{  
    unsigned char rc = 0;  
    
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);    
    i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);  
    udelay(GPIO_DELAY);    

    if (gpio_get_value(g_sdaGpio)) 
    {  
      printk("ack fail\n");
      rc = 1;  
    } 
    
    i2c_gpio_set_val(g_sclGpio, GPIO_DATA_LOW);  
    udelay(GPIO_DELAY);    
    
    return rc;  
}  

static unsigned char gpio_i2c_send_byte(unsigned char send_byte)  
{  
    unsigned char rc = 0;  
    unsigned char out_mask = 0x80;  
    unsigned char value;  
    unsigned char count = 8;  
    
    while (count > 0) 
    {                  
        value = ((send_byte & out_mask) ? 1 : 0);     
        if (value == 1) 
        {             
            i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);    
        }      
        else 
        {                                              
            i2c_gpio_set_val(g_sdaGpio, GPIO_DATA_LOW);  
            i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_OUTPUT);    
        }
        udelay(GPIO_DELAY);    
        
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);  
        udelay(GPIO_DELAY);    

        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_LOW);  
                    
        out_mask >>= 1;        
        count--;         
    }  
    
    udelay(GPIO_DELAY);    
    rc = gpio_i2c_receive_ack();
    
    return rc;
    
}

static void gpio_i2c_read_byte(unsigned char *buffer, unsigned char ack)  
{  
    unsigned char count = 0x08;  
    unsigned char data = 0x00;  
    unsigned char temp = 0;  
      
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);    
    
    while (count > 0) 
    {  
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);  
        udelay(GPIO_DELAY);    
        temp = gpio_get_value(g_sdaGpio);  
        data <<= 1;  
        if (temp)
        {
            data |= 0x01;
        }
        
        i2c_gpio_set_val(g_sclGpio, GPIO_DATA_LOW);  
        udelay(GPIO_DELAY);    
        count--;  
    }
    
    gpio_i2c_send_ack(ack);//0 = ACK    1 = NACK   
    *buffer = data;           
    
    return;
}

unsigned int gpio_i2c_set_reg(unsigned char sensor_addr, unsigned char reg_cmd, unsigned char *data, unsigned int len)  
{  
    unsigned char rc = 0;  
    unsigned char i;
    unsigned long flags = 0;

    if ((NULL == data) || (0 == len))
    {
        return 1;
    }
    
    spin_lock_irqsave(&hw_gpioI2cLock, flags);
    gpio_i2c_start();
       
    rc = gpio_i2c_send_byte((sensor_addr << 1) | 0x00);  
    if (rc)
    {   
		gpio_i2c_stop();  
        spin_unlock_irqrestore(&hw_gpioI2cLock, flags);
        return 1;  
    }
  
    rc = gpio_i2c_send_byte(reg_cmd);     // send command
    if (rc)
    {      
		gpio_i2c_stop();  
        spin_unlock_irqrestore(&hw_gpioI2cLock, flags);    
        return 1;  
    }

    for (i = 0; i < len; i++) 
    {          
        rc = gpio_i2c_send_byte(*data);  
        if (rc)
        {    
			gpio_i2c_stop();  
        	spin_unlock_irqrestore(&hw_gpioI2cLock, flags);          
            return 1;  
        }

        data++;  
    }  
      
    gpio_i2c_stop();
    spin_unlock_irqrestore(&hw_gpioI2cLock, flags);    
				
    if (rc) 
    {              
        return 1;
    }      
    return 0;    
}

unsigned int gpio_i2c_get_reg(unsigned char sensor_addr, unsigned char reg_cmd, unsigned char *data, unsigned int len)  
{  
    unsigned char rc = 0;  
	unsigned long flags = 0;
    unsigned char i;

    if ((NULL == data) || (0 == len))
    {
        return 1;
    }
    
	spin_lock_irqsave(&hw_gpioI2cLock, flags);	
    gpio_i2c_start();

    rc = gpio_i2c_send_byte((sensor_addr << 1) | 0x00);  
    if (rc)
    {   
    	gpio_i2c_stop();
		spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 
        return 1;  
    }

    rc = gpio_i2c_send_byte(reg_cmd);     // send command
    if (rc)
    {          
    	gpio_i2c_stop();
		spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 
        return 1;  
    }

    gpio_i2c_start();

    rc = gpio_i2c_send_byte((sensor_addr << 1) | 0x01);  
    if (rc)
    {   
    	gpio_i2c_stop();
		spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 
        return 1;  
    }

    for (i = 0; i < len; i++) 
    {  
        // 当是最后一个字节后发送NACK信号.
        gpio_i2c_read_byte(data++, !(len - i - 1));
    } 

    gpio_i2c_stop();
	spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 

    if (rc) 
    {              
        return 1;
    }     

    return 0;    

}

unsigned int TCA6507_set_reg(unsigned char start_addr, unsigned char *data, unsigned int len)  
{  
    unsigned char reg_cmd;  

    if ((NULL == data) || (0 == len) || start_addr > 0x15)
    {
        return 1;
    }

    reg_cmd = start_addr;

    if (len > 1)
        reg_cmd |= 0x10;
    
    return gpio_i2c_set_reg(TCA6507_LED_ADDR, reg_cmd, data, len);   
}

unsigned int TCA6507_get_reg(unsigned char start_addr, unsigned char *data, unsigned int len)  
{  
    unsigned char reg_cmd;  

    if ((NULL == data) || (0 == len) || start_addr > 0x15)
    {
        return 1;
    }

    reg_cmd = start_addr;

    if (len > 1)
        reg_cmd |= 0x10;
    
    return gpio_i2c_get_reg(TCA6507_LED_ADDR, reg_cmd, data, len);   
}

unsigned int TCA2672_set_reg(unsigned char start_addr, unsigned char *data, unsigned int len)  
{  
    unsigned char reg_cmd;  

    if ((NULL == data) || (0 == len) || start_addr > 0x1E)
    {
        return 1;
    }

    reg_cmd = start_addr | 0x80;

    if (len > 1)
        reg_cmd |= 0x20;
    
    return gpio_i2c_set_reg(TCL2672_SENSOR_ADDR, reg_cmd, data, len);   
}

unsigned int TCA2672_get_reg(unsigned char start_addr, unsigned char *data, unsigned int len)  
{  
    unsigned char reg_cmd;  

    if ((NULL == data) || (0 == len) || start_addr > 0x1E)
    {
        return 1;
    }

    reg_cmd = start_addr | 0x80;

    if (len > 1)
        reg_cmd |= 0x20;
    
    return gpio_i2c_get_reg(TCL2672_SENSOR_ADDR, reg_cmd, data, len);   
}

unsigned int kerSysGpioI2CInit(VOID)
{
	spin_lock_init(&hw_gpioI2cLock);
    
    g_sdaGpio = BP_GPIO_9_AH;
    g_sclGpio = BP_GPIO_8_AH;

    printk("sdaGpio: [%d] sclGpio: [%d] \r\n", g_sdaGpio, g_sclGpio);

    i2c_gpio_set_val(g_sdaGpio, GPIO_DATA_HIGH);  
    i2c_gpio_set_dir(g_sdaGpio, GPIO_DIR_INPUT);    
    i2c_gpio_set_val(g_sclGpio, GPIO_DATA_HIGH);  
    i2c_gpio_set_dir(g_sclGpio, GPIO_DIR_OUTPUT);  

    return 0;
}

unsigned int kerSysGpioI2CGetReg(E_I2C_DEV dev, unsigned char start_addr, unsigned char *data, unsigned int len)
{
    if (E_LED_I2C_DEV == dev)
    {
        return TCA6507_get_reg(start_addr, data, len);
    }

    if (E_SENSOR_I2C_DEV == dev)
    {
        return TCA2672_get_reg(start_addr, data, len);
    }

    return 1;
}


unsigned int kerSysGpioI2CSetReg(E_I2C_DEV dev, unsigned char start_addr, unsigned char *data, unsigned int len)
{
    if (E_LED_I2C_DEV == dev)
    {       
        return TCA6507_set_reg(start_addr, data, len);
    }

    if (E_SENSOR_I2C_DEV == dev)
    {       
        return TCA2672_set_reg(start_addr, data, len);
    }

    return 1;
}

unsigned int kerSysSensorClearInterrupt(void)
{
    unsigned char rc = 0;  
    unsigned char i2c_cmd = (0x1<<7) | (0x3<<5) | 0x5;
	unsigned long flags = 0;

	spin_lock_irqsave(&hw_gpioI2cLock, flags);	
    gpio_i2c_start();
       
    rc = gpio_i2c_send_byte((TCL2672_SENSOR_ADDR << 1) | 0x00);  
    if (rc)
    {   
    	gpio_i2c_stop();
		spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 
        return 1;  
    }
  
    rc = gpio_i2c_send_byte(i2c_cmd);     // send command
    if (rc)
    {          
    	gpio_i2c_stop();
		spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 
        return 1;  
    }

    gpio_i2c_stop();
	spin_unlock_irqrestore(&hw_gpioI2cLock, flags); 
    
    if (rc) 
    {              
        return 1;
    }   
	   
    return 0;  
}

EXPORT_SYMBOL(kerSysGpioI2CInit);
EXPORT_SYMBOL(kerSysSensorClearInterrupt);
EXPORT_SYMBOL(kerSysGpioI2CSetReg);
EXPORT_SYMBOL(kerSysGpioI2CGetReg);
/*END: added by Huawei*/
