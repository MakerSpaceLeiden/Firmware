/*
 * Pin definitions
 *
 */
#include "mbed.h"
#include "global.h"
#include "pins.h"

// status leds
DigitalOut led1(LAOS_LED1);
DigitalOut led2(LAOS_LED2);
DigitalOut led3(LAOS_LED3);
DigitalOut led4(LAOS_LED4);

// Status and communication
DigitalOut eth_link(LAOS_ETH_LINK); // green
DigitalOut eth_speed(LAOS_ETH_SPEED); // yellow

// Stepper IO
DigitalOut enable(LAOS_IO_ENABLE);
DigitalOut xdir(LAOS_IO_XDIR);
DigitalOut xstep(LAOS_IO_XSTEP);
DigitalOut ydir(LAOS_IO_YDIR);
DigitalOut ystep(LAOS_IO_YSTEP);
DigitalOut zdir(LAOS_IO_ZDIR);
DigitalOut zstep(LAOS_IO_ZSTEP);

// Inputs;
DigitalIn xhome(LAOS_IO_XHOME);
DigitalIn yhome(LAOS_IO_YHOME);
DigitalIn zmin(LAOS_IO_ZMIN);
DigitalIn zmax(LAOS_IO_ZMAX);

// laser IO
PwmOut *pwm = NULL;
DigitalOut *pwm_io = NULL;

DigitalOut *laser_enable = NULL;
DigitalOut *laser_on = NULL;    

DigitalOut * exhaust = NULL;
DigitalIn * cover = NULL;
AnalogIn  * temp = NULL;

extern GlobalConfig *cfg;

void LaserInit() {
   if (cfg->laser_enable_pin != NC) {
	laser_enable = new DigitalOut(cfg->laser_enable_pin, !cfg->laser_enable_polarity);
	printf("Laser Enable function active (pin %x, enabled=%s)" NL, cfg->laser_enable_pin, cfg->laser_enable_polarity ? "high" : "low");
   };
   if (cfg->laser_on_pin != NC) {
	laser_on = new DigitalOut(cfg->laser_on_pin, !cfg->laser_on_polarity);
	printf("Laser ON (firing) function active (pin %x, firing=%s)" NL, cfg->laser_on_pin, cfg->laser_on_polarity ? "high" : "low");
   };
   if (cfg->exhaust_pin != NC) {
	exhaust = new DigitalOut(cfg->exhaust_pin, !cfg->exhaust_polarity);
	printf("Exhaust function active (pin %x, on=%s)" NL, cfg->exhaust_pin, cfg->exhaust_polarity ? "high" : "low");
   };
   if (cfg->cover_pin != NC) {
	cover = new DigitalIn(cfg->cover_pin);
        cover->mode(PullUp);
	printf("Cover function active (pin %x, closed=%s)" NL, cfg->cover_pin, cfg->cover_polarity ? "high" : "low");
   };
};

void LaserSet(laser_state_t s, double power) {
  static int l_s;
  static double l_p = -1;
  bool enable = 0, on = 0;
  bool ch = (s != l_s) || (l_p != power);
  l_s = s; l_p = power;

  if (ch) printf("LaserSet(%s,%f)" NL, (s == LASER_OFF) ? "off" : ((s == LASER_ENABLED) ? "enabled" : ((s == LASER_FIRING) ? "firing" : "???")), power);

  switch(s) {
  case LASER_OFF:
     enable = on = false;
     break;
  case LASER_ENABLED:
     enable = true;
     on = false;
     break;
  case LASER_FIRING:
     enable = true;
     on = true;
     break;
  };
  if (on && power < 0.01) 
       on = false;

  if (laser_enable) {
	if (ch) printf("  laser_enable to %d (flag %s, pin %s)" NL,  
		enable ? cfg->laser_enable_polarity : !cfg->laser_enable_polarity, enable ? "true" : "false", pin2name(cfg->laser_enable_pin)); 

	*laser_enable = enable ? cfg->laser_enable_polarity : !cfg->laser_enable_polarity;
  };

  if (laser_on) {
       if (ch) printf("  laser_on     to %d (flag %s, pin %s)" NL,   
		on ? cfg->laser_on_polarity : !cfg->laser_on_polarity, on ? "true" : "false", pin2name(cfg->laser_on_pin));

       *laser_on = on ? cfg->laser_on_polarity : !cfg->laser_on_polarity;
  };

  if (cfg->laser_pwm_pin != NC) {
     if (on) {
        if (pwm_io) { 
		delete(pwm_io); 
		pwm_io = NULL; 
	};
	if (pwm == NULL) {
		pwm = new PwmOut(cfg->laser_pwm_pin);
                pwm->period_us(1e6 / cfg->pwmfreq);
	};
        if (ch) printf("  pwm          to %f %% (duty cycle, pin %s)" NL,  power,pin2name(cfg->laser_pwm_pin));
        *pwm = (float) power;
     } else {
	if (pwm) { 	
		delete(pwm);
		pwm = NULL; 
	};
        if (pwm_io == NULL) { 
		pwm_io = new DigitalOut(cfg->laser_pwm_pin, !cfg->laser_pwm_polarity);
	};
        if (ch) printf("  pwm          to %d (digital, flag false, pin %s)" NL,  !cfg->laser_pwm_polarity, pin2name(cfg->laser_pwm_pin));
        *pwm_io = !cfg->laser_pwm_polarity;
     };
  }
  if (ch) fflush(stdout);
}

void TempInit() {
  if (cfg->laser_temp_pin == NC)
	return;
  temp = new AnalogIn(cfg->laser_temp_pin);
  printf("Temperature sensor active (pin %x)" NL, cfg->laser_temp_pin);
};

static float raw2volt(unsigned short raw) {
  const float VCC = 3.3f;
  return raw * VCC / (1<<16);
}

static float volt2temp(float Vin) {
  #define K2C (273.15) // 0 C in Kelvin
  #define T25 (K2C + 25.0f) // 25 C in Kelvin

  // NTC resistor spec from datasheet.
  #define Rt  (10*1000.f)       // At 25 C
  #define B  (3435.0f)

  // Reference voltage and resistor to ground.
  #define Vref (2.495f)
  #define Rref  (10*1000.f)

  // float r = Rref * (Vref/Vin- 1);

  return 1.0f / ( log((Vref/Vin- 1))/B + 1/T25) - K2C;
}

float laser_temp() {
  if (temp == NULL || cfg == NULL || cfg->laser_temp_pin == NC)
	return -300.0;
  return volt2temp(*temp);
};

typedef struct { const char * str; PinName pin; } pntable;
pntable n2p[] = {
        { "p5",  p5  },{ "p6",  p6  },{ "p7",  p7  },{ "p8",  p8  },{ "p9",  p9  },
        { "p10", p10 },{ "p11", p11 },{ "p12", p12 },{ "p13", p13 },{ "p14", p14 },
        { "p15", p15 },{ "p16", p16 },{ "p17", p17 },{ "p18", p18 },{ "p19", p19 },
        { "p20", p20 },{ "p21", p21 },{ "p22", p22 },{ "p23", p23 },{ "p24", p24 },
        { "p25", p25 },{ "p26", p26 },{ "p27", p27 },{ "p28", p28 },{ "p29", p29 },
        { "p30", p30 },
#if defined(TARGET_LPC11U24)
        { "p33", p33 },{ "p34", p34 },{ "p35", 35 },{ "p36", p36 },%
#endif
        { "LED1", LED1 }, { "LED2", LED2 }, { "LED3", LED3 }, { "LED4", LED4 },
        { "NC", NC },
        { NULL, NC },
};

PinName name2pin(char * str) {
   for(pntable *p = n2p; p->str; p++) {
	if (strcasecmp(str, p->str) == 0)
		return p->pin;
	};
   return NC;
};

const char * pin2name(PinName pin) {
   for(pntable *p = n2p; p->str; p++) {
	if (pin == p->pin)
		return p->str;
	};
   return "??";
};

void setExhaust(bool onOff) {
	static int l = -3;
	if (l != (int) onOff)
		printf("setExhaust(%s) - pin %s to %d" NL, onOff ? "true" : "false", pin2name(cfg->exhaust_pin),  onOff ? cfg->exhaust_polarity : !cfg->exhaust_polarity);
	l = (int) onOff;

	if (*exhaust)
		* exhaust = onOff ? cfg->exhaust_polarity : cfg->exhaust_polarity;
};

bool coverClosed() {
	if (cover)
		return *cover ? cfg->cover_polarity : !cfg->cover_polarity;
	return true;
};

