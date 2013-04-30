/*
 * 
 * Description: This Windows application will read in .txt files, located on a USB drive, that contain GPS locations, 
 *              and will parse them into a static map provided by Google Maps' web server.
 *              
 * Date: 4/25/2013
 * 
 * Author: Ben Paolillo 
 * 
 * Note: This application was made solely for Boston University's ECE Senior Design Team ReCycle. It should not be used
 *       for any other purpose.
 *
 * Requirements:
 * 	This application will only run on Windows (Vista, 7, 8).
 *	This application requires an active internet connection.
 * 
 */

How the application works:

This application will only work with a USB drive that has been used with the ReCycle system prior to launching the application.
There are specific directories and hidden files that need to be in the correct place on the user's USB drive in order to have
the application function properly, if at all. This application assumes that these directories are in place on the USB drive, 
and the included source code is provided as-is.

This application will parse trip text files located on the USB drive. If the text files are valid and located correctly
on the drive, then the application will build a URL string and subsequently pass that string to Google Maps' web server to pull
a static Google Maps image with a route clearly plotted on top of the static map.

If there are too many coordinates located within the trip text file (as is often the case), then the application will first,
remove duplicate coordinates (as these appear often with the GPS module we are using), and second, the application will average
pairs of GPS location coordinates until the length of the URL string is under 2000 characters. This is the maximum length of a
URL string supported by modern web browsers such as Google Chrome, Mozilla FireFox, Internet Explorer, etc. Note, however, that
this does not severely reduce the accuracy of the route plotted, but it does reduce it by a negligible amount.

The pre-compiled executable is named "USB_Google_Maps.exe" and is located in: USB_Google_Maps/bin/Debug.
