/***********************************************************************
*
*   Copyright (c) Pegatron Corporation
*   All Rights Reserved
*
*   Purpose: MCU led control api 
*
************************************************************************/

#ifndef _LEDAPI_H
#define _LEDAPI_H

#include "mcuctl_common.h"

#define SET_LED_COLOR 0x73
#define SET_LED_STATE 0x74
#define SET_LED_INTENSITY 0x75
#define SET_LED_VISIBILITY 0x76
#define SET_LED_FREQUENCY 0x77
#define SET_LED_RUN_LDROM 0x78
#define SET_LED_BOTTOM_VISIBILITY 0x79

//PegatronCVP, Frederick 20140227 - add get defines for ioctl use-- defined values are not really used
#define GET_LED_STATE  0x80
#define GET_LED_INTENSITY  0x81
#define GET_FIRMWARE_VERSION    0x88

#define INTENSITY_LEVEL_1 0x02
#define INTENSITY_LEVEL_2 0x03
#define INTENSITY_LEVEL_3 0x10
#define INTENSITY_LEVEL_4 0x32
#define INTENSITY_LEVEL_5 0x64
#define GET_LED_DSL 0x7A
#define GET_LED_PHONE 0x7B
#define GET_LED_WIFI 0x7C
#define GET_LED_MOBILE 0x7D
#define GET_LED_BOTTOM 0x7E
#define FLASH_FREQUENCY 0x70
#define READ_ARRAY_COLOR 0
#define READ_ARRAY_STATE 1
#define READ_ARRAY_INTENSITY 2
#define READ_ARRAY_VISIBILITY 3
#define READ_ARRAY_FREQUENCY 4
#define READ_ARRAY_MEMO 5
#define FADE_IN_FREQUENCY 0x09 // fade in range is 0x01~ 0x49, and plus 1 is plus 100ms
#define FADE_OUT_FREQUENCY 0x59 // fade out range is 0x51~0x99, and  plus 1 is plus 100ms
#define CMD_DELAY_TIME 5 // 5ms delay time between two commands
#define FADE_IN_DELAY_TIME 800 // 800ms delay time after fade in 
#define GPIO_PULL_HIGH 0x01
#define GPIO_PULL_LOW 0x00

typedef enum{
        LED_DSL = 0,
        LED_PHONE = 1,
        LED_WIFI = 2,
        LED_MOBILE = 3,
        LED_BOTTOM = 4
}LED_TYPE;

typedef enum{
        LED_COLOR_RED = 0,
        LED_COLOR_GREEN = 1,
        LED_COLOR_BLUE =2,
        LED_COLOR_PURPLE =3
}LED_COLOR;

typedef enum{
        LED_LEVEL_1 = 0,
        LED_LEVEL_2 = 1,
        LED_LEVEL_3 = 2,
        LED_LEVEL_4 = 3,
        LED_LEVEL_5 = 4
}LED_LEVEL;

typedef enum{
        LED_STATE_OFF = 0,
        LED_STATE_ON = 1,
        LED_STATE_BREATH = 4,
        LED_STATE_FLASH =5
}LED_STATE;

typedef enum{
        LED_VISIBILITY_FADE_OFF = 0,
        LED_VISIBILITY_FADE_ON =1
}LED_VISIBILITY;

typedef enum{
        ERR_CODE_OK = 0,
        ERR_CODE_GENERIC_FAILURE = 1,
        ERR_CODE_INVALID_COLOR=2,
        ERR_CODE_INVALID_STATE=3,
        ERR_CODE_INVALID_INTENSITY=4,
        ERR_CODE_INVALID_LED=5
}LED_ERROR_CODE;

typedef struct McuLedState
{
    unsigned char State;
} MCU_LED_STATE_PARMS;

int setLedVisibility(unsigned int visibility);
int setLedBottomVisibility(unsigned int visibility);
int setLedState (unsigned int ledVal, unsigned int color, unsigned int state);
int getLedState(unsigned int ledVal, unsigned int* color, unsigned int *state);
int setLedIntensity(unsigned int ledVal, unsigned int level);
int getLedIntensity (unsigned int ledVal, unsigned int* level);
int doMcuFwUpgrade(const char* fwpath);
int getFirmwareVersion (unsigned int *yeartop, unsigned int *yearend, unsigned int *month, unsigned int *day, unsigned int* versiontop, unsigned int* versionend);
#endif // #ifndef _LEDAPI_H
