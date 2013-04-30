#ifndef Core_GFX_H
#define Core_GFX_H

#include <stdint.h>
#include <avr/pgmspace.h>

#define swap(a, b) { int16_t t = a; a = b; b = t; }

class ReCycle_GFX{
	public:

		ReCycle_GFX(void);
		
		uint16_t textbgcolor;
		bool  wrap; // If set, 'wrap' text at right edge of display
		int rotation;
		int cursor_y, cursor_x;
		int textsize;
		uint32_t textcolor;
		int width, height;
		int16_t  WIDTH, HEIGHT;
		
		void	begin(void),
				drawPixel(int16_t x, int16_t y, uint16_t color),
				drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t),
				drawFastHLine(int16_t x0, int16_t y0, int16_t w, uint16_t color),
				drawFastVLine(int16_t x0, int16_t y0, int16_t h, uint16_t color),
				drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t),
				fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c),
				fillScreen(uint16_t color),
				reset(void),
				setRotation(uint8_t x),
				drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color),
				drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color),
				fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color),
				fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color),
				drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color),
				fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color),
				drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color),
				fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color),
				drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color),
				drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size),
				setCursor(int16_t x, int16_t y),
				setTextSize(uint8_t s),
				setTextColor(uint16_t c),
				setTextColor(uint16_t c, uint16_t b),
				setTextWrap(bool w),
				// These methods are public in order for BMP examples to work:
				setAddrWindow(int x1, int y1, int x2, int y2),
				pushColors(uint16_t *data, uint8_t len, bool first);

		uint16_t	color565(uint8_t r, uint8_t g, uint8_t b),
					readPixel(int16_t x, int16_t y),
					readID(void);
		int16_t		Width(void),
					Height(void);
					
		uint8_t		getRotation(void);
					
		size_t		write(uint8_t c);

	private:

		void	init(),
		flood(uint16_t color, uint32_t len);
		uint8_t  driver;
};

#endif