# Wi-Fi Enabled Heat Recovery Ventilator (HRV) Controller

This project replaces the stock control panel of a Heat Recovery Ventilator (HRV) system with a Wi-Fi–enabled ESP8266 module. It continuously exchanges stale indoor air with fresh outdoor air, providing a healthier, cleaner, and more comfortable home environment.
---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Hardware Setup](#hardware-setup)
4. [Software Requirements](#software-requirements)
5. [Project Structure](#project-structure)
6. [Configuration](#configuration)
7. [Usage](#usage)
8. [Credits](#credits)
9. [License](#license)

---

## Overview

A Heat Recovery Ventilator (HRV) helps maintain fresh indoor air. This project demonstrates how to replace the HRV's existing control panel with an ESP8266-based controller. The controller communicates with the HRV via a single data line (half-duplex), reads the temperature from the HRV sensors, and publishes the data to the LCD screen. The fan speed is controlled using custom logic to just warm the house in winter and cool it in summer, no more blowing cold air in all night in winter.
---

## Features

- **ESP8266 Wi-Fi connectivity** for getting the date to control if its summer or winter logic  
- **Monitor roof temperature** and control the HRV fan speed automatically.  
  
 

---

## Hardware Setup

### HRV Wiring

The HRV unit in the roof exposes six pins, of which the following are used:

| Pin | Function        |
|----:|-----------------|
|  1  | 5V VCC          |
|  2  | GND             |
|  3  | Data (Rx/Tx)    |
|  4  | Data (Unused)   |
|  5  | GND (Unused)    |
|  6  | 5V VCC (Unused) |

For our purposes, we only need three connections:

1. **Pin 1 → 5V** on the ESP8266  
2. **Pin 2 → GND** on the ESP8266  
3. **Pin 3 → GPIO D6** on the ESP8266 (data line)

> **Note**: We use a single wire for both TX and RX (half-duplex) on `GPIO D6` of the ESP8266.

### ESP8266

This code is designed for a typical **ESP8266 NodeMCU** or **Wemos D1** (or similarly compatible boards).

- Power the ESP8266 with **5V** on `Vin` (the onboard regulator will step down to 3.3V).
- Connect GND from HRV to GND on the ESP8266.
- Connect the HRV data line to the **D6** pin. 
- Connect the LCD to pins 4 (SDA) and 5 (SCL) and GND (if not using the 2n7000) and +3.3V
- Connect the optional 2N7000 to reset the display if it get corrupted.
-   Pin 1 = GND, Pin 2 goes to D3 and Pin 3 goes to GND on the LCD    
     _______
    |       |  
    |2N7000 |
    |_______|
	 \|_|_|/
      | | |
      | | | 
      1 2 3
---

## Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/) (VSCode)  
- ESP8266 board support in Arduino IDE or PlatformIO  
- Required libraries:
  - [Adafruit_AHTX0](https://github.com/adafruit/Adafruit_AHTX0
  - [SoftwareSerial](https://www.arduino.cc/en/Reference/softwareSerial)  
  - [u8g2] (https://github.com/olikraus/u8g2) 
  - [Adafruit-GFX-Library] (https://github.com/adafruit/Adafruit-GFX-Library) 
  - [Adafruit_Sensor] (https://github.com/adafruit/Adafruit_Sensor)
  - [Adafruit BusIO] 

---

## Project Structure

```
.
├── hrv.ino             
└── secrets.h           
```

### Notable Files

- **`hrv.ino`**: Contains the setup and main loop for the ESP8266. Handles reading data, sending fan messages, everything basically.  
- **`secrets.h`**: Store your Wi-Fi credentials (excluded from version control for security).  

---

## Configuration

1. **Install Libraries**: Make sure and [SoftwareSerial](https://www.arduino.cc/en/Reference/softwareSerial) is installed.  
2. **Update `secrets.h`**:

   
   const char* ssid         = "YOUR_WIFI_SSID";
   const char* password     = "YOUR_WIFI_PASSWORD";

   ```
3. **Pin Configuration**: By default, the code uses `D6` for the HRV data line, Pin 4 and 5 for the ST7567_JLX12864 LCD screen and Pin 3 for the optional 2N7000 to reset the LCD every 3 hours. If you want to change this, update the definition in the code
   ```cpp
   #define D6 (12) // ESP8266 GPIO pin number for data line  
   
   const byte PinSDA = 4;
   const byte PinSCL = 5; 
   
   const uint8_t LCD_2N7000 = D3;
   ```
4. **Northern Hemisphere**: Change the summer and winter months in the roof logic, there are separate 2 lines to change
   ```cpp  
    if ( ( ( currentRoofTemperature - 4 >= HouseTemp && currentRoofTemperature >= 18 ) 
                    || ( currentRoofTemperature > HouseTemp && currentRoofTemperature > 21 ) ) 
                && ( iMonth == 0 || ( iMonth >= 5 && iMonth <= 11 ) ) ) 
                
    and
    
    if ( (currentRoofTemperature + 2 <= HouseTemp && currentRoofTemperature >= 12 )
              && ( iMonth == 0 || ( iMonth < 5 && iMonth > 11 ) ) )
   ```
   
---

## Usage

1. **Connect Hardware**: Wire the HRV to the ESP8266 as described above.  
2. **Compile and Upload**: Use the Arduino IDE or PlatformIO to compile and upload the code to your ESP8266.  
3. **Check Serial Monitor**:  you will see debug messages in the Serial Monitor at **115200 baud**.  


---

## Credits

- **Spencer** for the [data structure reference](http://www.hexperiments.com/?page_id=47)  
- **chimera** for the [original logic](https://www.geekzone.co.nz/forums.asp?forumid=141&topicid=195424)  
- **millst** for the [TX/RX single pin (half-duplex) technique](https://www.geekzone.co.nz/forums.asp?forumid=141&topicid=195424&page_no=2#2982537)  
- **durankeeley** for the [Set fan speed code and structure of this readme] https://github.com/durankeeley/hrv-ESP8266  
---

## License

This project is provided “as is” without any guarantees. You may use it or modify it for personal or commercial use.

---
 
