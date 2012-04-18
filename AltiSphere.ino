// ___________________________
//|        Ben Oxley          |
//|AltiSphere Pre Release Code|
//|___________________________|
//Must initalise some variables on day of flight, may not work over midnight

//FIX: The release notes includes a handy pre-compiler directive to check of the arduino flavor you are using.

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

/*
 Output  0 - RXI                        Output  7 - SERVO DATA
 Output  1 - TXO                        Output  8 - DRIVE ENABLE COMMS
 Output  2 - N/C                        Output  9 - DRIVE COMMS
 Output  3 - N/C                        Output 10 - N/C
 Output  4 - MOSFET ON                  Output 11 - MOSI
 Output  5 - NOT RECEIEVE ENABLE COMMS  Output 12 - MISO
 Output  6 - RECIEVE COMMS              Output 13 - SCK
 
 Servo Goes between 18 and 90 Degrees
 
 Analog 0 - N/C
 Analog 1 - N/C
 Analog 2 - Pressure Sensor   Vout = VS × (0.2 × P(kPa)+0.5) ± 6.25% VFSS  Voltage is divided by two. Range 0.25-->2.25v Gives a resolution of 6 Pa (4000/(2/(3.3/1024)))
 Analog 3 - N/C
 Analog 4 - Temperature Sensors SDA
 Analog 5 - Temperature Sensors SCL
 Analog 6 - N/C
 Analog 7 - N/C 
 
 Ideal analog divider is 10K and 27K   730, 378, 3
 */

#include <Arduino.h> //Includes for compiling
#include <SoftwareSerial.h> //Include for software serial lines
#include <Servo.h> //Includes for servo control on any pins
#include <avr/wdt.h> //Includes for watchdog timer
#include <WString.h> //
#include <stdio.h>
#include <stdlib.h>
#include <Time.h> //Includes for time keeping library
#include <ctype.h> 
#include <util/crc16.h> //Includes for crc16 cyclic redundancy check to validate serial comms

//Time settings
#define TIME_MSG_LEN  11   // time sync to PC is HEADER followed by Unix time_t as ten ASCII digits
#define TIME_HEADER  'T'   // Header tag for serial time sync message
#define TIME_REQUEST  7 
#define TIME_SET_StrLen 17  // the maximum length of a time set string hh:mm:ss,dd,mm,yyyy

//Servo Settings
#define servopin 7 //Digital Pin servo is connected to
#define servoOpen 90 //Servo open position
#define servoClosed 18 //Servo closed position
Servo vservo; //Create servo object for valve servo
int lastservopos; //Integer to store the last given position of the servo

//Serial Connection Settings
SoftwareSerial commSerial(6, 9); //Creates software serial object and defines the tx and rx pins
char inData[60]; //Character array for serial data recieved
char GPStime[8]; //Character array to store time string from data recieved
char GPSlat[10]; //Character array to store latitude string from data recieved
char GPSlong[10]; //Character array to store longitude string from data recieved
char GPSalt[7]; //Character array to store altitude string from data recieved
char GPSchecksum[4]; //Character array to store checksum string from data recieved
float lat; //Float value for latitude
float lon; //Float value for longitude
float alt; //Float value for altitude
boolean validcrc; //Boolean to declare if the recieved message was valid


//Pressure Sensor Variables
int sensorValue; //Integer value for ADC reading from pressure sensor
int pressure; //Integer value for pressure given by the mapping formula below

//Time Vars
TimeElements te; //Declare time object

//Speed Variables
float speedarray[20]; //Declare array to store speed values to create moving average
int floatindex; //Declare the current place in the speedarray[]
int timelast; //Initialise time of last valid recieved packet
float altlast; //Initialise last recorded altitude variable
#define MOVINGAVG 10//Define size of moving average

//Pre-Flight Variables
#define datenow "030412" //Define date today (DDMMYY)
#define arraysize 6 //Define size of flight altitude array
int flightalt[] = {
  0,5000,10000,15000,20000,30000}; //Initialise variable array for altitude control points
int flightspd[] = {
  100,5,2,2,2,2}; //Initialise variable array for controlled speeds in m/s

void setup() {
  Serial.begin(9600); //Baud rate for logging to openLog
  pinMode(4,OUTPUT); //Define pins 4 and 8 as outputs for controlling the line driver
  pinMode(8,OUTPUT);
  digitalWrite(4,LOW); //Set settings for transmission for controlling the line driver
  digitalWrite(8,LOW);
  vservo.attach(7); //Attach the servo object to pin 7
  Serial.println(); 
  commSerial.begin(9600); //Start softwareSerial for line driver 
  Serial.println("Run Altisphere Logging");
  servopos(18);
}

//******************************************
//Main loop of the program, this defines the
//order of processes.
//******************************************
void loop() {
  //readpressure;
  //logit;
  readpressure();
  waitforcomms();
  if (validcrc) {
    processSetTime();
    logspeed();
    //servomove();
  }
  servomove();
  //failsafe?
  logit();
  //digitalClockDisplay();
  //delay(1000);
}

//******************************************
//This program controls the servo movements.
//******************************************
void servomove() {
  if ((millis() - timelast) > 3600000) { //If the last GPS lock was an hour ago
    if (speedavg() > flightplan()) {
      servopos(servoOpen);
    } 
    else {
      servopos(servoClosed);
    }
  } 
  else { 
      servopos(servoOpen); //open the valve to dump helium
  }
}


//******************************************
//This reads the analog voltage from the 
//pressure sensor and converts it to a
//mapped pressure.
//******************************************
void readpressure() {
  sensorValue = analogRead(A2);
  pressure = map(sensorValue, 0, 730, -2000, 2000);  //Map ADC to pressure
}

//******************************************
//Sends string of values to the openLog sys
//-tem. 
//******************************************
void logit() {
  int timenow = millis();
  Serial.print("Since On,");
  Serial.print(timenow, DEC);
  Serial.print(",Time,");
  Serial.print(now());
  Serial.print(",Lat,");
  Serial.print(GPSlat);
  Serial.print(",");
  Serial.print(lat,DEC);
  Serial.print(",Long,");
  Serial.print(GPSlong);
  Serial.print(",");
  Serial.print(lon,DEC);
  Serial.print(",Alt,");
  Serial.print(GPSalt);
  Serial.print(",");
  Serial.print(alt,DEC);
  Serial.print(",Checksum,");
  Serial.println(GPSchecksum);
  Serial.print(",Raw Pressure,");
  Serial.print(sensorValue,DEC);
  Serial.print(",Adjusted Pressure,");
  Serial.print(pressure,DEC);
  Serial.print(",Last Servo Position,");
  Serial.print(lastservopos,DEC);
  Serial.print("\n");
}

//******************************************
//Program to slowly change the servo positi
//-on. This is to avoid over-stressing the
//servo or surrounding mount structure.
//******************************************
void servopos(int pos) {
  digitalWrite(4,HIGH); //Turn the MOSFET on
  if (pos != lastservopos) //If the desired servo position has changed
    if (pos >= lastservopos) //If the servo position has increased
    {
      for (int x = lastservopos; x <= pos; x++) //Slowly increase the servo's position
      {
        vservo.write(x);
        delay(50);
      }
    }
    else //Else the servo position has decreased
  {
    for (int x = lastservopos; x >= pos; x--) //Slowly decrease the servo's position
    {
      vservo.write(x);
      delay(50);
    }
  }
  digitalWrite(4,LOW); //Turn the MOSFET off
  lastservopos = pos; // write the new servo position to the register
}

//******************************************
//Program to recieve communications from 
//the ASTRA tracker. The program then moves
//the data into separate variables and verif
//-ies the checksum and converts the data to
//floats.
//******************************************
void waitforcomms() {
  byte index = 0;
  while(commSerial.available() > 0)
  {
    char aChar = commSerial.read();
    if(aChar == '\n')
    {
      // End of record detected. Time to parse

      index = 0;
      inData[index] = NULL;
    }
    else
    {
      inData[index] = aChar;
      index++;
      inData[index] = '\0'; // Keep the string NULL terminated
    }
  }
  Serial.print("Data in: ");
  for (int x = 0; x <= 59; x++) {
    Serial.print(inData[x]);
  }
  Serial.print("\n");
  int pointer;
  while (inData[pointer] != 'B') pointer++; //Find the "B" in the datastring
  //Split the data up by string 
  int result = sscanf (inData,"%*[^','],%[^','],%[^','],%[^','],%[^'*']*%s",&GPStime, &GPSlat, &GPSlong, &GPSalt, &GPSchecksum);
  //Pass the pointer to the program so that it can scroll through
  if (CRC16(&inData[pointer]) == int(GPSchecksum)) { //this might die if the data is bad, use watchdog timer
    validcrc = true;
    //char inData[] = "UUUUBEN,18:27:01,52.00000,-0.27585,2981*9ACE";  
    //$$ASTRA1,27728,14:10:18,5212.5118,00006.3573,522,32.1,485*172A
    lat = atof(GPSlat); //should be convert to float, change to atof
    lon = atof(GPSlong);
    alt = atof(GPSalt);
  } 
  else {
    validcrc = false;
    Serial.print("Incorrect communications string. Result = ");
    Serial.print(result);
    Serial.print("\n");
  }
  //Do Checksum
  /*
    do
   {
   
   } while (millis()-TimerA >= 1000UL); */

}


//******************************************
//Obsolete program to be finished. To be
//used in later versions for sending data to
//the ASTRA tracker.
//******************************************
void sendcomms (String message) {
  digitalWrite(8,HIGH);
  commSerial.begin(7200);
  commSerial.print(message);
}

//******************************************
//Program to process the time into a time
//object based on the GPS time. It compares
//this with the internally kept time to
//check this.
//The time can also be set as follows:
//S[hh:mm:ss dd mm yy]  
//******************************************
void processSetTime(){
  //[hh:mm:ss dd mm yyyy]
  String timedate = GPStime; //write GPStime into a new string
  timedate += datenow; //concatenate time and date together
  //Serial.println(datenow); //print the string for testing
  char setString[TIME_SET_StrLen]; //initialse character array for writing the ascii values into
  int index = 0; //start the index for the for loop
  for (int i = 0; i < TIME_SET_StrLen; i++) { //For the length of the string
    char c = timedate[i]; // take the current byte of the string
    if( isdigit(c))  // non numeric characters are discarded
      setString[index++] = c -'0'; // convert from ascii to integer
  }
  breakTime(now(), te);   // put the time now into the TimeElements structure  
  int count = index;
  int element = 0;
  for( index = 0; index < count; index += 2)  // step through each pair of digits
  {
    int val = setString[index] * 10 + setString[index+1] ; // get the numeric value of the next pair of numbers
    switch( element++){ //Set time elements
    case  0 :  
      te.Hour = val; 
      break;
    case  1 :  
      te.Minute = val; 
      break;
    case  2 :  
      te.Second = val; 
      break;
    case  3 :  
      te.Day = val; 
      break;
    case  4 :  
      te.Month= val; 
      break;
    case  5 :  
      te.Year = val + 30; 
      break; // year 0 is 1970	  
    }    
  }
  if (makeTime(te) > now()) setTime( makeTime(te)); //Write current time into time object

}

//******************************************
//Program to send the current time to the
//serial port. Only used for debugging.
//******************************************
void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(dayStr(weekday()));
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(monthShortStr(month()));
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

//******************************************
//Helper function for printing digits to
//serial port. Only used for debugging.
//******************************************
void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10) { //If it is less than 10 then it should be preceeded by a 0
    Serial.print('0');
  }
  Serial.print(digits);
}

//******************************************
//Check flightspeed that is relevant to the
//current altitude. Needs an extended
//function to be written for descent.
//******************************************
float flightplan(){
  float targetspeed;
  for (int i = 0; i < arraysize; i++) //lookup which array element relates to the current altitude
  {
    if ( flightalt[i] <= alt) 
    {
      targetspeed = flightspd[i]; //looks up the target speed for this altitude
    }
  }
  return targetspeed; //returns the target speed to the calling function
}

//******************************************
//Program to log speed into moving average 
//array.
//******************************************
void logspeed(){
  floatindex++;
  if (floatindex >= 19) floatindex = 0; //Checks to see if we are near the end of the array so it can loop around
  float speednow = (alt - altlast)/(millis()-timelast)/1000; //Calculate the current speed
  speedarray[floatindex] = speednow; //Add the current speed to the array
  altlast = alt; // Stores the new altitude as the previous time
  timelast = millis(); //Stores the time related to the most recent altitude recorded
}

//******************************************
//Program to calculate the speed based on a
//moving average array.
//******************************************
float speedavg(){
  float speedcalc; //initalise float for speed
  for (int i = 0; i <= MOVINGAVG ; i++) { //loop through the values to add together
    if ((floatindex - i) < 0) { // if the index drops off the bottom of the array
      speedcalc += speedarray[ 20 - i + floatindex ]; // look at values decreasing from 99
    } 
    else {
      speedcalc +=speedarray[floatindex - i];
    }
  }
  speedcalc /= MOVINGAVG; //divide by the amount of values added together
  return speedcalc; //return the value
}

//******************************************
//Program to verify the CRC check of the
//form CRC-CCITT. the program breaks when it
//finds * or \0
//******************************************
uint16_t CRC16 (char *c)
{
  uint16_t crc = 0xFFFF;
  while (*c && *c != '*') crc = _crc_xmodem_update(crc, *c++);
  return crc;
}
















