// Credit to: Spencer (data structure) http://www.hexperiments.com/?page_id=47
// Credit to: chimera (Original Logic) - https://www.geekzone.co.nz/forums.asp?forumid=141&topicid=195424
// Credit to: millst (TX/RX on the same pin) - https://www.geekzone.co.nz/forums.asp?forumid=141&topicid=195424&page_no=2#2982537
// Using non 5v version of Arduino https://www.instructables.com/HRV-Wireless/
// Credit to durankeeley: version with sending serial code - https://github.com/durankeeley/hrv-ESP8266


#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "time.h"


#include <U8g2lib.h>

const byte PinSDA = 4;
const byte PinSCL = 5;

U8G2_ST7567_JLX12864_F_SW_I2C display(U8G2_R1, PinSCL, PinSDA, U8X8_PIN_NONE );
//U8G2_ST7567_JLX12864_F_HW_I2C display( U8G2_R1, U8X8_PIN_NONE );


#include <Adafruit_AHTX0.h>
Adafruit_AHTX0 aht;

// Replace with your network credentials
#include "secrets.h"


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * 12;
const int   daylightOffset_sec = 0000;


// HRV constants
#define MSGSTARTSTOP 0x7E
#define HRVROOF 0x30
#define HRVHOUSE 0x31


// Pin definitions
#define D6 (12)



char         packetBuffer[255];
int          iTotalDelay = 0;
int          iStatLine = 2; 

int iLoopX = 0;
int iLoopY = 0;


int TargetHouseTemp  = 24;

// serial and temperature data
byte  serialData[10];
byte  dataIndex        = 0;
byte  checksumIndex    = 0;
bool  dataStarted      = false;
bool  dataReceived     = false;
float currentRoofTemperature = 0.0;

byte  targetFanSpeed;
byte  lastTargetFanSpeed = 0;
char  tempLocation     = 'R';
char  HRVTemperature_buff[16];
char  FanSpeed_buff[16];

String sMessage = "Idle";

char  LocalDate_buff[80];
int   iMonth = 0;
bool  bHaveTime   = false;

float HouseTemp = 0;
float HouseHumidity = 0;

// timing variables and constants:
unsigned long previousReadMillis = 0;
unsigned long previousLCDResetMillis = 0;

const unsigned long SERIAL_READ_INTERVAL = 10000;      // 10 seconds
const unsigned long LCD_RESET_INTERVAL = 1800000;    // 30 mins

SoftwareSerial hrvSerial;

// Define message buffer and publish string
char message_buff[16];

// TTL hrvSerial data indicators
bool bStarted = false;
bool bEnded = false;

// Temperature from Roof or House?
char eTempLoc;

// TTL hrvSerial data array, bIndex, bIndex of checksum and temperature
int iLoop;
byte inData[10];
byte bIndex;
byte bChecksum;

// Temperature for sending to MQTT
float fHRVTemp;
int iHRVControlTemp;
int iHRVFanSpeed;


// Maintain last temperature data, don't send MQTT unless changed
float fHRVLastRoof;
float fHRVLastHouse;
int iHRVLastControl;
int iHRVLastFanSpeed;
int iBrokenLoopCount = 0;

void setup() 
{

  char  buff[100];
  int   iTries = 0;

   
  Serial.begin(115200);
  Serial.println(F("Booting..."));


  // Note: RX/TX on the same pin, so setting half-duplex, buffer size 256
  hrvSerial.begin(1200, SWSERIAL_8N1, D6, D6, false, 256);
  hrvSerial.enableIntTx(false); // disable tx interrupt

  targetFanSpeed = 0x00;

  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);
  myDelay(1000);

  bHaveTime = false;
  
  iTries = 0;
  while (WiFi.status() != WL_CONNECTED && iTries < 300 ) 
  {
    myDelay(1000);
    Serial.print(".");
    iTries +=1;
  }

  if ( WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi");
    bHaveTime = false;
    iMonth = 0;
  }
  else
  {
    Serial.println("Connected to WiFi");

    myDelay(3000);

    struct tm timeinfo;

    iTries = 0;
    while (iMonth == 0 && iTries < 30 ) 
    {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      myDelay(1000);
      Serial.print(".");
      iTries +=1;

      if(!getLocalTime(&timeinfo))
      {
        Serial.println("Failed to obtain time");
        strcpy(LocalDate_buff,"");
        bHaveTime = false;
        iMonth = 0;
      }
      else
      {      

        strftime(LocalDate_buff, 80, "%m", &timeinfo);
        iMonth = atof(LocalDate_buff);

        //strftime(LocalDate_buff, 80, "%Y-%m-%d %H:%M:%S", &timeinfo);
        strftime(LocalDate_buff, 80, "%d/%m/%Y", &timeinfo);
        Serial.println( LocalDate_buff );

        bHaveTime = true;
        
      }

    }
       
    
  }
  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);


  Serial.println(F("LCD init..."));
  
  Wire.begin();
 
  display.setI2CAddress(0x3f * 2) ; // Must be * 2);

  // Moved into ResetLCD routine as it turns mirror after a few hours
  //display.begin();
  //display.setContrast(140);
  //display.clearBuffer();
 
  ResetLCD();

  display.setFont(u8g2_font_lucasfont_alternate_tr); 
  display.clearBuffer();

  randomSeed(analogRead(0));

  DrawDisplay();

  display.setCursor(10,10);
  display.print("Starting");
  display.sendBuffer();


  if (! aht.begin()) 
  {
    Serial.println("Could not find AHT? Check wiring");
    
  }
  else
  {
    Serial.println("AHT10 or AHT20 found");
  }

  // Initialize defaults
  bIndex = 0;
  bChecksum = 0;
  iTotalDelay=0;

  currentRoofTemperature = -99;

}

void loop() {

  unsigned long currentMillis = millis();

  if ( currentMillis - previousLCDResetMillis >= LCD_RESET_INTERVAL ) 
  {
    previousLCDResetMillis = currentMillis;

    ResetLCD();

    if ( bHaveTime )
    {
      struct tm timeinfo;
      if(!getLocalTime(&timeinfo))
      {
        Serial.println("Failed to obtain time");
        strcpy(LocalDate_buff,"No Time");
        //bHaveTime = false;
        iMonth = 0;

      }
      else
      {
        strftime(LocalDate_buff, 80, "%m", &timeinfo);
        iMonth = atof(LocalDate_buff);
        
        strftime(LocalDate_buff, 80, "%d/%m/%Y", &timeinfo);
        Serial.println( LocalDate_buff );
      }
    }


  } 

  //DrawBorder();

  if ( currentMillis - previousReadMillis >= SERIAL_READ_INTERVAL ) //|| previousReadMillis == 0 
  {
    previousReadMillis = currentMillis;

 
    // If we fail to read something from roof or house for 15 mins then turn off the fan 
    if ( HouseTemp != -99 && currentRoofTemperature != -99 )
    {
      iBrokenLoopCount = 0;
      sMessage = "";
    }
    else
    {
      iBrokenLoopCount +=1;

      if ( iBrokenLoopCount > ( 900 / (SERIAL_READ_INTERVAL/ 1000) ) )  // 60 seconds * 15 mins / SERIAL_READ_INTERVAL = 15 mins of loops
      {
        iBrokenLoopCount = 0;
        targetFanSpeed = 0;

        if (HouseTemp == -99)
        {
          sMessage = "Bad House"; 
        }
        else
        {
          sMessage = "Bad Roof";
        }

        DrawDisplay();
  
      }

    }
 
    
    sensors_event_t humidity, temp;

    aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    Serial.print("Temperature: "); 
    Serial.print(temp.temperature); 
    
    Serial.print("Humidity: "); 
    Serial.print(humidity.relative_humidity); 

    HouseHumidity = humidity.relative_humidity;
    HouseTemp = temp.temperature;

    if (isnan(HouseHumidity) || isnan(HouseTemp) ) 
    {
      Serial.println(F("Failed to read from DHT sensor!"));
      HouseHumidity = -99;
      HouseTemp = -99;
    }
    else
    {
      // My AHT20 seems to be getting temp a bit too high
      HouseTemp -= 3;
    }
    
    Serial.print(F("Humidity: "));
    Serial.print(HouseHumidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(HouseTemp);
    Serial.print(F("°C "));
   

    // Send the packet to the HRV Roof 
    byte messageData[] = {0x31, 0x01, 0x9E, targetFanSpeed, 0x0E, 0x80, 0x70};

    // Calculate the checksum
    byte checksum = calculateChecksum(messageData, sizeof(messageData));
  

    //
    // From http://www.hexperiments.com/?page_id=47
    //
    // All fields are one byte except [Data].
    //  
    // [Start][ID][Data][Checksum][End]
    //
    // [Start]    = 0x7E
    // [ID]       = Message ID
    // [Data]     = Message data (multiple bytes)
    // [Checksum] = Message checksum (see below)
    // [End]      = 0x7E

    // Escaped characters in message body:
    // 7E -> 7D 5E
    // 7D -> 7D 5D

    // Example:
    // 7E 31 01 A6 00 14 00 78 9C 7E
    // [7E] Start
    // [31] Message ID
    // [01 A6 00 14 00 78] Data
    // [9C] Checksum
    // [7E] End 

    // The checksum is calculated by starting with zero and subtracting 
    // (modulo 256) each byte in [ID] and [Data].

    // Checksum calculation: (0x00 - 0x31 - 0x01 - 0xA6 - 0x00 - 0x14 -0x00 - 0x78) % 0x100 = 0x9C
    
	//	    Message ID 1
	//	
	//	Control panel data.
	//	
	//	[ID] = 0x31 (ASCII �1�)
	//	[Data]: 6 bytes
	//	
	//	2 bytes: Control panel temperature
	//	1 byte: Unknown (Possibly fan speed?)
	//	1 byte: Control panel set temperature (in whole degrees C)
	//	2 bytes: Unknown
	//	
	//	Example:
	//	7E 31 01 A6 00 14 00 78 9C 7E
	//	
	//	Temperature 01A6 hex =  422 decimal
	//	422 * 1/16 degrees = 26.375 degrees C.
	//	
	//	Set temperature 14 hex = 20 decimal = 20 degrees.
	//	Message ID 0
	//	
	//	Response from roof controller.
	//	
	//	[ID] = 0x30 (ASCII �0�)
	//	[Data] = 3 bytes:
	//	2 bytes: Roof temperature
	//	1 byte: Unknown
	//	
	//	Example:
	//	7E 30 02 1D 00 B1 7E
	//	
	//	Temperature 021D hex =  541 decimal
	//	541 * 1/16 degrees =  33.8125 degrees C


    // Positions:
    //   [0] => MSGSTARTSTOP (0x7E) - Start of frame
    //   [1] => ID: 0x31 = Control Panel
    //   [2] => Control Panel Temp: first part
    //   [3] => Control Panel Temp: second part
    //   [4] => Target Fan Speed
    //   [5] => Control Panel Set Temp (degrees C)
    //   [6] => Unknown: Control Panel Mode first part ?
    //   [7] => Unknown: Control Panel Fan Mode second part ?
    //   [8] => Checksum
    //   [9] => MSGSTARTSTOP (0x7E) - End of frame

    // Construct the full message
    byte message[] = {MSGSTARTSTOP, 0x31, 0x01, 0x9E, targetFanSpeed, 0x0E, 0x80, 0x70, checksum, MSGSTARTSTOP};
    dumpMessage(message, sizeof(message));

    hrvSerial.enableTx(true);
    hrvSerial.write(message, sizeof(message));
    
      Serial.print(F("TX: "));
      Serial.write(message, sizeof(message));
  
  
    hrvSerial.enableTx(false);
    delay(10);



    if (hrvSerial.available() == 0)
    {
      Serial.println("No serial data detected!");
      sMessage = "No Serial!";
      currentRoofTemperature = -99;
    }
    else
    {
      Serial.println("Yay Serial data!");
    }

    bStarted = false;
    bEnded = false;
    bIndex = 0;

    // Only if we're plugged in...
    while (hrvSerial.available() > 0)
    {
      // Read hrvSerial data
      int inChar = hrvSerial.read();

      // Start or stop marker or data too long - which is it? Wait til next loop
      if (inChar == MSGSTARTSTOP || bIndex > 8)
      {
        // Start if first time we've got the message
        if (bIndex == 0)
        {
          bStarted = true; 
        }
        else
        {
          bChecksum = bIndex-1;
          bEnded = true;
          break;
        }
      }  

      // Grab data
      if (bStarted == true)
      {
              
        // Double check we actually got something
        if (sizeof(inChar) > 0)
        {
          //Serial.print(inChar, HEX);
          //Serial.print(",");
          inData[bIndex] = inChar;
          bIndex++;
        }
      }

      // Time for WDT
      myDelay(1);
      
    }

    // Validate data, or if not enough data will fail
    if (bStarted && bEnded && bChecksum > 0)
    {
      int iChar;
      int iLess;

      // Checks
      byte bCalc;
      String sCalc;
      byte bCheck;
      String sCheck;

      // Subtract from zero
      iChar = 0;
      
      // Subtract each byte in ID and data
      for (int iPos=1; iPos < bChecksum; iPos++)
      {
        iLess = inData[iPos];
        iChar = iChar - iLess;
      }

      // Convert calculations
      bCalc = (byte) (iChar % 0x100);
      sCalc = decToHex(bCalc, 2);
      bCheck = (byte) inData[bChecksum];
      sCheck = decToHex(bCheck, 2);
      
      // Mod result by 256 and compare to checksum, or not enough data
      if (sCalc != sCheck || bIndex < 6) 
      {
        // Checksum failed, reset
        bStarted = false;
        bEnded = false;
        bIndex = 0;
        
        // Need to flush, maybe getting the end marker first 
        hrvSerial.flush();
      }
      
      // Reset checksum
      bChecksum = 0;
          
    }

    // We got both start and end messages, process the data
    if (bStarted && bEnded)
    {

      // Only process if we got enough data, minimum 6 characters
      if (bIndex > 5)
      {   
          
        String sHexPartOne;
        String sHexPartTwo;
        int iPos;
        
        // Pull data out of the array, position 0 is 0x7E (start and end of message)
        for (int iPos=1; iPos <= bIndex; iPos++)
        {
          
          // Position 1 defines house or roof temperature
          if (iPos==1) { eTempLoc = (char) inData[iPos]; }

          // Position 2 and 3 are actual temperature, convert to hex
          if (iPos == 2) { sHexPartOne = decToHex(inData[iPos], 2); }
          if (iPos == 3) { sHexPartTwo = decToHex(inData[iPos], 2); }

          // Fan speed
          if (eTempLoc == HRVHOUSE && iPos == 4) { iHRVFanSpeed = inData[iPos]; }
          
          // If temperature is from control panel
          if (eTempLoc == HRVHOUSE && iPos == 5) { iHRVControlTemp = inData[iPos]; }

        }
        
        // Concatenate first and second hex, convert back to decimal, 1/16th of dec + rounding
        // Note: rounding is weird - it varies between roof and house, MQTT sub rounds to nearest 0.5
        fHRVTemp = hexToDec(sHexPartOne + sHexPartTwo);
        fHRVTemp = (fHRVTemp * 0.0625);

        int iHRVTemp;
        iHRVTemp = (int) ((fHRVTemp * 2) + 0.5);
        fHRVTemp = (float) iHRVTemp / 2;

       
        if (eTempLoc == HRVROOF)
        {
            Serial.print("Roof ");
            Serial.println(message_buff);
            
            // Only send if changed by +/- 0.5 degrees
            if (fHRVTemp != fHRVLastRoof)
            {
              //Serial.println("(sending)");
              fHRVLastRoof = fHRVTemp;
              currentRoofTemperature = fHRVTemp;
              
              Serial.print("Current Roof Temperature: ");
              Serial.println(currentRoofTemperature);
             
            }
        }


        // Control the fan
        // If the house is cold then pump in warm if available
        if ( HouseTemp != -99 && currentRoofTemperature != -99 )
        {
          // Goldilocks zone achieved  
          if ( HouseTemp == TargetHouseTemp  )
          {
              targetFanSpeed = 0;
              sMessage = "Idle";
          }
          else
          // Porridge is too cold
          // Only warm the house if its Winter - i.e. May to November
          if ( HouseTemp < TargetHouseTemp  )
          {
            // If the roof air is warmer then pump it in
            if ( ( ( currentRoofTemperature - 4 >= HouseTemp && currentRoofTemperature >= 18 ) 
                    || ( currentRoofTemperature > HouseTemp && currentRoofTemperature > 21 ) ) 
                && ( iMonth == 0 || ( iMonth >= 5 && iMonth <= 11 ) ) )
            {
              targetFanSpeed = 100;
              sMessage = "Warming";
            }
            else
            {
              targetFanSpeed = 0;
              //sMessage = "Idle";
              if (( iMonth >= 5 && iMonth <= 11 ) )
              {
                sMessage = "Winter";
              }
              else
              {
                sMessage = "Summer";
              }
              
            }

          }
          else
          // Porridge is too hot so blow on it
          // Only cool the house if its Summer i.e. December to April
          if ( HouseTemp > TargetHouseTemp  )
          {
            // *IF the roof air is cooler then pump it in
            if ( (currentRoofTemperature + 2 <= HouseTemp && currentRoofTemperature >= 12 )
              && ( iMonth == 0 || ( iMonth < 5 && iMonth > 11 ) ) )
            {
              targetFanSpeed = 50;
              sMessage = "Cooling";
            }
            else
            {
              targetFanSpeed = 0;
              //sMessage = "Idle";
              if ( ( iMonth < 5 && iMonth > 11 ) )
              {
                sMessage = "Summer";
              }
              else
              {
                sMessage = "Winter";
              }
            }
          }

        }
        else
        {
          sMessage = "Bad Temp";
          targetFanSpeed = 0;
        }
        
        // Reset defaults for processing
        bStarted = false;
        bEnded = false;
        bIndex = 0;
          
      }
      
    }
    else
    {
      // Wait for hrvSerial to come alive
      myDelay(2000);

      sMessage = "Check HRV";
  
    }

    DrawDisplay();

  }

  
  
}


void ResetLCD()
{
   display.begin();
  display.setContrast(140);
  display.clearBuffer();

}
void DrawBorder()
{
  display.setDrawColor(0);
  display.drawLine(iLoopX, iLoopY, iLoopX,  iLoopY);

  if ( iLoopY == 0 )
  {
    iLoopX +=1;
    if ( iLoopX > 63 )
    {
      iLoopX = 63;
      iLoopY = 1; 
    }
  }
  else 
  if ( iLoopX == 63 )
  {
    iLoopY +=1;
    if ( iLoopY > 127 )
    {
      iLoopY = 127;  
      iLoopX = 62;
      
    }
  }
  else 
  if ( iLoopY == 127 )
  {
    iLoopX -=1;
    if ( iLoopX < 0 )
    {
      iLoopX = 0;  
      iLoopY = 126;
      
    }
  }
  else 
  if ( iLoopX == 0 )
  {
    iLoopY -=1;
    if ( iLoopY < 0 )
    {
      iLoopY = 0;  
      iLoopX = 1;
      
    }
  }
  display.setDrawColor(1);
  display.drawLine(iLoopX, iLoopY, iLoopX,  iLoopY);
  display.sendBuffer();

}

void DrawDisplay()
{

    
    // Based on OLED code
    
    display.clearBuffer();
   
    iStatLine = random(1,16);


    display.drawLine(2, iStatLine +32, 61,  iStatLine +32);
    display.drawLine(2, iStatLine +72, 61,  iStatLine +72);
    display.drawLine(2, iStatLine +32, 2, iStatLine +72);
    display.drawLine(61, iStatLine +32, 61, iStatLine +72);
    

    display.drawLine(2, iStatLine +32, 17, iStatLine +8);
    display.drawLine(17, iStatLine +8, 47, iStatLine +8);
    display.drawLine(47, iStatLine +8, 61, iStatLine +32);

    // Draw the chimney
    display.drawLine(6, iStatLine +26, 6, iStatLine +8);
    display.drawLine(6, iStatLine +8, 12, iStatLine + 8);
    display.drawLine(12, iStatLine +8, 12, iStatLine +15);
  

    display.setCursor(23,iStatLine + 24);
    if ( currentRoofTemperature == -99 )
    {
      display.print("Err");
    }
    else
    {
      display.print((int)currentRoofTemperature);
      display.setCursor(37,iStatLine + 24);
      display.print("C");
    }
   
    
    display.setCursor(23,iStatLine + 46);
    display.print( (int) HouseTemp );
    display.setCursor(37,iStatLine + 46);
    display.print("C");
    display.setCursor(23,iStatLine + 58);
    display.print( (int) HouseHumidity );
    display.setCursor(37,iStatLine + 58);
    display.print("%");
    
    display.setCursor(34,iStatLine + 70);
    display.print(">");
    display.setCursor(41,iStatLine + 70);
    display.print((int)TargetHouseTemp);
    display.setCursor(54,iStatLine + 70);
    display.print("C");

    
    display.setCursor(4,iStatLine + 84);
    display.print(sMessage);
   

    display.setCursor(4,iStatLine + 98);
    display.print("Fan");

    
    if (targetFanSpeed == 0)
    { 
      display.setCursor(28,iStatLine + 98);
      display.print("Off");
    }
    else 
    {      
      display.setCursor(28,iStatLine + 98);
      display.print((int)targetFanSpeed);
      display.setCursor(50,iStatLine + 98);
      display.print("%");

    }


    display.setCursor(1,iStatLine + 112);
    display.print(LocalDate_buff);
   
    display.sendBuffer();


}

//
// Convert from decimal to hex
//
String decToHex(byte decValue, byte desiredStringLength) 
{
  
  String hexString = String(decValue, HEX);
  while (hexString.length() < desiredStringLength) hexString = "0" + hexString;
  
  return hexString;
}


//
// Convert from hex to decimal
//
unsigned int hexToDec(String hexString) 
{
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}
//
// This function yields back to the watchdog to avoid random ESP8266 resets
//
void myDelay(int ms)  
{

  int i;
  for(i=1; i!=ms; i++) 
  {
    delay(1);
    if(i%100 == 0) 
   {
      ESP.wdtFeed(); 
      yield();
    }
  }

  iTotalDelay+=ms;
  
}

byte calculateChecksum(byte* data, size_t length) {
  int checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum -= data[i];
  }
  return (byte)(checksum % 0x100);
}

void dumpMessage(const byte* message, size_t length) {
  // Each byte will occupy up to 3 characters: 2 hex digits + a space.
  // Add some space for a prefix like "Message: " and the terminator.
  char buffer[3 * length + 16];
  int offset = 0;

  // Start the string
  offset += sprintf(buffer + offset, "Message: ");

  // Append each byte in hex
  for (size_t i = 0; i < length; i++) {
    // %02X prints two hex digits (zero-padded if <0x10), uppercase
    offset += sprintf(buffer + offset, "%02X ", message[i]);
  }

  // Print it all in one go
  Serial.printf("%s\n", buffer);
}


