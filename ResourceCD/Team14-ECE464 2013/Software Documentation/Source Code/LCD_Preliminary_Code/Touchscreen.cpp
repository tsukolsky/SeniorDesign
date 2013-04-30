// Touch screen library with X Y and Z (pressure) readings as well
// as oversampling to avoid 'bouncing'
// Adapted from Adafruit version; see their copyright.
// (c) ladyada / adafruit
// Code under MIT License

#include <avr/pgmspace.h>
#include "TouchScreen.h"
#include "ReCycle_pins.h"

// increase or decrease the touchscreen oversampling. This is a little different than you make think:
// 1 is no oversampling, whatever data we get is immediately returned
// 2 is double-sampling and we only return valid data if both points are the same
// 3+ uses insert sort to get the median value.
// We found 2 is precise yet not too slow so we suggest sticking with it!

#define NUMSAMPLES 2
#define analog_reference 0x01

// This function performs an ADC on the pin specified.
int analogRead(uint8_t pin)
{
	uint8_t low, high;

	if (pin >= 14) pin -= 14; // allow for channel or pin numbers

	// set the analog reference (high two bits of ADMUX) and select the
	// channel (low 4 bits).  this also sets ADLAR (left-adjust result)
	// to 0 (the default).
	ADMUX = (analog_reference << 6) | (pin & 0x07);

	// start the conversion
	//sbi(ADCSRA, ADSC);
	ADCSRA |= (1<<ADSC);

	// ADSC is cleared when the conversion finishes
	while (bit_is_set(ADCSRA, ADSC));

	// we have to read ADCL first; doing so locks both ADCL
	// and ADCH until ADCH is read.  reading ADCL second would
	// cause the results of each conversion to be discarded,
	// as ADCL and ADCH would be locked when it completed.
	low  = ADCL;
	high = ADCH;

	// combine the two bytes
	return (high << 8) | low;
}

Point::Point(void) {
	x = y = 0;
}

Point::Point(int16_t x0, int16_t y0, int16_t z0) {
	x = x0;
	y = y0;
	z = z0;
}

bool Point::operator==(Point p1) {
	return  ((p1.x == x) && (p1.y == y) && (p1.z == z));
}

bool Point::operator!=(Point p1) {
	return  ((p1.x != x) || (p1.y != y) || (p1.z != z));
}

#if (NUMSAMPLES > 2)
static void insert_sort(int array[], uint8_t size) {
	uint8_t j;
	int save;
	
	for (int i = 1; i < size; i++) {
		save = array[i];
		for (j = i; j >= 1 && save < array[j - 1]; j--)
		array[j] = array[j - 1];
		array[j] = save;
	}
}
#endif

Point TouchScreen::getPoint(void) {
	int x, y, z;
	int samples[NUMSAMPLES];
	uint8_t i, valid;
	

	/*uint8_t xp_port = digitalPinToPort(_xp);
	uint8_t yp_port = digitalPinToPort(_yp);
	uint8_t xm_port = digitalPinToPort(_xm);
	uint8_t ym_port = digitalPinToPort(_ym);

	uint8_t xp_pin = digitalPinToBitMask(_xp);
	uint8_t yp_pin = digitalPinToBitMask(_yp);
	uint8_t xm_pin = digitalPinToBitMask(_xm);
	uint8_t ym_pin = digitalPinToBitMask(_ym);*/


	valid = 1;

	DDR_TOUCH &= ~(1<<YP);
	DDR_TOUCH &= ~(1<<YM);
	//pinMode(_yp, INPUT);
	//pinMode(_ym, INPUT);
	
	TOUCH_PORT &= ~(1<<YP);
	TOUCH_PORT &= ~(1<<YM);
	//*portOutputRegister(yp_port) &= ~yp_pin;
	//*portOutputRegister(ym_port) &= ~ym_pin;
	//digitalWrite(_yp, LOW);
	//digitalWrite(_ym, LOW);
	
	DDR_TOUCH |= (1<<XP);
	DDR_TOUCH |= (1<<XM);
	//pinMode(_xp, OUTPUT);
	//pinMode(_xm, OUTPUT);
	//digitalWrite(_xp, HIGH);
	//digitalWrite(_xm, LOW);
	TOUCH_PORT |=  (1<<XP);
	TOUCH_PORT &= ~(1<<XM);
	//*portOutputRegister(xp_port) |= xp_pin;
	//*portOutputRegister(xm_port) &= ~xm_pin;
	
	for (i=0; i<NUMSAMPLES; i++) {
		samples[i] = analogRead(_yp);
	}
	#if NUMSAMPLES > 2
		insert_sort(samples, NUMSAMPLES);
	#endif
	#if NUMSAMPLES == 2
		if (samples[0] != samples[1]) { valid = 0; }
	#endif
	x = (1023-samples[NUMSAMPLES/2]);

	DDR_TOUCH &= ~(1<<XP);
	DDR_TOUCH &= ~(1<<XM);
	//pinMode(_xp, INPUT);
	//pinMode(_xm, INPUT);
	TOUCH_PORT &= ~(1<<XP);
	//*portOutputRegister(xp_port) &= ~xp_pin;
	//digitalWrite(_xp, LOW);

	DDR_TOUCH |= (1<<YP);
	//pinMode(_yp, OUTPUT);
	TOUCH_PORT |= (1<<YP);
	//*portOutputRegister(yp_port) |= yp_pin;
	//digitalWrite(_yp, HIGH);
	DDR_TOUCH |= (1<<YM);
	//pinMode(_ym, OUTPUT);

	for (i=0; i<NUMSAMPLES; i++) {
		samples[i] = analogRead(_xm);
	}

	#if NUMSAMPLES > 2
		insert_sort(samples, NUMSAMPLES);
	#endif
	#if NUMSAMPLES == 2
		if (samples[0] != samples[1]) { valid = 0; }
	#endif

   y = (1023-samples[NUMSAMPLES/2]);

   // Set X+ to ground
   DDR_TOUCH |= (1<<XP);
   //pinMode(_xp, OUTPUT);
   TOUCH_PORT &= ~(1<<XP);
   //*portOutputRegister(xp_port) &= ~xp_pin;
   //digitalWrite(_xp, LOW);
  
   // Set Y- to VCC
   TOUCH_PORT |= (1<<YM);
   //*portOutputRegister(ym_port) |= ym_pin;
   //digitalWrite(_ym, HIGH); 
  
   // Hi-Z X- and Y+
   TOUCH_PORT &= ~(1<<YP);
   //*portOutputRegister(yp_port) &= ~yp_pin;
   //digitalWrite(_yp, LOW);
   DDR_TOUCH &= ~(1<<YP);
   //pinMode(_yp, INPUT);
  
   int z1 = analogRead(_xm); 
   int z2 = analogRead(_yp);

   if (_rxplate != 0) {
     // now read the x 
     float rtouch;
     rtouch = z2;
     rtouch /= z1;
     rtouch -= 1;
     rtouch *= x;
     rtouch *= _rxplate;
     rtouch /= 1024;
     
     z = rtouch;
   } else {
     z = (1023-(z2-z1));
   }

   if (! valid) {
     z = 0;
   }

   return Point(x, y, z);
}

TouchScreen::TouchScreen(uint8_t xp, uint8_t yp, uint8_t xm, uint8_t ym) {
  _yp = yp;
  _xm = xm;
  _ym = ym;
  _xp = xp;
  _rxplate = 0;
  pressureThreshhold = 10;
}


TouchScreen::TouchScreen(uint8_t xp, uint8_t yp, uint8_t xm, uint8_t ym,
			 uint16_t rxplate) {
  _yp = yp;
  _xm = xm;
  _ym = ym;
  _xp = xp;
  _rxplate = rxplate;

  pressureThreshhold = 10;
}

int TouchScreen::readTouchX(void) {
	DDR_TOUCH &= ~(1<<YP);
	DDR_TOUCH &= ~(1<<YM);
	TOUCH_PORT &= ~(1<<YP);
	TOUCH_PORT &= ~(1<<YM);
   // pinMode(_yp, INPUT);
   // pinMode(_ym, INPUT);
   // digitalWrite(_yp, LOW);
   // digitalWrite(_ym, LOW);
   
   DDR_TOUCH |= (1<<XP);
   DDR_TOUCH |= (1<<XM);
   TOUCH_PORT |= (1<<XP);
   TOUCH_PORT |= (1<<XM);
   //pinMode(_xp, OUTPUT);
   //digitalWrite(_xp, HIGH);
   //pinMode(_xm, OUTPUT);
   //digitalWrite(_xm, LOW);
   
   return (1023-analogRead(_yp));
}


int TouchScreen::readTouchY(void) {
	DDR_TOUCH &= ~(1<<XP);
	DDR_TOUCH &= ~(1<<XM);
	TOUCH_PORT &= ~(1<<XP);
	TOUCH_PORT &= ~(1<<XM);
   //pinMode(_xp, INPUT);
   //pinMode(_xm, INPUT);
   //digitalWrite(_xp, LOW);
   //digitalWrite(_xm, LOW);
   
   DDR_TOUCH |= (1<<YP);
   DDR_TOUCH |= (1<<YM);
   TOUCH_PORT |= (1<<YP);
   TOUCH_PORT |= (1<<YM);
   //pinMode(_yp, OUTPUT);
   //digitalWrite(_yp, HIGH);
   //pinMode(_ym, OUTPUT);
   //digitalWrite(_ym, LOW);
   
   return (1023-analogRead(_xm));
}


uint16_t TouchScreen::pressure(void) {
  // Set X+ to ground
  DDR_TOUCH |=  (1<<XP);
  TOUCH_PORT &= ~(1<<XP);
  //pinMode(_xp, OUTPUT);
  //digitalWrite(_xp, LOW);
  
  // Set Y- to VCC
  DDR_TOUCH |= (1<<YM);
  TOUCH_PORT |= (1<<YM);
  //pinMode(_ym, OUTPUT);
  //digitalWrite(_ym, HIGH); 
  
  // Hi-Z X- and Y+
  TOUCH_PORT &= ~(1<<XM);
  DDR_TOUCH &= ~(1<<XM);
  TOUCH_PORT &= ~(1<<YP);
  DDR_TOUCH &= ~(1<<YP);
 // digitalWrite(_xm, LOW);
 // pinMode(_xm, INPUT);
 // digitalWrite(_yp, LOW);
 // pinMode(_yp, INPUT);
  
  int z1 = analogRead(_xm); 
  int z2 = analogRead(_yp);

  if (_rxplate != 0) {
    // now read the x 
    float rtouch;
    rtouch = z2;
    rtouch /= z1;
    rtouch -= 1;
    rtouch *= readTouchX();
    rtouch *= _rxplate;
    rtouch /= 1024;
    
    return rtouch;
  } else {
    return (1023-(z2-z1));
  }
}
