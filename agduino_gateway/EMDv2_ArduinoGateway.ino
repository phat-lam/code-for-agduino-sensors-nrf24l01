/*
 * Simple code for the nRF_gateway ID 00 in mushroom growing room. 
 * Collects data from sensor nodes with RF wireless transceiver nRF24L01+. 
 * Sends data to BB by UART interface.
 * 
 * Atmega328p-au MCU with arduino bootloader.
 * Data transceiver: rfic nRF24L01+ with SPI serial interface, frequency: 2.4MHz, baud-rate: 250 kbps, channel: 0x60 hex (96 int).
 * No Sensors
 * 
 * The circuit:
 * D8: nRF CE pin.
 * D9: nRF CS pin.
 * D11: nRF MOSI pin.
 * D12: nRF MISO pin.
 * D13: nRF SCK pin.
 * TXD: BB_UART1 RXD pin.
 * RXD: BB_UART1 TXD pin.
 * 
 * Note: connect a capacitor 10uF between Vcc and Gnd of nRF module.
 * 
 * Created 18 Mar 2017 by AGCT.
 */
// Comes with IDE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
// SPI lib for Arduino
#include <SPI.h>
// Timer lib
#include "SimpleTimer.h"
// nRF lib
#include "RF24.h"

SimpleTimer timer;
/*-----( Declare Constants and Pin Numbers )------*/
// WSN ID. "000" shows this node in Network 0 and has ID 00.
char node_id[4] = "000";
// nRF24L01+: CE pin and CS pin
RF24 radio(8,9);
// Radio pipe addresses of node 01 to node 06
const uint64_t rx_pipes1 = 0xB3B4B5B6F1LL;
const uint64_t rx_pipes2 = 0xB3B4B5B6CDLL;
const uint64_t rx_pipes3 = 0xB3B4B5B6A3LL;
const uint64_t rx_pipes4 = 0xB3B4B5B60FLL;
const uint64_t rx_pipes5 = 0xB3B4B5B605LL;
const uint64_t rx_pipes6 = 0xB3B4B5B678LL;
/*-----( Declare Objects and Variables )----------*/                                                   
// Received data
char str_data[60];
/*----- SETUP: RUNS ONCE -------------------------*/
void setup() 
{
  // Initialize the serial communications. Set speed to 9600 kbps.
  Serial.begin(9600);
  // Initialize the nRF module.
  radio.begin();
  // Set channel
  radio.setChannel(0x60);
  // Set transmission power
  radio.setPALevel(RF24_PA_MAX);
  // Set baud-rate
  radio.setDataRate(RF24_250KBPS);
  // Ensure autoACK is enabled
  radio.setAutoAck(1);
  // Optionally, increase the delay between retries & # of retries
  radio.setRetries(2,15);
  radio.setPayloadSize(32);

  // Open the reading pipes on radio modules
  radio.openReadingPipe(1, rx_pipes1);
  radio.openReadingPipe(2, rx_pipes2);
  radio.openReadingPipe(3, rx_pipes3); 
  radio.openReadingPipe(4, rx_pipes4);
  radio.openReadingPipe(5, rx_pipes5);
  radio.openReadingPipe(6, rx_pipes6);
  
  radio.powerUp() ;
  // Transmission mode
  radio.stopListening();
  // Receiving mode
  radio.startListening();
}
/*----- End SETUP ---------------------------------*/
/*----- LOOP: RUNS CONSTANTLY ---------------------*/
void loop() 
{ 
  while (radio.available())
  {
    // Receiving data
    radio.read(&str_data, sizeof(str_data));
    // Send data to BB with UART communication.
    // End with "\n\r"
    Serial.print(str_data);
    Serial.print("\n\r");
  } 
}
/*----- End LOOP ----------------------------------*/
/*-----( Declare User-written Functions )----------*/
int getState(unsigned short pin)
{
  boolean state = digitalRead(pin);
  return state == true ? 0 : 1;
}
void sendCallback(unsigned short callback)
{
  // First, stop listening so the nRF module can talk
  radio.stopListening();
  // Send the final one back.
  radio.write( &callback, sizeof(unsigned short) );
  // Now, resume listening so the nRF module get the next packets.
  radio.startListening();
}
/*-----( THE END )------------------------------------*/
