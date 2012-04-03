//ISSUE #2: “wprogram.h” not found.  It has been renamed “Arduino.h”

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

Servo Goes between 18 and 90

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
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <avr/wdt.h>
#include <WString.h>
#include <stdio.h>
#include <stdlib.h>
#include <Time.h>  
#include <ctype.h>


//Time settings
#define TIME_MSG_LEN  11   // time sync to PC is HEADER followed by Unix time_t as ten ASCII digits
#define TIME_HEADER  'T'   // Header tag for serial time sync message
#define TIME_REQUEST  7 
#define TIME_SET_StrLen 17  // the maximum length of a time set string hh:mm:ss,dd,mm,yyyy

//Servo Settings
#define servopin 7
#define servoOpen 255
#define servoClosed 1
Servo vservo;
int lastservopos;

//Serial Connection Settings
SoftwareSerial commSerial(6, 9);
char inData[100];
char GPStime[20];
char GPSlat[20];
char GPSlong[20];
char GPSalt[20];
char GPSchecksum[20];

//Pressure Sensor Variables
int sensorValue;
int pressure;

//Time Vars
TimeElements te;
#define datenow "030412"




void setup() {
  Serial.begin(9600);
  pinMode(4,OUTPUT);
  pinMode(8,OUTPUT);
  digitalWrite(4,LOW);
  digitalWrite(8,LOW);
  vservo.attach(7);
  Serial.println();
  commSerial.begin(9600); 
  Serial.println("Run Altisphere Logging");
}

void loop() {
  //readpressure;
  //logit;
  readpressure();
  waitforcomms();
  logit();
  processSetTime();
  digitalClockDisplay();
  delay(1000);
}

void readpressure() {
  sensorValue = analogRead(A2);
  pressure = map(sensorValue, 0, 730, -2000, 2000);  //Map ADC to pressure
}

void logit() {
  int timenow = millis();
  Serial.print("Since On,");
  Serial.print(timenow, DEC);
  Serial.print(",Time,");
  Serial.print(now());
  Serial.print(",Lat,");
  Serial.print(GPSlat);
  Serial.print(",Long,");
  Serial.print(GPSlong);
  Serial.print(",Alt,");
  Serial.print(GPSalt);
  Serial.print(",Checksum,");
  Serial.println(GPSchecksum);
  Serial.print(",Raw Pressure,");
  Serial.print(sensorValue,DEC);
  Serial.print(",Adjusted Pressure,");
  Serial.print(pressure,DEC);
  Serial.print("\n");
}

void servopos(int pos) {
  digitalWrite(4,HIGH);
  if (pos != lastservopos)
  if (pos >= lastservopos)
    {
      for (int x = lastservopos; x <= pos; x++) 
      {
      vservo.write(x);
      delay(50);
      }
    }
  else
    {
      for (int x = lastservopos; x >= pos; x--) 
      {
      vservo.write(x);
      delay(50);
      }
    }
  digitalWrite(4,LOW);
  pos == lastservopos;
}

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
   //char inData[] = "UUUUBEN,18:27:01,52.00000,-0.27585,2981*9ACE";
   int result = sscanf (inData,"%*[^','],%[^','],%[^','],%[^','],%[^'*']*%s",&GPStime, &GPSlat, &GPSlong, &GPSalt, &GPSchecksum);
   //Do Checksum
    /*
    do
    {
      
    } while (millis()-TimerA >= 1000UL); */
        
}

void sendcomms (String message) {

  digitalWrite(8,HIGH);
  commSerial.begin(7200);
  commSerial.print(message);
}


//The time can also be set as follows:
//  S[hh:mm:ss dd mm yy]  


void processSetTime(){
  //[hh:mm:ss dd mm yyyy]
  String timedate = GPStime;
  timedate += datenow;
  Serial.println(datenow);
  char setString[TIME_SET_StrLen];
  int index = 0;
  for (int i = 0; i < TIME_SET_StrLen; i++) {
     char c = timedate[i];
     if( isdigit(c))  // non numeric characters are discarded
	 setString[index++] = c -'0'; // convert from ascii    
  }


  breakTime(now(), te);   // put the time now into the TimeElements structure  
  
  int count = index;
  int element = 0;
  for( index = 0; index < count; index += 2)  // step through each pair of digits
  {
	int val = setString[index] * 10 + setString[index+1] ; // get the numeric value of the next pair of numbers
	switch( element++){
	  case  0 :  te.Hour = val; break;
	  case  1 :  te.Minute = val; break;
	  case  2 :  te.Second = val; break;
	  case  3 :  te.Day = val; break;
	  case  4 :  te.Month= val; break;
	  case  5 :  te.Year = val + 30; break; // year 0 is 1970	  
	}    
  }
  if (makeTime(te) > now()) setTime( makeTime(te));
  
}

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

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
  
  
  
