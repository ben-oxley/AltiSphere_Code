#include <SoftwareSerial.h>
#include <WString.h>

SoftwareSerial mySerial(6, 9,true);
char GPStime[20];
char GPSlat[20];
char GPSlong[20];
char GPSalt[20];
char GPSchecksum[20];

void setup()  
{
  pinMode(5,OUTPUT);
  pinMode(8,OUTPUT);
  digitalWrite(4,LOW);
  digitalWrite(8,LOW);
  Serial.begin(9600);
  Serial.println("SoftSerial Ready");

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);
  Serial.println("HardSerial Ready");
}
//String line = "$GPRMC,210508.000,A,4940.1220,N,00759.4077,E,1.12,2.87,101109,,*0D";
//Time,Lat,Long,Alt,CheckSum
void loop() // run over and over
{
char inData[] = "UUUUBEN,18:27:01,52.00000,-0.27585,2981*9ACE";
byte index = 0;
/*
   while(mySerial.available() > 0)
   {
	char aChar = mySerial.read();
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
*/
    int result = sscanf (inData,"%*[^','],%[^','],%[^','],%[^','],%[^'*']*%s",&GPStime, &GPSlat, &GPSlong, &GPSalt, &GPSchecksum);//, &GPSlong, &GPSalt);//, &GPSchecksum);
    //"CMGL: 9,\"REC READ\",\"0137443256780\",,149,8"
    //"CMGL:%d,\"REC READ\",\"%99[0-9]s"
    //Serial.print(result);
    Serial.print("Time: ");
    Serial.print(GPStime);
    Serial.print(" Lat: ");
    Serial.print(GPSlat);
    Serial.print(" Long: ");
    Serial.print(GPSlong);
    Serial.print(" Alt: ");
    Serial.print(GPSalt);
    Serial.print(" Checksum: ");
    Serial.println(GPSchecksum);
    delay(1000);
}
