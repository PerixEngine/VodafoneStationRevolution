/*
<:copyright-BRCM:2002:GPL/GPL:standard
 Copyright 2010 Pegatron Corp. All Rights Reserved.

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
* File Name  : kmcu_api.c
*
* Description: This file contains functions for controlling nuvo pwm mcu.
***************************************************************************/
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <board.h>

#include "bcm_OS_Deps.h"
#include "ledapi.h"

#define COUNT_TOP_LEDS LED_BOTTOM

typedef struct led_cache_t
{
    int dirty;
    int color;
    int state;
} led_cache_t;

/*Prototypes, Externs*/
extern void I2C_MCU_WriteByte (unsigned char slv_addr, unsigned char reg_prop, unsigned char reg_led, unsigned char reg_data);

/*Globals*/
static int visibility_is_on;
static led_cache_t led_cache[COUNT_TOP_LEDS]; /* Bottom led is not affected by visibility and should not be cached */

unsigned char kLedIntensityUCharMapping(unsigned int Led_Level)
{
	unsigned char cLed_Intensity = UCHAR_0x0;
    
    switch(Led_Level)
    {
        case LED_LEVEL_1:
            cLed_Intensity = INTENSITY_LEVEL_1;
            break;
        case LED_LEVEL_2:
            cLed_Intensity = INTENSITY_LEVEL_2;
            break;
        case LED_LEVEL_3:
            cLed_Intensity = INTENSITY_LEVEL_3;
            break;
        case LED_LEVEL_4:
            cLed_Intensity = INTENSITY_LEVEL_4;
            break;
        case LED_LEVEL_5:
            cLed_Intensity = INTENSITY_LEVEL_5;
            break;
        default:
            break;
    }  
    
    return cLed_Intensity;
}

/* 
*   led type mapping table.
*   transfor int value to mapping hex value 
*/
unsigned char kLedTypeUCharMapping(unsigned int iLed_Type)
{
    unsigned char cLed_Type = UCHAR_0x0;

    switch(iLed_Type)
    {
        case LED_DSL:
            cLed_Type = GET_LED_DSL;
            break;
        case LED_PHONE:
            cLed_Type = GET_LED_PHONE;
            break;
        case LED_WIFI:
            cLed_Type = GET_LED_WIFI;
            break;
        case LED_MOBILE:
            cLed_Type = GET_LED_MOBILE;
            break;
        case LED_BOTTOM:
            cLed_Type = GET_LED_BOTTOM;
            break;
        default:
            break;
    }    
    
    return cLed_Type;
}

static void kset_led_write(unsigned int ledVal, unsigned int color, unsigned int state)
{
    /* Write MCU led's color */
    I2C_MCU_WriteByte(SLAVE_ADDRESS, SET_LED_COLOR, ledVal, color);

    /* Write MCU led's frequency if in flashing state */
    if (state == LED_STATE_FLASH)
        I2C_MCU_WriteByte(SLAVE_ADDRESS, SET_LED_FREQUENCY, ledVal, FLASH_FREQUENCY);
   
    /* Write MCU led's state */
    I2C_MCU_WriteByte(SLAVE_ADDRESS, SET_LED_STATE, ledVal, state);
}

static void kset_led_cache_read(void)
{
    int ledVal;

    for (ledVal = 0; ledVal < COUNT_TOP_LEDS; ledVal++)
    {
        /* Write MCU the modified setting */
        if (led_cache[ledVal].dirty)
        {
            led_cache[ledVal].dirty = 0;
            kset_led_write(ledVal, led_cache[ledVal].color, led_cache[ledVal].state);
        }
    }
}

static void kset_led_cache_write(unsigned int ledVal, unsigned int color, unsigned int state)
{
    led_cache[ledVal].dirty = 1;
    led_cache[ledVal].color = color;
    led_cache[ledVal].state = state;
}

int ksetLedBottomVisibility(unsigned int visibility)
{
    unsigned char fade_val = visibility == LED_VISIBILITY_FADE_ON ? 
	FADE_IN_FREQUENCY : FADE_OUT_FREQUENCY;

    I2C_MCU_WriteByte(SLAVE_ADDRESS, SET_LED_VISIBILITY, LED_BOTTOM, fade_val);
    mdelay(CMD_DELAY_TIME);

    return ERR_CODE_OK;
}

int ksetLedVisibility(unsigned int visibility)
{
    int ret = ERR_CODE_OK;
	    
    switch (visibility)
    {
        case LED_VISIBILITY_FADE_OFF:
            /* set all led FADE OUT */            
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_DSL, FADE_OUT_FREQUENCY);
            mdelay(CMD_DELAY_TIME);
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_PHONE, FADE_OUT_FREQUENCY);            
            mdelay(CMD_DELAY_TIME);
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_WIFI, FADE_OUT_FREQUENCY);            
            mdelay(CMD_DELAY_TIME);
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_MOBILE, FADE_OUT_FREQUENCY);            
            mdelay(CMD_DELAY_TIME);
            visibility_is_on = 0;
            break;
        case LED_VISIBILITY_FADE_ON:
            /* Write stored led settings just before fading in commits */
            kset_led_cache_read();
            /* set all led FADE IN */
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_DSL, FADE_IN_FREQUENCY);
            mdelay(CMD_DELAY_TIME);
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_PHONE, FADE_IN_FREQUENCY);            
            mdelay(CMD_DELAY_TIME);
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_WIFI, FADE_IN_FREQUENCY);            
            mdelay(CMD_DELAY_TIME);
            I2C_MCU_WriteByte(SLAVE_ADDRESS,SET_LED_VISIBILITY,LED_MOBILE, FADE_IN_FREQUENCY);            
            mdelay(CMD_DELAY_TIME);
            visibility_is_on = 1;
            break;
        default:
            ret = ERR_CODE_GENERIC_FAILURE;
            break;
    }
    
    return ret; // return 0 for OK
}



int ksetLedState (unsigned int ledVal, unsigned int color, unsigned int state,
    unsigned int level)
{
    int ret = ERR_CODE_OK;
    
    if ((state != LED_STATE_OFF) && (state != LED_STATE_ON) && (state != LED_STATE_BREATH) && (state != LED_STATE_FLASH))
    {
        ret = ERR_CODE_INVALID_STATE;
        return ret;            
    }
    
    /* state value save in tmp structure, and write into mcu array after fade in */
    switch (ledVal)
    {
        case LED_DSL:
            if ((color != LED_COLOR_RED) && (color != LED_COLOR_GREEN))
            {
                ret = ERR_CODE_INVALID_COLOR;
                return ret;
            }                                               
            break;
        case LED_PHONE:
            if ((color != LED_COLOR_RED) && (color != LED_COLOR_GREEN))
            {
                ret = ERR_CODE_INVALID_COLOR;
                return ret;
            }
            break;
        case LED_WIFI:
            if (color != LED_COLOR_RED && color != LED_COLOR_GREEN)
            {
                ret = ERR_CODE_INVALID_COLOR;
                return ret;
            }
            break;
        case LED_MOBILE:
            if ((color != LED_COLOR_RED) && (color != LED_COLOR_GREEN) && (color != LED_COLOR_BLUE) && (color != LED_COLOR_PURPLE))
            {
                ret = ERR_CODE_INVALID_COLOR;
                return ret;
            }
            break;
        case LED_BOTTOM:
            if (color != LED_COLOR_RED)
            {
                ret = ERR_CODE_INVALID_COLOR;
                return ret;
            }
            break;
        default:
            ret = ERR_CODE_INVALID_LED;
            return ret;
    }

    /* When visibility is off, store the settings and write them late when 
     * visibility is back on */
    if (visibility_is_on || ledVal == LED_BOTTOM)
    {
        kset_led_write(ledVal, color, state);
	ksetLedIntensity(ledVal, level);
    }
    else
        kset_led_cache_write(ledVal, color, state);
    
    return ret; // return 0 for OK
}

int kgetLedState(unsigned int ledVal, unsigned char *color, unsigned char *state)
{
    int ret = ERR_CODE_OK;
    unsigned char uchar_led;
    BOARD_I2C_MCU_PARMS i2cMcuParms;

    if ((ledVal < LED_DSL) || (ledVal > LED_BOTTOM))
    {
        ret = ERR_CODE_INVALID_LED;
        return ret;            
    }

    uchar_led = kLedTypeUCharMapping(ledVal);    
    
    I2C_MCU_ReadByte(SLAVE_ADDRESS,uchar_led,&i2cMcuParms);
        
    memcpy(color,&i2cMcuParms.result[READ_ARRAY_COLOR],sizeof(unsigned char));    
    memcpy(state,&i2cMcuParms.result[READ_ARRAY_STATE],sizeof(unsigned char));    
    
    return ret; // return 0 for OK
}

int ksetLedIntensity(unsigned int ledVal, unsigned int level)
{
    int ret = ERR_CODE_OK, retLEDState = ERR_CODE_OK;
    unsigned char cintensity = UCHAR_0x0;
    unsigned char tempColor = UCHAR_0x0, tempState = UCHAR_0x0;

    /* check level input value */
    if ((level < LED_LEVEL_1) || (level > LED_LEVEL_5))
    {
        ret = ERR_CODE_INVALID_INTENSITY;
        return ret;
    }
    
    /* get current led state */
    if ((retLEDState = kgetLedState(ledVal, &tempColor, &tempState)) != ERR_CODE_OK)
    {
        ret = ERR_CODE_INVALID_STATE;
        return ret;
    }
    /* VF IT requested to change led intensity only for LED_STATE_ON */
    if (tempState != LED_STATE_ON)
	cintensity = kLedIntensityUCharMapping(LED_LEVEL_5);
    else
	cintensity = kLedIntensityUCharMapping(level);

    I2C_MCU_WriteByte(SLAVE_ADDRESS, SET_LED_INTENSITY, (unsigned char)ledVal, cintensity);
           
    return ret; // return 0 for OK
}

int kgetLedIntensity (unsigned int ledVal, unsigned int *level)
{
    int ret = ERR_CODE_OK;
    unsigned char uchar_led;
    BOARD_I2C_MCU_PARMS i2cMcuParms;

    if ((ledVal < LED_DSL) || (ledVal > LED_BOTTOM))
    {
        ret = ERR_CODE_INVALID_LED;
        return ret;            
    }

    uchar_led = kLedTypeUCharMapping(ledVal);   
    I2C_MCU_ReadByte(SLAVE_ADDRESS,uchar_led,&i2cMcuParms);
    
    memcpy(level,&i2cMcuParms.result[READ_ARRAY_INTENSITY],sizeof(unsigned char));    
    return ret; // return 0 for OK
}

int kgetFirmwareVersion(unsigned int *yeartop, unsigned int *yearend, unsigned int *month, unsigned int *day, unsigned int* versiontop, unsigned int* versionend)
{
    char tempStr[16];
    BOARD_I2C_MCU_PARMS i2cMcuParms;
    int ret = ERR_CODE_OK;
    
    memset (tempStr,'\0',16);
    
    /* check firmware version */
    I2C_MCU_ReadByte(SLAVE_ADDRESS,CMD_GET_FIRMWARE_VERSION,&i2cMcuParms);
        
    memcpy(yeartop,&i2cMcuParms.result[0],sizeof(unsigned char));    
    memcpy(yearend,&i2cMcuParms.result[1],sizeof(unsigned char));    
    memcpy(month,&i2cMcuParms.result[2],sizeof(unsigned char));    
    memcpy(day,&i2cMcuParms.result[3],sizeof(unsigned char));    
    memcpy(versiontop,&i2cMcuParms.result[4],sizeof(unsigned char));    
    memcpy(versionend,&i2cMcuParms.result[5],sizeof(unsigned char));
       
    return ret;
}

EXPORT_SYMBOL(ksetLedVisibility);
EXPORT_SYMBOL(ksetLedBottomVisibility);
EXPORT_SYMBOL(ksetLedState);
EXPORT_SYMBOL(kgetLedState);
EXPORT_SYMBOL(ksetLedIntensity);
EXPORT_SYMBOL(kgetLedIntensity);
EXPORT_SYMBOL(kgetFirmwareVersion);
