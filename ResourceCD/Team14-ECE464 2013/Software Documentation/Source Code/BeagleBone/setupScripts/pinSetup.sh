#!/bin/bash
########################################################################
## pinSetup.sh
## Author: Todd Sukolsky
## Copyright of Todd Sukolsky and Re.Cycle
## Date Created: 2/9/13
## Last Revices: 4/19/13
########################################################################
## Description:
##    This script prepares all pins and ports needed for use on the 
## BeagleBone. If you specify the option to '-b', it will blink an LED.
## This is unneeded funtionally though. Note the pin, mode, and echo value
## for the UART and pin assignments. Also, not that these echo values
## are in HEX.
##
########################################################################
## Revisions:
##	4/3: Added GPIO allocations for ports.
##	4/10: Changed alert pins to inputs, changed a GPIO to WAVR to output
##	      that goes high off start (GPIO2_3).
########################################################################
## Notes:
##	(1): B6|B5|B4|B3|B2|B1|B0
##	     B6: 0, B5: Input-0, Output-1, B4: PullUp-0, PullDown-1, B3: PullEnable-0, Disable-1
##	     B2-B0: Mode
########################################################################


###################################################################################################
## Setup BeagleBone UART1 : BB P9, pin 24 -> UART1_TXD -> Watchdog_AVR_rxd
#
echo 8 > /sys/kernel/debug/omap_mux/uart1_txd

####################################################################################################
## Setup BeagleBone UART1 : BB P9, pin 26 -> UART1_RXD -> Watchdog_AVR_txd
#
echo 28 > /sys/kernel/debug/omap_mux/uart1_rxd

####################################################################################################
## Setup BeagleBone UART2 : BB P9, pin 21 -> UART2_TXD -> GPS_RXD (mode 1)
#
echo 9 > /sys/kernel/debug/omap_mux/spi0_d0

###################################################################################################
## Setup BeagleBone UART2 : BB P9, pin 22 -> UART2_RXD -> GPS_TXD (mode 1)
#
echo 29 > /sys/kernel/debug/omap_mux/spi0_sclk 

##################################################################################################
## Setup BeagleBone UART4 : BB P9, pin 11 -> UART4_RXD -> GAVR_TXD (mode 6)
#
echo 2e > /sys/kernel/debug/omap_mux/gpmc_wait0

##################################################################################################
## Setup BeagleBone UART4 : BB P9, pin 13 -> UART4_TXD -> GAVR_RXD (mode 6)
#
echo e > /sys/kernel/debug/omap_mux/gpmc_wpn


##################################################################################################
## Setup BeagleBone UART5 : BB P8, pin 37 -> UART5_TXD -> GAVR_RXD2 (mode 4)
#
echo c > /sys/kernel/debug/omap_mux/lcd_data8

##################################################################################################
## Setup BeagleBone UART4 : BB P9, pin 38 -> UART5_RXD -> GAVR_TXD2 (mode 4)
#
echo 2c > /sys/kernel/debug/omap_mux/lcd_data9

####################################################################################################
##  BeagleBone P8, pin 3 -> GPIO1_6 (32+6) -> INT2 on WAVR
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad6
#setup actual GPIO
echo 38 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio38/direction
echo 0 > /sys/class/gpio/gpio38/value

###################################################################################################
## BeagleBone P8, pin 4 -> GPIO1_7 (32+7) -> INT1 on GAVR
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad7	
#setup actual GPIO
echo 39 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio39/direction
echo 0 > /sys/class/gpio/gpio39/value

####################################################################################################
##  BeagleBone P8, pin 5 -> GPIO1_2 (32+2) -> BONE_INT/GAVRO
#
#setu2 mode
echo 2f > /sys/kernel/debug/omap_mux/gpmc_ad2
#setup actual GPIO
echo 34 > /sys/class/gpio/export
echo "in" > /sys/class/gpio/gpio34/direction
echo rising > /sys/class/gpio/gpio34/edge

####################################################################################################
##  BeagleBone P8, pin 6 -> GPIO1_3 (32+3) -> BONE_INT/WAVRO
#
#setup mode
echo 2f > /sys/kernel/debug/omap_mux/gpmc_ad3
#setup actual GPIO
echo 35 > /sys/class/gpio/export
echo "in" > /sys/class/gpio/gpio35/direction
echo rising > /sys/class/gpio/gpio35/edge

####################################################################################################
##  BeagleBone P8, pin 7 -> GPIO2_2 (64+2) -> BB_INT (about to be shut down).
#
#setup mode
echo 2f > /sys/kernel/debug/omap_mux/gpmc_advn_ale
#setup actual GPIO
echo 66 > /sys/class/gpio/export
echo "in" > /sys/class/gpio/gpio66/direction
echo rising > /sys/class/gpio/gpio35/edge

####################################################################################################
##  BeagleBone P8, pin 8 -> GPIO2_3 (64+3) -> WIO6/BB8_8
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_oen_ren
#setup actual GPIO
echo 67 > /sys/class/gpio/export
echo "high" > /sys/class/gpio/gpio67/direction

####################################################################################################
##  BeagleBone P8, pin 9 -> GPIO2_5 (64+5) -> WIO0/BB8_9 -> BoneOn GPIO to WAVR
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ben0_cle
#setup actual GPIO
echo 69 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio69/direction
echo 1 > /sys/class/gpio/gpio69/value

####################################################################################################
##  BeagleBone P8, pin 10 -> GPIO2_4 (64+4) -> WIO1/BB8_10
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_wen
#setup actual GPIO
echo 68 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio68/direction

####################################################################################################
##  BeagleBone P8, pin 11 -> GPIO1_13 (32+13) -> WIO2/BB8_11
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad13
#setup actual GPIO
echo 45 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio45/direction

####################################################################################################
##  BeagleBone P8, pin 12 -> GPIO1_12 (32+12) -> GIO3/BB8_12 -> UART2 INTERRUPT.
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad12
#setup actual GPIO
echo 44 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio44/direction
echo 0 > /sys/class/gpio/gpio44/value

####################################################################################################
##  BeagleBone P8, pin 13 -> GPIO0_23 (0+23) -> GIO4/BB8_13
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad9
#setup actual GPIO
echo 23 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio23/direction

####################################################################################################
##  BeagleBone P8, pin 14 -> GPIO0_26 (0+26) -> GIO5/BB8_14
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad10
#setup actual GPIO
echo 26 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio26/direction

####################################################################################################
##  BeagleBone P8, pin 15 -> GPIO1_15 (32+15) -> GIO6/BB8_15
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad15
#setup actual GPIO
echo 47 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio47/direction

####################################################################################################
##  BeagleBone P8, pin 16 -> GPIO1_14 (32+14) -> GIO7/BB8_16
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad14
#setup actual GPIO
echo 46 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio46/direction

####################################################################################################
##  BeagleBone P8, pin 17 -> GPIO0_27 (0+27) -> GIO8/BB8_17 -> USBINSERTED? High if so, low if not.
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad11
#setup actual GPIO
echo 27 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio27/direction
echo 0 > /sys/class/gpio/gpio27/value

####################################################################################################
##  BeagleBone P8, pin 18 -> GPIO2_1 (64+1) -> GIO9/BB8_18
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_clk
#setup actual GPIO
echo 65 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio65/direction

####################################################################################################
##  BeagleBone P8, pin 19 -> GPIO0_22 (0+22) -> GIO10/BB8_19
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_ad8
#setup actual GPIO
echo 22 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio22/direction

####################################################################################################
##  BeagleBone P8, pin 20 -> GPIO1_31 (32+31) -> GIO11/BB8_20
#
#setup mode
echo f > /sys/kernel/debug/omap_mux/gpmc_csn2
#setup actual GPIO
echo 63 > /sys/class/gpio/export
echo "low" > /sys/class/gpio/gpio63/direction


echo "Done with pin settings" > /dev/kmsg
exit

