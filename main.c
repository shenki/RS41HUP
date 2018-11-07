// STM32F100 and SI4032 RTTY transmitter
// released under GPL v.2 by anonymous developer
// enjoy and have a nice day
// ver 1.5a
#include <stm32f10x_gpio.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_usart.h>
#include <stm32f10x_adc.h>
#include <stm32f10x_rcc.h>
#include "stdlib.h"
#include <stdio.h>
#include <string.h>
#include <misc.h>
#include "f_rtty.h"
#include "init.h"
#include "config.h"
#include "radio.h"
#include "ublox.h"
#include "delay.h"
#include "aprs.h"
#include "util.h"
#include "mfsk.h"
#include "horus_l2.h"

// If enabled, print out binary packets as hex before and after coding.
//#define MFSKDEBUG 1

// IO Pins Definitions. The state of these pins are initilised in init.c
#define GREEN  GPIO_Pin_7 // Inverted
#define RED  GPIO_Pin_8 // Non-Inverted (?)


// Transmit Modulation Switching
#define STARTUP 0
#define RTTY 1
#define FSK_4 2
#define FSK_2 3
volatile int current_mode = STARTUP;

// Telemetry Data to Transmit - used in RTTY & MFSK packet generation functions.
unsigned int send_count;        //frame counter
int voltage;
int8_t si4032_temperature;
GPSEntry gpsData;

char callsign[15] = {CALLSIGN};
char status[2] = {'N'};
uint16_t CRC_rtty = 0x12ab;  //checksum (dummy initial value)
char buf_rtty[200];
char buf_mfsk[200];

// Volatile Variables, used within interrupts.
volatile int adc_bottom = 2000;
volatile char flaga = 0; // GPS Status Flags
volatile int led_enabled = 1; // Flag to disable LEDs at altitude.

volatile unsigned char pun = 0;
volatile unsigned int cun = 10;
volatile unsigned char tx_on = 0;
volatile unsigned int tx_on_delay;
volatile unsigned char tx_enable = 0;
rttyStates send_rtty_status = rttyZero;
volatile char *tx_buffer;
volatile uint16_t current_mfsk_byte = 0;
volatile uint16_t packet_length = 0;
volatile uint16_t button_pressed = 0;
volatile uint8_t disable_armed = 0;


// Binary Packet Format
// Note that we need to pack this to 1-byte alignment, hence the #pragma flags below
// Refer: https://gcc.gnu.org/onlinedocs/gcc-4.4.4/gcc/Structure_002dPacking-Pragmas.html
#pragma pack(push,1) 
struct TBinaryPacket
{
uint8_t   PayloadID;
uint16_t  Counter;
uint8_t   Hours;
uint8_t   Minutes;
uint8_t   Seconds;
float   Latitude;
float   Longitude;
uint16_t    Altitude;
uint8_t   Speed; // Speed in Knots (1-255 knots)
uint8_t   Sats;
int8_t   Temp; // Si4032 temperature, as a signed value (-128 to +128, though sensor limited to -64 to +64 deg C)
uint8_t   BattVoltage; // 0 = 0v, 255 = 5.0V, linear steps in-between.
uint16_t Checksum; // CRC16-CCITT Checksum.
};  //  __attribute__ ((packed)); // Doesn't work?
#pragma pack(pop)


// Function Definitions
void collect_telemetry_data();
void send_rtty_packet();
void send_mfsk_packet();
uint16_t gps_CRC16_checksum (char *string);


/**
 * GPS data processing
 */
void USART1_IRQHandler(void) {
  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
    ublox_handle_incoming_byte((uint8_t) USART_ReceiveData(USART1));
      }else if (USART_GetITStatus(USART1, USART_IT_ORE) != RESET) {
    USART_ReceiveData(USART1);
  } else {
    USART_ReceiveData(USART1);
  }
}

//
// Symbol Timing Interrupt
// In here symbol transmission occurs.
//

void TIM2_IRQHandler(void) {
  static int mfsk_symbol = 0;

  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    if (ALLOW_DISABLE_BY_BUTTON){
      if (ADCVal[1] > adc_bottom){
        button_pressed++;
        if (button_pressed > (BAUD_RATE / 3)){
          disable_armed = 1;
          GPIO_SetBits(GPIOB, RED);
          //GPIO_SetBits(GPIOB, GREEN);
        }
      } else {
        if (disable_armed){
          GPIO_SetBits(GPIOA, GPIO_Pin_12);
        }
        button_pressed = 0;
      }

      if (button_pressed == 0) {
        adc_bottom = ADCVal[1] * 1.1; // dynamical reference for power down level
      }
    }
      
    if (tx_on) {
      // RTTY Symbol selection logic.
      if(current_mode == RTTY){
        send_rtty_status = send_rtty((char *) tx_buffer);

        if (!disable_armed){
          if (send_rtty_status == rttyEnd) {
            if (led_enabled) GPIO_SetBits(GPIOB, RED);
            if (*(++tx_buffer) == 0) {
              tx_on = 0;
              // Reset the TX Delay counter, which is decremented at the symbol rate.
              tx_on_delay = TX_DELAY / (1000/BAUD_RATE);
              tx_enable = 0;
              //radio_disable_tx(); // Don't turn off the transmitter!
            }
          } else if (send_rtty_status == rttyOne) {
            radio_rw_register(0x73, RTTY_DEVIATION, 1);
            if (led_enabled) GPIO_SetBits(GPIOB, RED);
          } else if (send_rtty_status == rttyZero) {
            radio_rw_register(0x73, 0x00, 1);
            if (led_enabled) GPIO_ResetBits(GPIOB, RED);
          }
        }
      } else if (current_mode == FSK_4) {
        // 4FSK Symbol Selection Logic
        // Get Symbol to transmit.  
        mfsk_symbol = send_mfsk(tx_buffer[current_mfsk_byte]);

        if(mfsk_symbol == -1){
          // Reached the end of the current character, increment the current-byte pointer.
          if (current_mfsk_byte++ == packet_length) {
              // End of the packet. Reset Counters and stop modulation.
              radio_rw_register(0x73, 0x03, 1); // Idle at Symbol 3
              current_mfsk_byte = 0;
              tx_on = 0;
              // Reset the TX Delay counter, which is decremented at the symbol rate.
              tx_on_delay = TX_DELAY / (1000/BAUD_RATE);
              tx_enable = 0;
              
          } else {
            // We've now advanced to the next byte, grab the first symbol from it.
            mfsk_symbol = send_mfsk(tx_buffer[current_mfsk_byte]);
          }
        }
        // Set the symbol!
        if(mfsk_symbol != -1){
          radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
        }
        

      } else if (current_mode == FSK_2) {
        // 2FSK Symbol Selection Logic
        // Get Symbol to transmit.
        mfsk_symbol = send_2fsk(tx_buffer[current_mfsk_byte]);

        if(mfsk_symbol == -1){
          // Reached the end of the current character, increment the current-byte pointer.
          if (current_mfsk_byte++ == packet_length) {
              // End of the packet. Reset Counters and stop modulation.
              radio_rw_register(0x73, 0x00, 1); // Idle at Symbol 0.
              current_mfsk_byte = 0;
              tx_on = 0;
              // Reset the TX Delay counter, which is decremented at the symbol rate.
              tx_on_delay = TX_DELAY / (1000/BAUD_RATE);
              tx_enable = 0;
              
          } else {
            // We've now advanced to the next byte, grab the first symbol from it.
            mfsk_symbol = send_2fsk(tx_buffer[current_mfsk_byte]);
          }
        }
        // Set the symbol!
        if(mfsk_symbol != -1){
          radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
        }
      } else{
        // Ummmm. 
      }
    }else{
      // TX is off 
      // If we are don't have RTTY enabled, and if we have MFSK_CONTINUOUS set,
      // transmit continuous MFSK symbols.
      #ifndef RTTY_ENABLED
        #ifdef MFSK_CONTINUOUS
          mfsk_symbol = (mfsk_symbol+1)%4;
          radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
        #endif
      #endif
    }

    // Delay between Transmissions Logic.
    // tx_on_delay is set at the end of a RTTY transmission above, and counts down
    // at the interrupt rate. When it hits zero, we set tx_enable to 1, which allows
    // the main loop to continue.
    if (!tx_on && --tx_on_delay == 0) {
      tx_enable = 1;
      tx_on_delay--;
    }

    // Green LED Blinking Logic
    if (--cun == 0) {
      if (pun) {
        // Clear Green LED.
        if (led_enabled) GPIO_SetBits(GPIOB, GREEN);
        pun = 0;
      } else {
        // If we have GPS lock, set LED
        if (flaga & 0x80) {
          if (led_enabled) GPIO_ResetBits(GPIOB, GREEN);
        }
        pun = 1;
      }
      // Wait 200 symbols.
      cun = 200;
    }
  }
}

int main(void) {
#ifdef DEBUG
  debug();
#endif
  RCC_Conf();
  NVIC_Conf();
  init_port();

  init_timer(BAUD_RATE);

  delay_init();
  ublox_init();

  GPIO_SetBits(GPIOB, RED);
  // NOTE - Green LED is inverted. (Reset to activate, Set to deactivate)
  GPIO_SetBits(GPIOB, GREEN);
  USART_SendData(USART3, 0xc);

  radio_soft_reset();
  // setting RTTY TX frequency
  radio_set_tx_frequency(TRANSMIT_FREQUENCY);

  // setting TX power
  radio_rw_register(0x6D, 00 | (TX_POWER & 0x0007), 1);

  // initial RTTY modulation
  radio_rw_register(0x71, 0x00, 1);

  // Temperature Value Offset
  radio_rw_register(0x13, 0x00, 1); // Was 0xF0 (?)

  // Temperature Sensor Calibration
  radio_rw_register(0x12, 0x20, 1);

  // ADC configuration
  radio_rw_register(0x0f, 0x80, 1);
  tx_buffer = buf_rtty;
  tx_on = 0;
  tx_enable = 1;

  // Why do we have to do this again?
  spi_init();
  radio_set_tx_frequency(TRANSMIT_FREQUENCY);   
  radio_rw_register(0x71, 0x00, 1);
  init_timer(BAUD_RATE);

  radio_enable_tx();


  while (1) {
    // Don't do anything until the previous transmission has finished.
    if (tx_on == 0 && tx_enable) {
        if (current_mode == STARTUP){
          // Grab telemetry information.
          collect_telemetry_data();

          // Now Startup a RTTY Transmission
          current_mode = RTTY;
          // If enabled, transmit a RTTY packet.
          #ifdef RTTY_ENABLED
            send_rtty_packet();
          #endif

        } else if (current_mode == RTTY){
          // We've just transmitted a RTTY packet, now configure for 4FSK.
          current_mode = FSK_4;
          #ifdef MFSK_ENABLED
            send_mfsk_packet();
          #endif
        } else {
          // We've finished the 4FSK transmission, grab new data.
          current_mode = STARTUP;
        }
    } else {
      NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
      __WFI();
    }
  }
}


void collect_telemetry_data() {
  // Assemble and proccess the telemetry data we need to construct our RTTY and MFSK packets.
  send_count++;
  si4032_temperature = radio_read_temperature();
  voltage = ADCVal[0] * 600 / 4096;
  ublox_get_last_data(&gpsData);

  if (gpsData.fix >= 3) {
      flaga |= 0x80;
      // Disable LEDs if altitude is > 1000m. (Power saving? Maybe?)
      if ((gpsData.alt_raw / 1000) > 1000){
        led_enabled = 0;
      } else {
        led_enabled = 1;
      }
  } else {
      // No GPS fix.
      flaga &= ~0x80;
      led_enabled = 1; // Enable LEDs when there is no GPS fix (i.e. during startup)

      // Null out lat / lon data to avoid spamming invalid positions all over the map.
      gpsData.lat_raw = 0;
      gpsData.lon_raw = 0;
  }
}


void send_rtty_packet() {
  // Write a RTTY packet into the tx buffer, and start transmission.

  // Convert raw lat/lon values into degrees and decimal degree values.
  uint8_t lat_d = (uint8_t) abs(gpsData.lat_raw / 10000000);
  uint32_t lat_fl = (uint32_t) abs(abs(gpsData.lat_raw) - lat_d * 10000000) / 1000;
  uint8_t lon_d = (uint8_t) abs(gpsData.lon_raw / 10000000);
  uint32_t lon_fl = (uint32_t) abs(abs(gpsData.lon_raw) - lon_d * 10000000) / 1000;

  uint8_t speed_kph = (uint8_t)((float)gpsData.speed_raw*0.0036);
 
  // Produce a RTTY Sentence (Compatible with the existing HORUS RTTY payloads)
  
  sprintf(buf_rtty, "\n\n\n\n$$$$$%s,%d,%02u:%02u:%02u,%s%d.%04ld,%s%d.%04ld,%ld,%d,%d,%d,%d",
        callsign,
        send_count,
        gpsData.hours, gpsData.minutes, gpsData.seconds,
        gpsData.lat_raw < 0 ? "S" : "N", lat_d, lat_fl,
        gpsData.lon_raw < 0 ? "W" : "E", lon_d, lon_fl,
        (gpsData.alt_raw / 1000),
        speed_kph,
        gpsData.sats_raw,
        voltage*10,
        si4032_temperature
        );
  
  // Calculate and append CRC16 checksum to end of sentence.
  CRC_rtty = string_CRC16_checksum(buf_rtty + 9);
  sprintf(buf_rtty, "%s*%04X\n", buf_rtty, CRC_rtty & 0xffff);

  // Point the TX buffer at the temporary RTTY packet buffer.
  tx_buffer = buf_rtty;

  // Enable the radio, and set the tx_on flag to 1.
  start_bits = RTTY_PRE_START_BITS;
  radio_enable_tx();
  tx_on = 1;
  // From here the timer interrupt handles things.
}


void send_mfsk_packet(){
  // Generate a MFSK Binary Packet
  //packet_length = mfsk_test_bits(buf_mfsk);

  // Sanitise and convert some of the data.
  if(gpsData.alt_raw < 0){
    gpsData.alt_raw = 0;
  }
  float float_lat = (float)gpsData.lat_raw / 10000000.0;
  float float_lon = (float)gpsData.lon_raw / 10000000.0;

  uint8_t volts_scaled = (uint8_t)(255*(float)voltage/500.0);

  // Assemble a binary packet
  struct TBinaryPacket BinaryPacket;
  BinaryPacket.PayloadID = BINARY_PAYLOAD_ID%256;
  BinaryPacket.Counter = send_count;
  BinaryPacket.Hours = gpsData.hours;
  BinaryPacket.Minutes = gpsData.minutes;
  BinaryPacket.Seconds = gpsData.seconds;
  BinaryPacket.Latitude = float_lat;
  BinaryPacket.Longitude = float_lon;
  BinaryPacket.Altitude = (uint16_t)(gpsData.alt_raw/1000);
  BinaryPacket.Speed = (uint8_t)((float)gpsData.speed_raw*0.0036);
  BinaryPacket.BattVoltage = volts_scaled;
  BinaryPacket.Sats = gpsData.sats_raw;
  BinaryPacket.Temp = si4032_temperature;

  BinaryPacket.Checksum = (uint16_t)array_CRC16_checksum((char*)&BinaryPacket,sizeof(BinaryPacket)-2);

  #ifdef MFSKDEBUG
  // Write BinaryPacket into the RTTY transmit buffer as hex
  memcpy(buf_mfsk,&BinaryPacket,sizeof(struct TBinaryPacket));
  sprintf(buf_rtty,"$$$$");
  print_hex(buf_mfsk, sizeof(struct TBinaryPacket), buf_rtty+4);

  //Configure for transmit
  tx_buffer = buf_rtty;
  // Enable the radio, and set the tx_on flag to 1.
  start_bits = RTTY_PRE_START_BITS;
  radio_enable_tx();
  current_mode = RTTY;
  tx_on = 1;

  // Wait until transmit has finished.
  while(tx_on){
    NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
    __WFI();
  }
  current_mode = FSK_4;
  #endif


  // Write Preamble characters into mfsk buffer.
  sprintf(buf_mfsk, "\x1b\x1b\x1b\x1b");
  // Encode the packet, and write into the mfsk buffer.
  int coded_len = horus_l2_encode_tx_packet((unsigned char*)buf_mfsk+4,(unsigned char*)&BinaryPacket,sizeof(BinaryPacket));


  #ifdef MFSKDEBUG
  // Write the coded packet into the RTTY transmit buffer as hex
  sprintf(buf_rtty,"$$$$");
  print_hex(buf_mfsk, coded_len+4, buf_rtty+4);

  //Configure for transmit
  tx_buffer = buf_rtty;
  // Enable the radio, and set the tx_on flag to 1.
  start_bits = RTTY_PRE_START_BITS;
  radio_enable_tx();
  current_mode = RTTY;
  tx_on = 1;

  // Wait until transmit has finished.
  while(tx_on){
    NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
    __WFI();
  }
  current_mode = FSK_4;
  // Wait until tx_enable
  while(tx_enable == 0){
    NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
    __WFI();
  }
  _delay_ms(1000);
  #endif

  // Data to transmit is the coded packet length, plus the 4-byte preamble.
  packet_length = coded_len+4;
  tx_buffer = buf_mfsk;

  // Enable the radio, and set the tx_on flag to 1.
  radio_enable_tx();
  tx_on = 1;
}




#ifdef  DEBUG
void assert_failed(uint8_t* file, uint32_t line)
{
    while (1);
}
#endif