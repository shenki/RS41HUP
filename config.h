//
// Created by SQ5RWU on 2016-12-24.
//

#ifndef RS41HUP_CONFIG_H
#define RS41HUP_CONFIG_H

#ifdef USE_EXTERNAL_CONFIG
#include "config_external.h"
#else


//************GLOBAL Settings*****************
#define TRANSMIT_FREQUENCY  434.650f //Mhz middle frequency
#define BAUD_RATE  100 // RTTY & MFSK Baud rate

// Modulation Settings - Comment out a line below to enable/disable a modulation.
#define RTTY_ENABLED 1
#define MFSK_ENABLED 1

// TX Power
#define TX_POWER  5 // PWR 0...7 0- MIN ... 7 - MAX
// TODO: Check the below levels are correct. Apparnetly 7 - 50mW at 434 MHz.
// 0 --> -1dBm
// 1 --> 2dBm
// 2 --> 5dBm
// 3 --> 8dBm
// 4 --> 11dBm
// 5 --> 14dBm - DEFAULT
// 6 --> 17dBm
// 7 --> 20dBm

// Delay *between* transmitted packets
#define TX_DELAY  1000

//*************RTTY SETTINGS******************
#define CALLSIGN "N0CALL" // put your RTTY callsign here, max. 15 characters
#define RTTY_DEVIATION 0x3	// RTTY shift = RTTY_DEVIATION x 270Hz
#define RTTY_7BIT   1 // if 0 --> 5 bits
#define RTTY_USE_2_STOP_BITS   1


//************MFSK Binary Settings************
#define BINARY_PAYLOAD_ID 0x01 // Payload ID for use in Binary Packets


//***********Other Settings ******************
// Switch sonde ON/OFF via Button
// If this is a flight you might prevent sonde from powered off by button
#define ALLOW_DISABLE_BY_BUTTON 0



//************* APRS Settings *************************
// Note - APRS functionality currently disabled, and will eventually be removed.
//
#define APRS_CALLSIGN "N0CALL" // put your APRS callsign here, 6 characters. If your callsign is shorter add spaces
#define APRS_SSID 'B' // put your APRS SSID here
// 0 --> Your primary station usually fixed and message capable
// 1 --> generic additional station, digi, mobile, wx, etc.
// 2 --> generic additional station, digi, mobile, wx, etc.
// 3 --> generic additional station, digi, mobile, wx, etc.
// 4 --> generic additional station, digi, mobile, wx, etc.
// 5 --> Other network sources (Dstar, Iphones, Blackberry's etc)
// 6 --> Special activity, Satellite ops, camping or 6 meters, etc.
// 7 --> walkie talkies, HT's or other human portable
// 8 --> boats, sailboats, RV's or second main mobile
// 9 --> Primary Mobile (usually message capable)
// A --> internet, Igates, echolink, winlink, AVRS, APRN, etc.
// B --> balloons, aircraft, spacecraft, etc.
// C --> APRStt, DTMF, RFID, devices, one-way trackers*, etc.
// D --> Weather stations
// E --> Truckers or generally full time drivers
// F --> generic additional station, digi, mobile, wx, etc.

#define APRS_COMMENT " Hello from the sky!"
#define APRS_FREQUENCY  439.100f //Mhz middle frequency - Australian 70cm APRS Frequency

#endif

#endif //RS41HUP_CONFIG_H
