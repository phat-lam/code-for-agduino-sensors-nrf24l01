/*
 * Simple code for the nRF_gateway ID 04 in mushroom growing room. 
 * Collects data from sensors and Sends data to BB_gateway by RF wireless transceiver nRF24L01+.
 * 
 * Atmega328p-au MCU with arduino bootloader.
 * Data transceiver: rfic nRF24L01+ with SPI serial interface, frequency: 2.4MHz, baud-rate: 250 kbps, channel: 0x60 hex (96 int).
 * Sensors: temperature & humidity DHT11 (soft-I2C interface), light intensity BH1750(I2C interface), 5TM soil moisture and temperature sensor.
 * 
 * The circuit:
 * D4: DHT data pin.
 * D5: 5TM data pin.
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
#include <Wire.h>
#include <SPI.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// nRF lib
#include "nRF24L01.h"
#include "RF24.h"
// DHT lib for T&RH sensor
#include "DHT.h"
// SDI-12 lib for soil sensor
#include "SDI12.h"
// Timer lib
#include "SimpleTimer.h"

SimpleTimer timer;
/*-----( Declare Constants and Pin Numbers )------*/
// WSN ID. "004" shows this node in Network 0 and has ID 04.
char node_id[] = "004";
// nRF24L01+: CE pin and CS pin
RF24 radio(8,9);
// Radio pipe addresses of node 04
const uint64_t tx_pipes4 = 0xB3B4B5B60FLL;
// DHT sensor (AM2301)
#define DHTPIN  4
#define DHTTYPE DHT21
// 5TM data pin
#define DATAPIN 5
// Timer period
int timer_period = 10000;
/*-----( Declare Objects and Variables )----------*/
// Transmitted data
char str_data[60];
// DHT sensor
DHT dht(DHTPIN, DHTTYPE);
// No CO2 sensor
int ppmCO2 = 0;
// BH1750 sensor
// Setting I2C address
int BH1750address = 0x23;
byte buff[2];
// 5TM sensor
SDI12 mySDI12(DATAPIN);
float soil_dielctric = 0, soil_temp = 0;
// keeps track of active addresses
// each bit represents an address:
// 1 is active (taken), 0 is inactive (available)
// setTaken('A') will set the proper bit for sensor 'A'
// set 
byte addressRegister[8] = { 
  0B00000000, 
  0B00000000, 
  0B00000000, 
  0B00000000, 
  0B00000000, 
  0B00000000, 
  0B00000000, 
  0B00000000 
};
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
  // Initialize SDI-12 communication.
  mySDI12.begin();
  // Initialize I2C communication.
  Wire.begin();
  // Initialize nRF module.
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
  radio.setRetries(4,15);
  radio.setPayloadSize(32);
  // Open the writing pipe on radio module
  radio.openWritingPipe(tx_pipes4); 
  
  radio.powerUp();
  // Transmission mode
  radio.startListening(); 
  // Receiving mode
  radio.stopListening();
  
  delay(4000);
  // Run rf_TransmitDataFunc every 'period' 10 seconds.
  timer.setInterval(timer_period, rf_TransmitDataFunc);
}
/*----- End SETUP ---------------------------------*/
/*----- LOOP: RUNS CONSTANTLY ---------------------*/
void loop() 
{ 
  timer.run();
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
  // First, stop listening so nRF module can talk
  radio.stopListening();                                                            // First, stop listening so we can talk
  // Send the final one back.
  radio.write( &callback, sizeof(unsigned short) );                                 // Send the final one back.
  // Now, resume listening so nRF module get the next packets.
  radio.startListening();                                                           // Now, resume listening so we catch the next packets.
}
void rf_TransmitDataFunc()  
{
  // Read humidity
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) 
  {
    //Serial.println("Failed to read DHT sensor!");
    return;
  }
  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)                                          
  float hic = dht.computeHeatIndex(t, h, false);
  // Read light sensor
  uint16_t li=0;
  BH1750_Init(BH1750address);
  if(2==BH1750_Read(BH1750address))
  {
    li=((buff[0]<<8)|buff[1])/1.2;
  } else li = 65535;
  // No CO2 sensor
  ppmCO2 = 0;
  // Read soil sensor
  // send command '0I!': print infor of 5tm sensor, address '0'
  //printInfo('0');
  // send command '0M!': read data of 5tm sensor, address '0'             
  takeMeasurement('0');
  
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
  radio.write(&str_data,strlen(str_data) );
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
// Read 5TM soil sensor
void takeMeasurement(char i)
{
  String command = ""; 
  command += i;
  // SDI-12 measurement command format  [address]['M'][!]
  command += "M!";
  mySDI12.sendCommand(command);
  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of measurments available, 0-9]
  while(!mySDI12.available()>5);
  delay(100); 
  
  mySDI12.read(); //consume address
  
  // find out how long we have to wait (in seconds).
  int wait = 0; 
  wait += 100 * mySDI12.read()-'0';
  wait += 10 * mySDI12.read()-'0';
  wait += 1 * mySDI12.read()-'0';
  
  // ignore # measurements, for this simple examlpe
  mySDI12.read();
  // ignore carriage return
  mySDI12.read();
  // ignore line feed
  mySDI12.read();
  
  long timerStart = millis(); 
  while((millis() - timerStart) > (1000 * wait))
  {
    //sensor can interrupt us to let us know it is done early
    if(mySDI12.available()) break;
  }
  
  // in this example we will only take the 'DO' measurement  
  mySDI12.flush(); 
  command = "";
  command += i;
  // SDI-12 command to get data [address][D][dataOption][!]
  command += "D0!";
  mySDI12.sendCommand(command);
  // wait for acknowlegement
  while(!mySDI12.available()>1);
  // let the data transfer 
  delay(300);
  printBufferToScreen(); 
  mySDI12.flush(); 
}
void printBufferToScreen()
{
  String buffer = "";
  String buffer1 = "";
  String buffer2 = "";
  mySDI12.read(); // consume address
  while(mySDI12.available()){
    char c = mySDI12.read();
    if(c == '+' || c == '-'){
      buffer += '/';   
      if(c == '-') buffer += '-'; 
    } 
    else {
      buffer += c;  
    }
    delay(100); 
  }
 buffer1 = buffer.substring(buffer.indexOf("/") + 1, buffer.lastIndexOf("/"));
 buffer2 = buffer.substring(buffer.lastIndexOf("/") + 1, buffer.lastIndexOf("/") + 5);

 soil_dielctric = buffer1.toFloat();
 soil_temp = buffer2.toFloat();
}

// gets identification information from a sensor, and prints it to the serial port
// expects a character between '0'-'9', 'a'-'z', or 'A'-'Z'. 
char printInfo(char i)
{
  int j; 
  String command = "";
  command += (char) i; 
  command += "I!";
  for(j = 0; j < 1; j++){
    mySDI12.sendCommand(command);
    delay(30); 
    if(mySDI12.available()>1) break;
    if(mySDI12.available()) mySDI12.read(); 
  }

  while(mySDI12.available()){
    char c = mySDI12.read();
    if((c!='\n') && (c!='\r')) Serial.write(c);
    delay(5); 
  } 
}

// converts allowable address characters '0'-'9', 'a'-'z', 'A'-'Z',
// to a decimal number between 0 and 61 (inclusive) to cover the 62 possible addresses
byte charToDec(char i)
{
  if((i >= '0') && (i <= '9')) return i - '0';
  if((i >= 'a') && (i <= 'z')) return i - 'a' + 10;
  if((i >= 'A') && (i <= 'Z')) return i - 'A' + 37;
}

// THIS METHOD IS UNUSED IN THIS EXAMPLE, BUT IT MAY BE HELPFUL. 
// maps a decimal number between 0 and 61 (inclusive) to 
// allowable address characters '0'-'9', 'a'-'z', 'A'-'Z',
char decToChar(byte i)
{
  if((i >= 0) && (i <= 9)) return i + '0';
  if((i >= 10) && (i <= 36)) return i + 'a' - 10;
  if((i >= 37) && (i <= 62)) return i + 'A' - 37;
}
/*-----( THE END )------------------------------------*/
