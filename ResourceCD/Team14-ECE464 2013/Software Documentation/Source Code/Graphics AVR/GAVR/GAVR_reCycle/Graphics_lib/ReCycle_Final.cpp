/*
 * PortReCycle.cpp
 *
 * Created: 1/29/2013 4:57:02 PM
 *  Author: Brian
 */ 

#define F_CPU 16000000UL

// Libraries
#include "Core_GFX.h"
#include "ReCycle_pins.h"
#include "Touchscreen.h"
#include "mysqrt.h"
#include "ff.h"
#include "diskio.h"
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <stdio.h>

// Min and max pressures registered
#define MINPRESSURE 0  // set to 0 for maximum sensitivity
#define MAXPRESSURE 1000  // seems to register no matter how hard its pressed

// Frequently used colors, 16-bit hex
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Constants for generating shapes
#define BOXSIZE 25 // seems like a good size for buttons, not too bulky but still possible to press.
#define PENRADIUS 4

// Parameters for state encoding
#define HOMESCREEN 0
#define CALIBRATE 1
#define MAPS_BETA 2
#define ACCURACY_TEST 3

// Simple encoding for demo positions
#define HOME 0
#define UP 1
#define LEFT 2
#define RIGHT 3
#define DOWN 4
#define UP_LEFT 5
#define UP_RIGHT 6
#define DOWN_LEFT 7
#define DOWN_RIGHT 8

// coordinates for directional buttons
#define UP_X 265
#define UP_Y 165
#define RIGHT_X 290
#define RIGHT_Y 190
#define DOWN_X 265
#define DOWN_Y 215
#define LEFT_X 240
#define LEFT_Y 190

//uint16_t read16(FILE f);
//uint32_t read32(FILE f);
//void bmpDraw(char *FILEname, int x, int y);

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

ReCycle_GFX tft = ReCycle_GFX();

ISR (TIMER0_COMPA_vect) {  /* should be called every 10ms */
	disk_timerproc();
}

int main(void)
{
	// Set up timer--> needed for fat32 library
	TIMSK0 |= 1 << OCIE0A;  // enable interrupt for timer match a 
	OCR0A = 156;  // 10 ms interrupt at 16MHz /1024
	TCCR0B |= (1 << CS02) | (1 << CS00);  //speed = F_CPU/1024
	power_timer0_enable();
	sei();
	
	// Initialize/mount SD card
	FATFS FileSystemObject;

	if(f_mount(0, &FileSystemObject)!=FR_OK) {
	  //flag error
	 }

	DSTATUS driveStatus = disk_initialize(0);

	if(driveStatus & STA_NOINIT ||
	   driveStatus & STA_NODISK ||
	   driveStatus & STA_PROTECT
	   ) {
	  //flag error.
	 }

	FIL logFile;
	//works
	if(f_open(&logFile, "/GpsLog.txt", FA_READ | FA_WRITE | FA_OPEN_ALWAYS)!=FR_OK) {
	  //flag error
	 }

	unsigned int bytesWritten;
	f_write(&logFile, "New log opened!\n", 16, &bytesWritten);
	//Flush the write buffer with f_sync(&logFile);

	//Close and unmount. 
	f_close(&logFile);
	f_mount(0,0);

	
	// Instantiate a touchscreen and an lcd object
	TouchScreen ts = TouchScreen(XP, YP, XM, YM, 350);
	
	//int incomingData = 0;
	int minX, minY, maxX, maxY;
	int cal_X, cal_Y;
	int state, lastState;
	//int currentPosition, nextPosition; // Tracks position in navigation
	
	minX=133; minY=254; maxX=903; maxY=868; // initially calibration is hard-coded, but configurable

	tft.reset();

	//uint16_t identifier = tft.readID();
	tft.begin();
	tft.setRotation(3);

	/*if (!SD.begin(SD_CS)) // Initialize SD card
	{
		return -1;  // Quit if initialization fails
	}*/

	//bmpDraw("ReCycle.bmp", 0, 0); // load the ReCycle logo, cuz why not
	_delay_ms(5000);
	state = HOMESCREEN; // start at home screen
	lastState = CALIBRATE;  // set different than home so graphics are initialized. this will be corrected in the process
	
	//pinMode(13, OUTPUT);

    while(1)
    {
		if (state == HOMESCREEN)
		{
			//digitalWrite(13, HIGH);
			Point p = ts.getPoint();  // asks for touch coordinates, then returns pin 13 low.
			//digitalWrite(13, LOW);
  
			//pinMode(XM, OUTPUT);
			//pinMode(YP, OUTPUT);
    
			if (lastState != state)  // Just changed to this screen; initialize graphics.
			{
				//bmpDraw("Home.bmp", 0, 0);
				lastState = state;
			}
			if (p.z > MINPRESSURE && p.z < MAXPRESSURE) 
			{
				// scale from ADC of touchscreen signal to screen coordinate system, and align.
				cal_X = map(p.y, minY, maxY, 0, 320);
				cal_Y = map(p.x, minX, maxX, 240, 0);
      
				if (cal_X > 38 && cal_X < 285 && cal_Y > 81 && cal_Y < 115)
				{
        			state = ACCURACY_TEST;
      			}
      			else if (cal_X > 38 && cal_X < 285 && cal_Y > 142 && cal_Y < 178)
      			{
      			  state = CALIBRATE;
      			}
    		}
		}
		else if (state == CALIBRATE)
		{
			/*int minX_old = 0;
			int maxX_old = 0;
			int minY_old = 0;
			int maxY_old = 0;*/  // these might be used later if i want to give the option to restore the previous calibration
    
			if (lastState != state)
			{
			//bmpDraw("Calib.bmp", 0, 0);
			lastState = state;
			}
    
			// minX_old = minX; minY_old = minY; // see previous comment
			// maxX_old = maxX; maxY_old = maxY;
			minX = 190; maxX = 840;
			minY = 310; maxY = 840;
			while (state == CALIBRATE)
			{
				//digitalWrite(13, HIGH);
      			Point p = ts.getPoint();  // asks for touch coordinates, then returns pin 13 low.
      			//digitalWrite(13, LOW);
  
      			//pinMode(XM, OUTPUT);
      			//pinMode(YP, OUTPUT);
      
      			if (p.z > MINPRESSURE && p.z < MAXPRESSURE) 
      			{
        			if (p.x < minX) { minX = p.x; /*Serial.print("\tminX = "); Serial.println(p.x);*/ }
        			if (p.x > maxX) { maxX = p.x; /*Serial.print("\tmaxX = "); Serial.println(p.x);*/ }
        			if (p.y < minY) { minY = p.y; /*Serial.print("\tminY = "); Serial.println(p.y);*/ }
        			if (p.y > maxY) { maxY = p.y; /*Serial.print("\tmaxY = "); Serial.println(p.y);*/ }
        
        			cal_X = map(p.y, minY, maxY, 0, 320);
        			cal_Y = map(p.x, minX, maxX, 240, 0);
        			if (cal_X > 80 && cal_X < 240 && cal_Y > 80 && cal_Y < 160)
        			{
          				state = HOMESCREEN;
        			}
				}
			}
		}
	}
}			

#define BUFFPIXEL 40

// some functions
/*
void bmpDraw(char *FILEname, int x, int y) {

	FILE	 bmpFILE;
	int      bmpWidth, bmpHeight;   // W+H in pixels
	uint8_t  bmpDepth;              // Bit depth (currently must be 24)
	uint32_t bmpImageoffset;        // Start of image data in FILE
	uint32_t rowSize;               // Not always = bmpWidth; may have padding
	uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
	uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
	uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
	bool	  goodBmp = false;       // Set to true on valid header parse
	bool	  flip    = true;        // BMP is stored bottom-to-top
	int      w, h, row, col;
	uint8_t  r, g, b;
	uint32_t pos = 0;
	uint8_t  lcdidx = 0;
	bool	  first = true;

	if((x >= tft.Width()) || (y >= tft.Height())) return;

	//Serial.println();
	//Serial.print("Loading image '");
	//Serial.print(FILEname);
	//Serial.println('\'');
	// Open requested FILE on SD card
	if ((bmpFILE = SD.open(FILEname)) == NULL) {
		//Serial.print("FILE not found");
		return;
	}

	// Parse BMP header
	if(read16(bmpFILE) == 0x4D42) { // BMP signature
	//Serial.print("FILE size: "); Serial.println(read32(bmpFILE));
	(void)read32(bmpFILE); // Read & ignore creator bytes
	bmpImageoffset = read32(bmpFILE); // Start of image data
	//Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
	// Read DIB header
	//Serial.print("Header size: "); Serial.println(read32(bmpFILE));
	bmpWidth  = read32(bmpFILE);
	bmpHeight = read32(bmpFILE);
	if(read16(bmpFILE) == 1) { // # planes -- must be '1'
	bmpDepth = read16(bmpFILE); // bits per pixel
	//Serial.print("Bit Depth: "); Serial.println(bmpDepth);
	if((bmpDepth == 24) && (read32(bmpFILE) == 0)) { // 0 = uncompressed

	goodBmp = true; // Supported BMP format -- proceed!
	Serial.print("Image size: ");
	Serial.print(bmpWidth);
	Serial.print('x');
	Serial.println(bmpHeight);

	// BMP rows are padded (if needed) to 4-byte boundary
	rowSize = (bmpWidth * 3 + 3) & ~3;

	// If bmpHeight is negative, image is in top-down order.
	// This is not canon but has been observed in the wild.
	if(bmpHeight < 0) {
		bmpHeight = -bmpHeight;
		flip      = false;
	}

	// Crop area to be loaded
	w = bmpWidth;
	h = bmpHeight;
	if((x+w-1) >= tft.Width())  w = tft.Width()  - x;
	if((y+h-1) >= tft.Height()) h = tft.Height() - y;

	// Set TFT address window to clipped image bounds
	tft.setAddrWindow(x, y, x+w-1, y+h-1);

	for (row=0; row<h; row++) { // For each scanline...
	// Seek to start of scan line.  It might seem labor-
	// intensive to be doing this on every line, but this
	// method covers a lot of gritty details like cropping
	// and scanline padding.  Also, the seek only takes
	// place if the FILE position actually needs to change
	// (avoids a lot of cluster math in SD library).
	if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
	pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
	else     // Bitmap is stored top-to-bottom
	pos = bmpImageoffset + row * rowSize;
	if(bmpFILE.position() != pos) { // Need seek?
	bmpFILE.seek(pos);
	buffidx = sizeof(sdbuffer); // Force buffer reload
}

for (col=0; col<w; col++) { // For each column...
// Time to read more pixel data?
if (buffidx >= sizeof(sdbuffer)) { // Indeed
// Push LCD buffer to the display first
if(lcdidx > 0) {
	tft.pushColors(lcdbuffer, lcdidx, first);
	lcdidx = 0;
	first  = false;
}
bmpFILE.read(sdbuffer, sizeof(sdbuffer));
buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r,g,b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if(lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        } 
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFILE.close();
  if(!goodBmp) return; //Serial.println("BMP format not recognized.");
}
*/

// These read 16- and 32-bit types from the SD card FILE.
// BMP data is stored little-endian.
// May need to reverse subscript order if porting elsewhere.
/*
uint16_t read16(FILE f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(FILE f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}	*/