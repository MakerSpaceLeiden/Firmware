#include "mbed.h"

#ifndef _PINS_H_
#define _PINS_H_

#define LAOS_LED1		LED1
#define LAOS_LED2		LED2
#define LAOS_LED3		LED3
#define LAOS_LED4		LED4

#define LAOS_ETH_LINK		p29
#define LAOS_ETH_SPEED		p30

#define LAOS_IO_ENABLE		p7

#define LAOS_IO_XDIR		p23
#define LAOS_IO_XSTEP		p24

#define LAOS_IO_YDIR		p25
#define LAOS_IO_YSTEP		p26

#define LAOS_IO_ZDIR		p27
#define LAOS_IO_ZSTEP		p28

#define LAOS_IO_XHOME		p8
#define LAOS_IO_YHOME		p17

#define LAOS_IO_ZMIN		p15
#define LAOS_IO_ZMAX		p16

#define LAOS_LASER_ENABLE	p21
#define LAOS_LASER_ENABLE_POLARIY	1

#define LAOS_LASER_ON		p5
#define LAOS_LASER_ON_POLARIY	0

#define LAOS_LASER_PWM		p22
#define LAOS_LASER_PWM_POLARIY		1

#define LAOS_EXHAUST		p6
#define LAOS_EXHAUST_POLARIY	1

#define LAOS_COVER		p19
#define LAOS_COVER_POLARITY	1

#define LAOS_TEMP		NC

// other definitions
extern "C" void mbed_reset();

// status leds
extern DigitalOut led1,led2,led3,led4;

// Status and communication
extern DigitalOut eth_link;  // green
extern DigitalOut eth_speed; // yellow

// Stepper IO
extern DigitalOut enable;
extern DigitalOut xdir;
extern DigitalOut xstep;
extern DigitalOut ydir;
extern DigitalOut ystep;
extern DigitalOut zdir;
extern DigitalOut zstep;

// Inputs
extern DigitalIn xhome;
extern DigitalIn yhome;
extern DigitalIn zmin;
extern DigitalIn zmax;

typedef enum { LASER_OFF, LASER_ENABLED, LASER_FIRING } laser_state_t;
extern void LaserInit();
extern void LaserSet(laser_state_t s, double p);

extern void TempInit();
extern float laser_temp();

extern void setExhaust(bool onOff);
extern bool coverClosed();

extern PinName name2pin(char * str);
extern const char * pin2name(PinName pin);

#endif
