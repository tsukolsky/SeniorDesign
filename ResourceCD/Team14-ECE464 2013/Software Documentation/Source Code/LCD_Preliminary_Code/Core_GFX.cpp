/********************************************
Core graphics lib, adapted from adafruit lib
********************************************/

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include "ILI932X_param.h"		// Reg names, commands, etc.
#include "ReCycle_pins.h"		// One stop for configuring GPIO
#include "Core_GFX.h"
#include "glcdfont.c"
#include <stdint.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define TFTWIDTH   240
#define TFTHEIGHT  320

int16_t abs(int16_t num){
	if (num >= 0)
		return num;
	else
		return (num*-1);
}

void Delay( uint16_t d ){
	for (int i = d; i > 0; i--){
		asm("nop");
	}
}
// Constructor--> no args; pins should be set at compile-time in ReCycle_pins.h
ReCycle_GFX::ReCycle_GFX() {
	
	// Set up GPIO
	DDR_DATA_LOW  = 0xFF;
	DDR_DATA_HIGH = 0xFF;
	RD_PORT		|= RD_MASK;
	WR_PORT		|= WR_MASK;
	CD_PORT		|= CD_MASK;
	CS_PORT		|= CS_MASK;
	RESET_PORT	|= RESET_MASK;
	BL_PORT		|= BL_MASK;
	
	// Initialize to idle
	RD_IDLE;
	WR_IDLE;
	CD_COMMAND;
	CS_IDLE;
	RESET_IDLE;
	BL_OFF;

	init();
}

// Initialization common to both shield & breakout configs
// Adapted for GAVR--> 
void ReCycle_GFX::init(void) {

	CS_IDLE; // Set all control bits to idle state
	WR_IDLE;
	RD_IDLE;
	CD_DATA;
	RESET_IDLE;

	setWriteDir(); // Set up LCD data port(s) for WRITE operations

	rotation  = 0;
	cursor_y  = cursor_x = 0;
	textsize  = 1;
	textcolor = 0xFFFF;
	width    = TFTWIDTH;
	height   = TFTHEIGHT;
}

static const uint16_t ILI932x_regValues[] PROGMEM = {
	ILI932X_START_OSC        , 0x0001, // Start oscillator
	TFTLCD_DELAY             , 50,     // 50 millisecond delay
	ILI932X_DRIV_OUT_CTRL    , 0x0100,
	ILI932X_DRIV_WAV_CTRL    , 0x0700,
	ILI932X_ENTRY_MOD        , 0x1030,
	ILI932X_RESIZE_CTRL      , 0x0000,
	ILI932X_DISP_CTRL2       , 0x0202,
	ILI932X_DISP_CTRL3       , 0x0000,
	ILI932X_DISP_CTRL4       , 0x0000,
	ILI932X_RGB_DISP_IF_CTRL1, 0x0,
	ILI932X_FRM_MARKER_POS   , 0x0,
	ILI932X_RGB_DISP_IF_CTRL2, 0x0,
	ILI932X_POW_CTRL1        , 0x0000,
	ILI932X_POW_CTRL2        , 0x0007,
	ILI932X_POW_CTRL3        , 0x0000,
	ILI932X_POW_CTRL4        , 0x0000,
	TFTLCD_DELAY             , 200,
	ILI932X_POW_CTRL1        , 0x1690,
	ILI932X_POW_CTRL2        , 0x0227,
	TFTLCD_DELAY             , 50,
	ILI932X_POW_CTRL3        , 0x001A,
	TFTLCD_DELAY             , 50,
	ILI932X_POW_CTRL4        , 0x1800,
	ILI932X_POW_CTRL7        , 0x002A,
	TFTLCD_DELAY             , 50,
	ILI932X_GAMMA_CTRL1      , 0x0000,
	ILI932X_GAMMA_CTRL2      , 0x0000,
	ILI932X_GAMMA_CTRL3      , 0x0000,
	ILI932X_GAMMA_CTRL4      , 0x0206,
	ILI932X_GAMMA_CTRL5      , 0x0808,
	ILI932X_GAMMA_CTRL6      , 0x0007,
	ILI932X_GAMMA_CTRL7      , 0x0201,
	ILI932X_GAMMA_CTRL8      , 0x0000,
	ILI932X_GAMMA_CTRL9      , 0x0000,
	ILI932X_GAMMA_CTRL10     , 0x0000,
	ILI932X_GRAM_HOR_AD      , 0x0000,
	ILI932X_GRAM_VER_AD      , 0x0000,
	ILI932X_HOR_START_AD     , 0x0000,
	ILI932X_HOR_END_AD       , 0x00EF,
	ILI932X_VER_START_AD     , 0X0000,
	ILI932X_VER_END_AD       , 0x013F,
	ILI932X_GATE_SCAN_CTRL1  , 0xA700, // Driver Output Control (R60h)
	ILI932X_GATE_SCAN_CTRL2  , 0x0003, // Driver Output Control (R61h)
	ILI932X_GATE_SCAN_CTRL3  , 0x0000, // Driver Output Control (R62h)
	ILI932X_PANEL_IF_CTRL1   , 0X0010, // Panel Interface Control 1 (R90h)
	ILI932X_PANEL_IF_CTRL2   , 0X0000,
	ILI932X_PANEL_IF_CTRL3   , 0X0003,
	ILI932X_PANEL_IF_CTRL4   , 0X1100,
	ILI932X_PANEL_IF_CTRL5   , 0X0000,
	ILI932X_PANEL_IF_CTRL6   , 0X0000,
	ILI932X_DISP_CTRL1       , 0x0133, // Main screen turn on
};

void ReCycle_GFX::begin(void) {
	uint8_t i = 0;
	
	width = TFTWIDTH;
	height = TFTHEIGHT;

	reset();
	
	uint16_t a, d;
	driver = ID_932X;
	CS_ACTIVE;
	while(i < sizeof(ILI932x_regValues) / sizeof(uint16_t)) {
		a = pgm_read_word(&ILI932x_regValues[i++]);
		d = pgm_read_word(&ILI932x_regValues[i++]);
		if(a == TFTLCD_DELAY) Delay(d);
		else                  writeRegister16(a, d);
	}
	setRotation(rotation);
	setAddrWindow(0, 0, TFTWIDTH-1, TFTHEIGHT-1);
}

void ReCycle_GFX::reset(void) {
	CS_IDLE;
	CD_DATA;
	WR_IDLE;
	RD_IDLE;
	
	RESET_ACTIVE;
	_delay_ms(2);
	RESET_IDLE;
	
	CS_ACTIVE;
	CD_DATA;
	write8(0x00);
	for(uint8_t i=0; i<7; i++) WR_STROBE;
	CS_IDLE;
	
	_delay_ms(100);
}

// Sets the LCD address window (and address counter, on 932X).
// Relevant to rect/screen fills and H/V lines.  Input coordinates are
// assumed pre-sorted (e.g. x2 >= x1).
void ReCycle_GFX::setAddrWindow(int x1, int y1, int x2, int y2) {

	CS_ACTIVE;

	// Values passed are in current (possibly rotated) coordinate
	// system.  932X requires hardware-native coords regardless of
	// MADCTL, so rotate inputs as needed.  The address counter is
	// set to the top-left corner -- although fill operations can be
	// done in any direction, the current screen rotation is applied
	// because some users find it disconcerting when a fill does not
	// occur top-to-bottom.
	int x, y, t;
	switch(rotation) {
		default:
			x  = x1;
			y  = y1;
			break;
		case 1:
			t  = y1;
			y1 = x1;
			x1 = TFTWIDTH  - 1 - y2;
			y2 = x2;
			x2 = TFTWIDTH  - 1 - t;
			x  = x2;
			y  = y1;
			break;
		case 2:
			t  = x1;
			x1 = TFTWIDTH  - 1 - x2;
			x2 = TFTWIDTH  - 1 - t;
			t  = y1;
			y1 = TFTHEIGHT - 1 - y2;
			y2 = TFTHEIGHT - 1 - t;
			x  = x2;
			y  = y2;
			break;
		case 3:
			t  = x1;
			x1 = y1;
			y1 = TFTHEIGHT - 1 - x2;
			x2 = y2;
			y2 = TFTHEIGHT - 1 - t;
			x  = x1;
			y  = y2;
			break;
	}
	writeRegister16(0x0050, x1); // Set address window
	writeRegister16(0x0051, x2);
	writeRegister16(0x0052, y1);
	writeRegister16(0x0053, y2);
	writeRegister16(0x0020, x ); // Set address counter to top left
	writeRegister16(0x0021, y );

	CS_IDLE;
}

// Fast block fill operation for fillScreen, fillRect, H/V line, etc.
// Requires setAddrWindow() has previously been called to set the fill
// bounds.  'len' is inclusive, MUST be >= 1.
void ReCycle_GFX::flood(uint16_t color, uint32_t len) {
	uint16_t blocks;
	uint8_t  i, hi = color >> 8,
	lo = color;

	CS_ACTIVE;
	CD_COMMAND;
	write8(0x00); // High byte of GRAM register...
	write8(0x22); // Write data to GRAM

	// Write first pixel normally, decrement counter by 1
	CD_DATA;
	write8(hi);
	write8(lo);
	len--;

	blocks = (uint16_t)(len / 64); // 64 pixels/block
	if(hi == lo) {
		// High and low bytes are identical.  Leave prior data
		// on the port(s) and just toggle the write strobe.
		while(blocks--) {
			i = 16; // 64 pixels/block / 4 pixels/pass
			do {
				WR_STROBE; WR_STROBE; WR_STROBE; WR_STROBE; // 2 bytes/pixel
				WR_STROBE; WR_STROBE; WR_STROBE; WR_STROBE; // x 4 pixels
			} while(--i);
		}
		// Fill any remaining pixels (1 to 64)
		for(i = (uint8_t)len & 63; i--; ) {
			WR_STROBE;
			WR_STROBE;
		}
	} else {
		while(blocks--) {
			i = 16; // 64 pixels/block / 4 pixels/pass
			do {
				write8(hi); write8(lo); write8(hi); write8(lo);
				write8(hi); write8(lo); write8(hi); write8(lo);
			} while(--i);
		}
		for(i = (uint8_t)len & 63; i--; ) {
			write8(hi);
			write8(lo);
		}
	}
	CS_IDLE;
}

void ReCycle_GFX::drawFastHLine(int16_t x, int16_t y, int16_t length, uint16_t color) {
	
	int16_t x2;

	// Initial off-screen clipping
	if((length <= 0     ) || (y      <  0     ) || ( y >= height) ||
	   (x      >= width) || ((x2 = (x+length-1)) <  0      )) {
		   return;
	   }

	if(x < 0) {        // Clip left
		length += x;
		x = 0;
	}
	if(x2 >= width) { // Clip right
		x2      = width - 1;
		length  = x2 - x + 1;
	}

  setAddrWindow(x, y, x2, y);
  flood(color, length); 
  setAddrWindow(0, 0, width - 1, height - 1);
}

void ReCycle_GFX::drawFastVLine(int16_t x, int16_t y, int16_t length, uint16_t color)
{
  int16_t y2;

  // Initial off-screen clipping
  if((length <= 0      ) ||
     (x      <  0      ) || ( x                  >= width) ||
     (y      >= height) || ((y2 = (y+length-1)) <  0     )) return;
  if(y < 0) {         // Clip top
    length += y;
    y       = 0;
  }
  if(y2 >= height) { // Clip bottom
    y2      = height - 1;
    length  = y2 - y + 1;
  }

  setAddrWindow(x, y, x, y2);
  flood(color, length); 
  setAddrWindow(0, 0, width - 1, height - 1);
}

void ReCycle_GFX::fillRect(int16_t x1, int16_t y1, int16_t w, int16_t h, uint16_t fillcolor) {
	int16_t  x2, y2;

	// Initial off-screen clipping
	if(( w            <= 0     ) ||  (h             <= 0      ) ||
	   ( x1           >= width) ||  (y1            >= height) ||
	   ((x2 = x1+w-1) <  0     ) || ((y2  = y1+h-1) <  0      )) {
		   return;
	   }		   
	if(x1 < 0) { // Clip left
		w += x1;
		x1 = 0;
	}
	if(y1 < 0) { // Clip top
		h += y1;
		y1 = 0;
	}
	if(x2 >= width) { // Clip right
		x2 = width - 1;
		w  = x2 - x1 + 1;
	}
	if(y2 >= height) { // Clip bottom
		y2 = height - 1;
		h  = y2 - y1 + 1;
	}

  setAddrWindow(x1, y1, x2, y2);
  flood(fillcolor, (uint32_t)w * (uint32_t)h);
  setAddrWindow(0, 0, width - 1, height - 1);
}

void ReCycle_GFX::fillScreen(uint16_t color) {
	
	// For the 932X, a full-screen address window is already the default
	// state, just need to set the address pointer to the top-left corner.
	// Although we could fill in any direction, the code uses the current
	// screen rotation because some users find it disconcerting when a
	// fill does not occur top-to-bottom.
	uint16_t x, y;
	switch(rotation) {
		default: x = 0            ; y = 0            ; break;
		case 1 : x = TFTWIDTH  - 1; y = 0            ; break;
		case 2 : x = TFTWIDTH  - 1; y = TFTHEIGHT - 1; break;
		case 3 : x = 0            ; y = TFTHEIGHT - 1; break;
	}
	CS_ACTIVE;
	writeRegister16(0x0020, x);
	writeRegister16(0x0021, y);
		
	flood(color, (long)TFTWIDTH * (long)TFTHEIGHT);
}

void ReCycle_GFX::drawPixel(int16_t x, int16_t y, uint16_t color) {

	// Clip
	if((x < 0) || (y < 0) || (x >= width) || (y >= height)) return;

	CS_ACTIVE;
	
	int16_t t;
	switch(rotation) {
		case 1:
		t = x;
		x = TFTWIDTH  - 1 - y;
		y = t;
		break;
		case 2:
		x = TFTWIDTH  - 1 - x;
		y = TFTHEIGHT - 1 - y;
		break;
		case 3:
		t = x;
		x = y;
		y = TFTHEIGHT - 1 - t;
		break;
	}
	writeRegister16(0x0020, x);
	writeRegister16(0x0021, y);
	writeRegister16(0x0022, color);

	CS_IDLE;
}

// Issues 'raw' an array of 16-bit color values to the LCD; used
// externally by BMP examples.  Assumes that setWindowAddr() has
// previously been set to define the bounds.  Max 255 pixels at
// a time (BMP examples read in small chunks due to limited RAM).
void ReCycle_GFX::pushColors(uint16_t *data, uint8_t len, bool first) {
	uint16_t color;
	uint8_t  hi, lo;
	CS_ACTIVE;
	if(first == true) { // Issue GRAM write command only on first call
		CD_COMMAND;
		write8(0x00);
		write8(0x22);
	}
	CD_DATA;
	while(len--) {
		color = *data++;
		hi    = color >> 8; // Don't simplify or merge these
		lo    = color;      // lines, there's macro shenanigans
		write8(hi);         // going on.
		write8(lo);
	}
	CS_IDLE;
}

// Because this function is used infrequently, it configures the ports for
// the read operation, reads the data, then restores the ports to the write
// configuration.  Write operations happen a LOT, so it's advantageous to
// leave the ports in that state as a default.
uint16_t ReCycle_GFX::readPixel(int16_t x, int16_t y) {

  uint16_t c;

  if((x < 0) || (y < 0) || (x >= width) || (y >= height)) return 0;

  CS_ACTIVE;

    int16_t t;
    switch(rotation) {
     case 1:
      t = x;
      x = TFTWIDTH  - 1 - y;
      y = t;
      break;
     case 2:
      x = TFTWIDTH  - 1 - x;
      y = TFTHEIGHT - 1 - y;
      break;
     case 3:
      t = x;
      x = y;
      y = TFTHEIGHT - 1 - t;
      break;
    }
    writeRegister16(0x0020, x);
    writeRegister16(0x0021, y);
    CD_COMMAND; write8(0x00); write8(0x22); // Read data from GRAM

  setReadDir(); // Set up LCD data port(s) for READ operations
  CD_DATA;
  c   = read8();        // Do not merge or otherwise simplify
  c <<= 8;              // these lines.  It's an unfortunate
  _delay_us(1); // artifact of the macro substitution
  c  |= read8();        // shenanigans that are going on.
  setWriteDir(); // Restore LCD data port(s) to WRITE configuration
  CS_IDLE;

  return c;
}

// Ditto with the read/write port directions, as above.
uint16_t ReCycle_GFX::readID(void) {

  uint16_t id;

  CS_ACTIVE;
  CD_COMMAND;
  write8(0x00);
  WR_STROBE;     // Extra strobe because high, low bytes are the same
  setReadDir();  // Set up LCD data port(s) for READ operations
  CD_DATA;
  id   = read8();       // Do not merge or otherwise simplify
  id <<= 8;             // these lines.  It's an unfortunate
  _delay_us(1); // artifact of the macro substitution
  id  |= read8();       // shenanigans that are going on.
  CS_IDLE;
  setWriteDir();  // Restore LCD data port(s) to WRITE configuration

  return id;
}

// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t ReCycle_GFX::color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void ReCycle_GFX::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  drawPixel(x0, y0+r, color);
  drawPixel(x0, y0-r, color);
  drawPixel(x0+r, y0, color);
  drawPixel(x0-r, y0, color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
  
    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
    
  }
}

void ReCycle_GFX::drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) {
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    } 
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void ReCycle_GFX::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// used to do circles and roundrects!
void ReCycle_GFX::fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color) {

  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

// bresenham's algorithm - thx wikpedia
void ReCycle_GFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }

  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {
      drawPixel(y0, x0, color);
    } else {
      drawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

// draw a rectangle
void ReCycle_GFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y+h-1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x+w-1, y, h, color);
}

// draw a rounded rectangle!
void ReCycle_GFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  // smarter version
  drawFastHLine(x+r  , y    , w-2*r, color); // Top
  drawFastHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawFastVLine(  x    , y+r  , h-2*r, color); // Left
  drawFastVLine(  x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// fill a rounded rectangle!
void ReCycle_GFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}

// draw a triangle!
void ReCycle_GFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

// fill a triangle!
void ReCycle_GFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {

  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a){      
		a = x1;
	} else if(x1 > b) {
		b = x1;
	}		
	
    if(x2 < a) {     
		a = x2;
	} else if(x2 > b) {
		b = x2;
	}		
    drawFastHLine(a, y0, b-a+1, color);
    return;
 }

  int16_t dx01 = x1 - x0;
  int16_t dy01 = y1 - y0;
  int16_t dx02 = x2 - x0;
  int16_t dy02 = y2 - y0;
  int16_t dx12 = x2 - x1;
  int16_t dy12 = y2 - y1;
  int16_t sa   = 0;
  int16_t sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap( a, b );
    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) {
		swap(a,b)
	}		
    drawFastHLine(a, y, b-a+1, color);
  }
}

void ReCycle_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {

  int16_t i, j, byteWidth = (w + 7) / 8;

  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(pgm_read_byte(bitmap + j * byteWidth + i / 8) & _BV(i & 7)) {
	drawPixel(x+i, y+j, color);
      }
    }
  }
}

size_t ReCycle_GFX::write(uint8_t c) {
	if (c == '\n') {
		cursor_y += textsize*8;
		cursor_x = 0;
	} else if (c == '\r') {
		// skip em
	} else {
		drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
		cursor_x += textsize*6;
		if (wrap && (cursor_x > (width - textsize*6))) {
			cursor_y += textsize*8;
			cursor_x = 0;
		}			
	}
	return 1;
}

// draw a character
void ReCycle_GFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size) {

	if((x >= width)            || // Clip right
	   (y >= height)           || // Clip bottom
	   ((x + 5 * size - 1) < 0) || // Clip left
	   ((y + 8 * size - 1) < 0))   // Clip top
	return;

	for (int8_t i=0; i<6; i++ ) {
		uint8_t line;
		if (i == 5)
			line = 0x0;
		else
			line = pgm_read_byte(font+(c*5)+i);
		for (int8_t j = 0; j<8; j++) {
			if (line & 0x1) {
				if (size == 1) { // default size
					drawPixel(x+i, y+j, color);
				} else {  // big size
					fillRect(x+(i*size), y+(j*size), size, size, color);
				}
			} else if (bg != color) {
				if (size == 1) // default size
					drawPixel(x+i, y+j, bg);
				else {  // big size
					fillRect(x+i*size, y+j*size, size, size, bg);
				}
			}
			line >>= 1;
		}
	}
}

void ReCycle_GFX::setCursor(int16_t x, int16_t y) {
  cursor_x = x;
  cursor_y = y;
}


void ReCycle_GFX::setTextSize(uint8_t s) {
  textsize = (s > 0) ? s : 1;
}


void ReCycle_GFX::setTextColor(uint16_t c) {
  textcolor = c;
  textbgcolor = c; 
  // for 'transparent' background, we'll set the bg 
  // to the same as fg instead of using a flag
}

void ReCycle_GFX::setTextColor(uint16_t c, uint16_t b) {
   textcolor = c;
   textbgcolor = b; 
}

void ReCycle_GFX::setTextWrap(bool w) {
  wrap = w;
}

uint8_t ReCycle_GFX::getRotation(void) {
  rotation %= 4;
  return rotation;
}

void ReCycle_GFX::setRotation(uint8_t x) {
  x %= 4;  // cant be higher than 3
  rotation = x;
  switch (x) {
	case 0:
	case 2:
		width = WIDTH;
		height = HEIGHT;
		break;
	case 1:
	case 3:
		width = HEIGHT;
		height = WIDTH;
		break;
  }
}

// return the size of the display which depends on the rotation!
int16_t ReCycle_GFX::Width(void) { 
  return width; 
}
 
int16_t ReCycle_GFX::Height(void) { 
  return height; 
}
