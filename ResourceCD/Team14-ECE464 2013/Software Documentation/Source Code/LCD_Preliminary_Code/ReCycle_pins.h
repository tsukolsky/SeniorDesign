#ifndef ReCycle_pins_H
#define ReCycle_pins_H

// This takes inspiration from the code I wrote for the 5' screen, RIP.
// Some functions and syntax were adjusted for the Adafruit lib.

// This version is for the GAVR on our home-brewed PCB.
// #including this file allows for macro port/pin access
// and inline write and read functions.

// control line pins
#define	CD		PJ7
#define RS		CD
#define	WR		PJ6
#define	RD		PJ5
#define	CS		PJ4
#define	RESET	PJ3
#define BL_EN	PJ2
#define XP		PF3
#define YP		PF2
#define XM		PF1
#define YM		PF0

// Ports
#define DATA_LOW		PORTA
#define DATA_HIGH		PORTC	// Not needed for this screen--we'll keep jic.
#define CONTROL_PORT	PORTJ
#define TOUCH_PORT		PORTF

// Data ports ***Used for reading input (data FROM breakout TO avr)***
#define DATA_LOW_IN		PINA
#define DATA_HIGH_IN	PINC	// also not needed

// Data-direction registers
#define DDR_DATA_LOW	DDRA
#define DDR_DATA_HIGH	DDRC
#define DDR_CONTROL		DDRJ
#define DDR_TOUCH		DDRF

// macros for shorthand
#define RD_PORT			CONTROL_PORT
#define WR_PORT			CONTROL_PORT
#define CD_PORT			CONTROL_PORT
#define CS_PORT			CONTROL_PORT
#define RESET_PORT		CONTROL_PORT
#define BL_PORT			CONTROL_PORT
#define RD_MASK			(1<<RD)
#define WR_MASK			(1<<WR)
#define CD_MASK			(1<<CD)
#define CS_MASK			(1<<CS)
#define RESET_MASK		(1<<RESET)
#define BL_MASK			(1<<BL_EN)

#define SD_PORT	PORTC
#define SD_CS	PC0

// Control signals are ACTIVE LOW (idle is HIGH)
// Command/Data: LOW = command, HIGH = data
// These are single-instruction operations and always inline
#define RD_ACTIVE		RD_PORT &= ~RD_MASK
#define RD_IDLE			RD_PORT |=  RD_MASK
#define WR_ACTIVE		WR_PORT &= ~WR_MASK
#define WR_IDLE			WR_PORT |=  WR_MASK
#define CD_COMMAND		CD_PORT &= ~CD_MASK
#define CD_DATA			CD_PORT |=  CD_MASK
#define CS_ACTIVE		CS_PORT &= ~CS_MASK
#define CS_IDLE			CS_PORT |=  CS_MASK
#define RESET_ACTIVE	RESET_PORT	&= ~RESET_MASK;
#define RESET_IDLE		RESET_PORT	|=  RESET_MASK;
#define BL_ON			BL_PORT		|=  BL_MASK;
#define BL_OFF			BL_PORT		&= ~BL_MASK;

// adjust data port directions for read/write
#define setWriteDirInline() { DDR_DATA_LOW = 0xFF; }		// just the port we need, save an instruction.
#define setReadDirInline()  { DDR_DATA_LOW = 0x00; }
#define setWriteDir()		setWriteDirInline();
#define setReadDir()		setReadDirInline();

// latches data for read/write
#define RD_STROBE   RD_ACTIVE, RD_IDLE
#define WR_STROBE { WR_ACTIVE; WR_IDLE; }
	
#define write8inline(d)	{ DATA_LOW = ((d) & 0xFF); WR_STROBE; }
#define read8inline() (RD_STROBE, PIND)
#define write8 write8inline
#define read8 read8inline

#define writeRegister8inline(a, d) { \
  CD_COMMAND; write8(a); CD_DATA; write8(d); }
  
#define writeRegister16inline(a, d) { \
uint8_t hi, lo; \
hi = (a) >> 8; lo = (a); CD_COMMAND; write8(hi); write8(lo); \
hi = (d) >> 8; lo = (d); CD_DATA   ; write8(hi); write8(lo); }

#define writeRegister16 writeRegister16inline

// Set value of 2 TFT registers: Two 8-bit addresses (hi & lo), 16-bit value
#define writeRegisterPairInline(aH, aL, d) { \
uint8_t hi = (d) >> 8, lo = (d); \
CD_COMMAND; write8(aH); CD_DATA; write8(hi); \
CD_COMMAND; write8(aL); CD_DATA; write8(lo); }

#endif