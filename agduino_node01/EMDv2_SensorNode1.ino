/*
 * Simple code for the nRF_gateway ID 00 in mushroom growing room. 
 * Collects data from sensor nodes with RF wireless transceiver nRF24L01+. 
 * Sends data to BB_gateway by UART interface.
 * 
 * Atmega328p-au MCU with arduino bootloader.
 * Data transceiver: rfic nRF24L01+ with SPI serial interface, frequency: 2.4MHz, baud-rate: 250 kbps, channel: 0x60 hex (96 int).
 * Sensors: temperature & humidity SHT11 (soft-I2C interface), light intensity BH1750(I2C interface).
 * 
 * The circuit:
 * D2: SHT clock pin.
 * D6: SHT data pin.
 * D8: nRF CE pin.
 * D9: nRF CS pin.
 * D11: nRF MOSI pin.
 * D12: nRF MISO pin.
 * D13: nRF SCK pin.
 * A4: BH1750 SDA pin.
 * A5: BH1750 SCL pin.
 * 
 * Note: connect a capacitor 10uF between Vcc and Gnd of nRF module.
 * 
 * Created 18 Mar 2017 by AGCT.
 */
// Comes with IDE
// I2C lib for light sensor
#include <Wire.h>
// SPI lib for nRF module
#include <SPI.h>
#include <stdlib.h>
#include <stdio.h>
// nRF lib
#include "RF24.h"
// Timer lib
#include "SimpleTimer.h"
// SHT lib for T&RH sensor
#include "SHT1x.h"

SimpleTimer timer;
/*-----( Declare Constants and Pin Numbers )------*/
// WSN ID. "001" shows this node in Network 0 and has ID 01.
char node_id[] = "001";
// nRF24L01+: CE pin and CS pin
RF24 radio(8,9);
// Radio pipe addresses of node 01
const uint64_t tx_pipes1 = 0xB3B4B5B6F1LL;
// SHT sensor
#define sht_dataPin 6
#define sht_clockPin 2
// Timer period
int timer_period = 10000;
/*-----( Declare Objects and Variables )----------*/
// Received data
char str_data[60];
// SHT sensor
SHT1x sht1x(sht_dataPin, sht_clockPin);
// BH1750 sensor
int BH1750address = 0x23;
byte buff[2];
// No CO2 sensor
int ppmCO2 = 0;
// No soil sensor
float soil_dielctric = 0, soil_temp = 0;
// Data(t, h, li, ppmCO2, soil_dielctric, soil_temp) in char-array
static char dtostrfbuffer1[4];
static char dtostrfbuffer2[3];
static char dtostrfbuffer3[6];
static char dtostrfbuffer4[5];
static char dtostrfbuffer5[5];
static char dtostrfbuffer6[4];
/*----- SETUP: RUNS ONCE -------------------------*/
void setup() 
{
  // Initialize the serial communications. Set speed to 9600 kbps.
  Serial.begin(9600); 
  // Initialize the I2C communication.
  Wire.begin();
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
  // Open the writing pipe on radio module
  radio.openWritingPipe(tx_pipes1); 
  
  radio.powerUp();
  // Transmission mode
  radio.startListening();
  // Receiving mode
  radio.stopListening();
  
  // Run rf_TransmitDataFunc every 'period' 10 seconds.
  timer.setInterval(timer_period, rf_TransmitDataFunc);
}
//----------------------------LOOP()------------------------------------------------//
void loop() 
{ 
  //timer.run();
  int int_data = 100;
  radio.stopListening();                                                            // Stop listening
  radio.write(&int_data,strlen(int_data));
  radio.startListening();                                                           // Start listening
  delay(1000);
}
/*-----( Declare User-written Functions )----------*/
int getState(unsigned short pin)
{
  boolean state = digitalRead(pin);
  return state == true ? 0 : 1;
}
void sendCallback(unsigned short callback)
{
  // First, stop listening so nRF module can talk
  radio.stopListening();
  // Send the final one back.
  radio.write( &callback, sizeof(unsigned short) );
  // Now, resume listening so nRF module get the next packets.
  radio.startListening();
}   
void rf_TransmitDataFunc()  
{
  // Read humidity
  float h = sht1x.readHumidity();
  // Read temperature as Celsius (the default)
  float t = sht1x.readTemperatureC();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = sht1x.readTemperatureF();
  // Read light sensor
  int i;
  uint16_t li=0;
  BH1750_Init(BH1750address);
  if(2==BH1750_Read(BH1750address))
  {
    li=((buff[0]<<8)|buff[1])/1.2;
  } else li = 65535;
  // No CO2 sensor
  ppmCO2 = 0;
  // No soil sensor
  soil_dielctric = 0;
  soil_temp = 0;
  
  // Convert all data into char-array
  dtostrf((int)(t*10), 3, 0, dtostrfbuffer1);
  dtostrf((int)h, 2, 0, dtostrfbuffer2);
  dtostrf(li, 5, 0, dtostrfbuffer3);
  dtostrf(ppmCO2, 4, 0, dtostrfbuffer4); 
  dtostrf(soil_dielctric * 100, 4, 0, dtostrfbuffer5);
  dtostrf(soil_temp * 10, 3, 0, dtostrfbuffer6);

  // Transmit data to serial port for testing
  // End with "\n\r"
  Serial.print("S"); Serial.print(node_id);
  Serial.print("T"); Serial.print(dtostrfbuffer1);
  Serial.print("H"); Serial.print(dtostrfbuffer2);
  Serial.print("L"); Serial.print(dtostrfbuffer3);
  Serial.print("C"); Serial.print(dtostrfbuffer4);
  Serial.print("D"); Serial.print(dtostrfbuffer5);
  Serial.print("P"); Serial.print(dtostrfbuffer6);
  Serial.print("E\n\r");
  
  // Connect all char-array into one
  sprintf(str_data,"S%sT%sH%sL%sC%sD%sP%sE", node_id, dtostrfbuffer1, dtostrfbuffer2, dtostrfbuffer3, dtostrfbuffer4, dtostrfbuffer5, dtostrfbuffer6);  
  // Transmission mode
  radio.stopListening();
  // Transmit data
  radio.write(&str_data,strlen(str_data));
  // Receiving mode
  radio.startListening();
  
  // Clear all variables
  t = 0;
  h = 0;
  li = 0;
  ppmCO2 = 0;
  soil_dielctric = 0;
  soil_temp = 0;
}
// Read light sensor
int BH1750_Read(int address) 
{
  int i=0;
  Wire.beginTransmission(address);
  Wire.requestFrom(address, 2);
  while(Wire.available()) 
  {
    // receive one byte
    buff[i] = Wire.read();
    i++;
  }
  Wire.endTransmission();  
  return i;
}
// Initialize light sensor
void BH1750_Init(int address) 
{
  Wire.beginTransmission(address);
  //1lx reolution 120ms
  Wire.write(0x10);
  Wire.endTransmission();
}
/*-----( THE END )------------------------------------*/
