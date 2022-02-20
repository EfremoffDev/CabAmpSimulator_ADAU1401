//ALL LIBS INCLUDE
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "graphics.h"
#include <ESPRotary.h>
#include <Button2.h>
#include <math.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/Picopixel.h>
#include <Fonts/TomThumb.h>
#include <EEPROM.h>
#include "FFat.h"
#include <WiFi.h>
#include "default_irs.h"

//GLOBAL VARIABLES
#define EEPROM_SIZE 32


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
const String amplifierName [5] = {"SVT Style", "SWR Style", "EBS Style", "M-Bass Style"}; //DEFATUL AMPLIFIERS LIST
String cabinetName [5] = {"2x10 Bass Cab byEfremoff", "4x10 Hartke XL", "8x10 Ampeg SVT HFL", "2x12 Mesa Subway"}; //DEFAULT CABS LIST


// PINS
#define PIN_AMP_SWITCH 34
#define PIN_AMP_LED 27
#define PIN_CAB_SWITCH 39
#define PIN_CAB_LED 14
#define PIN_OVER_LED 12
#define PIN_OVER_BTN 36
#define PIN_WP 33

// Addresses
#define ADDR_CAB_VOL 0x0053
#define ADDR_AMP_VOL 0x0041
#define ADDR_SAT_GAIN 0x0047
#define ADDR_CAB 0x03B1
#define ADDR_AMP 0x0051
#define ADDR_OVER 0x004F

#define ADDR_LOW_FREQ_B0 0x0005
#define ADDR_LOW_FREQ_B1 0x0006
#define ADDR_LOW_FREQ_B2 0x0007
#define ADDR_LOW_FREQ_A1 0x0008
#define ADDR_LOW_FREQ_A2 0x0009
#define ADDR_MID_FREQ_B0 0x000F
#define ADDR_MID_FREQ_B1 0x0010
#define ADDR_MID_FREQ_B2 0x0011
#define ADDR_MID_FREQ_A1 0x0012
#define ADDR_MID_FREQ_A2 0x0013
#define ADDR_HIGH_FREQ_B0 0x0014
#define ADDR_HIGH_FREQ_B1 0x0015
#define ADDR_HIGH_FREQ_B2 0x0016
#define ADDR_HIGH_FREQ_A1 0x0017
#define ADDR_HIGH_FREQ_A2 0x0018

#define M_PI 3.14159265358979323846

//MAIN COUNSTRUCTORS FOR MODULES:
//AMPLIFIER MODULE
struct AmpStruct {
    int selector = 0;
    int volume;
    bool swFlag = false;
    bool on = true;
    int lastVolume = -1;
    int savedVolume = 0;
    int savedName = 0;
    int low = 0;
    int savedLow = 0;
    ESPRotary encoder;
    Button2 button;
} Amp;
//CABINET MODULE
struct CabStruct {
    int selector = 0;
    int counter = 3;
    int savedVolume = 0;
    int savedName = 0;
    int volume = 0;
    int lastVolume = -1;
    bool swFlag = false;
    int high = 0;
    int savedHigh = 0;
    bool on = true;
    ESPRotary encoder;
    Button2 button;
} Cab;
//OVERDRIVE-SATURATION MODULE
struct SaturationStruct {
    bool swFlag = false;
    bool on = false;
    int gain = 0;
    int savedGain = 0;
    int lastGain = -1;
    int mid = 0;
    int savedMid = 0;
    ESPRotary encoder;
    Button2 button;
} Saturation;

// ALL VARIABLES/FLAGS and other STUFF

bool exitFlag = false;
bool enableSelector = false;
bool holdedFlag = false;
unsigned long startedTime = 0;
unsigned long currentTime = 0;
int modeFlag = 0;
int modeEncFlag = 0;
int modeBtnFlag = 0;

//Display Parameters Setup:
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Service Functions:
//Toggle Function - uses when switch pushed. 

void toggle(uint16_t addr, bool state, String strt){
    sendBytes(0x34, addr, state ? 0 : 1, strt);
    sendBytes(0x34, addr + 1, state ? 1 : 0, strt);
}

//Boolean function for read high/low states
bool high(uint16_t addr){
    return digitalRead(addr) == HIGH;
}

bool low(uint16_t addr){
    return digitalRead(addr) == LOW;
}

void AmpVolume(int vol, String strt){
    float sendVolume = powf(1.017, vol) - 1;
    sendBytes(0x34, ADDR_AMP_VOL, sendVolume, strt);
}

void CabVolume(int vol, String strt ){
  float StartupCabVolume = powf(1.006, vol) - 1;
  sendBytes(0x34, ADDR_CAB_VOL, StartupCabVolume, strt);
}

void SaturationGain(int gain, String strt){
  float sendGain = gain/4.001;

  sendBytes(0x34, ADDR_SAT_GAIN, sendGain, strt);
}
void EQSend(int gain, int freq, float q, String type, String strt){
  //Create a default shape for SVR-Amplifier styte
  if(type == "LOW"){
    gain = gain + 5;
  }
  if(type == "MID"){
    gain = gain - 5;
  }
  if(type == "HIGH"){
    gain = gain + 5;
  }
  float actualGain = gain/2.0; //Real Gain with step 0.5dB. gain is a value between -32 / 32. //C8
  
  //IIR Coefficient calculation for ADAU:
  
  float ax = powf (10.0 ,(actualGain/40));  //C11
  float omega = 2 * M_PI * freq / 48000;// C12
  float snOmg = sin(omega);//C13
  float cosOmg = cos(omega);//C14
  float alpha =  snOmg / (2 * q);//C15
  float a0 = 1 + ( alpha / ax );//C16
  float linearGain = 1 / a0;//C17

  float b0 = (1 +  (alpha * ax)) * linearGain;  
  float b1 = -(2 * cosOmg) * linearGain; 
  float b2 =  (1 - (alpha * ax)) * linearGain;   
  float a1 =  (2 * cosOmg) / a0; 
  float a2 = -(1 - (alpha / ax)) /a0; 
  
  //SEND IIR Coefficients to ADAU:
  if (type == "LOW"){
    sendBytes(0x34, ADDR_LOW_FREQ_B0, b0, strt);
    sendBytes(0x34, ADDR_LOW_FREQ_B1, b1, strt);
    sendBytes(0x34, ADDR_LOW_FREQ_B2, b2, strt);
    sendBytes(0x34, ADDR_LOW_FREQ_A1, a1, strt);
    sendBytes(0x34, ADDR_LOW_FREQ_A2, a2, strt);
  }
  else if (type == "MID"){
    sendBytes(0x34, ADDR_MID_FREQ_B0, b0, strt);
    sendBytes(0x34, ADDR_MID_FREQ_B1, b1, strt);
    sendBytes(0x34, ADDR_MID_FREQ_B2, b2, strt);
    sendBytes(0x34, ADDR_MID_FREQ_A1, a1, strt);
    sendBytes(0x34, ADDR_MID_FREQ_A2, a2, strt);
    
  }
  else if (type == "HIGH"){
    sendBytes(0x34, ADDR_HIGH_FREQ_B0, b0, strt);
    sendBytes(0x34, ADDR_HIGH_FREQ_B1, b1, strt);
    sendBytes(0x34, ADDR_HIGH_FREQ_B2, b2, strt);
    sendBytes(0x34, ADDR_HIGH_FREQ_A1, a1, strt);
    sendBytes(0x34, ADDR_HIGH_FREQ_A2, a2, strt);
  }
}

void setup() {
  WiFi.mode(WIFI_OFF); //Disable wi-fi. Can make some noise in sound part.
  Serial.begin(115200);
  EEPROM.begin(32); //Define eeprom size
  Wire.setClock(400000); //Start I2C with specified speed.
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }
  //CHECK SD-CARD and Files
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
  }
  else{
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
    }
    else{
      listDir(SD, "/", 0);
    }
  }

  //SETUP INPUTS/OUTPUTS:
  pinMode(PIN_WP, OUTPUT);
  digitalWrite(PIN_WP, HIGH);
  pinMode(PIN_AMP_SWITCH, INPUT);
  pinMode(PIN_CAB_SWITCH, INPUT);
  pinMode(PIN_OVER_BTN, INPUT);
  pinMode(PIN_CAB_LED, OUTPUT);
  pinMode(PIN_AMP_LED, OUTPUT);
  pinMode(PIN_OVER_LED, OUTPUT);
  

  //READ SAVED DATA FROM EEPROM:
  Amp.volume = EEPROM.read(20);
  Amp.on = (bool) EEPROM.read(21);
  Amp.selector = EEPROM.read(25);
  Amp.lastVolume = Amp.volume;

  Cab.volume = EEPROM.read(22);
  Cab.on = (bool) EEPROM.read(23);
  Cab.selector = EEPROM.read(26);
  Cab.lastVolume = Cab.volume;

  Saturation.on = (bool) EEPROM.read(24);
  Saturation.gain = EEPROM.read(27);
  Amp.low = EEPROM.read(28);
  Saturation.mid = EEPROM.read(29);
  Cab.high = EEPROM.read(30);

  Amp.encoder.begin(35, 32, 3);
  Cab.encoder.begin(16, 17, 3);
  Saturation.encoder.begin(2, 15, 3);

  Amp.button.begin(PIN_AMP_SWITCH);
  Cab.button.begin(PIN_CAB_SWITCH);
  Saturation.button.begin(PIN_OVER_BTN);

  Amp.button.setLongClickTime(1500);
  Cab.button.setLongClickTime(1500);
  Saturation.button.setLongClickTime(1500);

  //SET ENCODER STATES:
  Amp.encoder.resetPosition(Amp.volume);
  Cab.encoder.resetPosition(Cab.volume);
  Saturation.encoder.resetPosition(Saturation.gain);


  //Start Graphics
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  //SET UI:
  //SET "Firmware Update" view (not used for now).
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(1);
  display.setCursor(7,44);
  display.print("firmware update");
  display.display();

  //SET STARTUP VIEW:
  display.clearDisplay();
  display.drawBitmap(0, 0, E_logo, 128, 64, 1);
  display.setTextSize(1);
  display.setTextColor(1);
  display.setCursor(7,44);
  display.print("amp & cab simulator");
  display.setCursor(40,56);
  display.print("v.1.5.0");
  display.display();

  //SET FFAT SYSTEM for internal memory and print in serial saved files list.

  Serial.println("Starting FFat");
  if (!FFat.begin(true)) {
    Serial.println("An Error has occurred while mounting FATFS");
    return;
  }

  File root = FFat.open("/");
  File file = root.openNextFile();
  while(file){
      Serial.print("FILE: ");
      Serial.println(file.name());

      file = root.openNextFile();
  }

  delay(700); //DELAY FOR STARTUP VIEW

  //SET INFORMATION VIEW OF LOADED AMPLIFIER AND CABINET:
  display.clearDisplay();
  Serial.println("Device is ready");
  AmpSelector(amplifierName[Amp.selector], "Bass Amplifier");
  delay(1000);
  CabSelector(cabinetName[Cab.selector], "INTERNAL");
  delay(1000);
  //SET MAIN DISPLAY UI Function:
  SetMainDisplayData();
  Amp.encoder.setChangedHandler(CheckAmpEncoders);
  Cab.encoder.setChangedHandler(CheckCabEncoder);
  Saturation.encoder.setChangedHandler(CheckSatEncoder);

  Amp.button.setClickHandler(CheckAmpButton);
  Cab.button.setClickHandler(CheckCabButton);
  Saturation.button.setClickHandler(CheckSatButton);

  Amp.button.setLongClickDetectedHandler(CheckAmpButtonLong);
  Cab.button.setLongClickDetectedHandler(CheckCabButtonLong);
  Saturation.button.setLongClickDetectedHandler(CheckSatButtonLong);

  //SET LAST SAVED PARAMETERS in ADAU DSP-PROC.
  AmpVolume(Amp.volume, "STARTUP");
  CabVolume(Cab.volume, "STARTUP");
  SaturationGain(Saturation.gain,"STARTUP");
  toggle(ADDR_AMP, Amp.on, "STARTUP");
  toggle(ADDR_CAB, Cab.on, "STARTUP");
  toggle(ADDR_OVER, Saturation.on, "STARTUP");
  EQSend(Amp.low, 60, 1.0, "LOW", "STARTUP");
  EQSend(Saturation.mid, 950, 0.95, "MID", "STARTUP");
  EQSend(Cab.high, 4500, 0.8, "HIGH", "STARTUP");

  SaveLastCab(Cab.selector);
  //UNMUTE ADAU:
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000011100 - 4th bit - dac/adc unmuted
  Wire.write(0x00);
  Wire.write(0x1C);
  Wire.endTransmission();
}

void loop() {
  currentTime = millis();
  Amp.encoder.loop();
  Cab.encoder.loop();
  Saturation.encoder.loop();

  Amp.button.loop();
  Cab.button.loop();
  Saturation.button.loop();
  SaveData();

}

void SaveData(){
  if (startedTime && currentTime > startedTime + 1000){
      EEPROM.write(20, Amp.volume);
      EEPROM.write(21, Amp.on);
      EEPROM.write(22, Cab.volume);
      EEPROM.write(23, Cab.on);
      EEPROM.write(24, Saturation.on);
      EEPROM.write(25, Amp.selector);
      EEPROM.write(26, Cab.selector);
      EEPROM.write(27, Saturation.gain);
      EEPROM.write(28, Amp.low);
      EEPROM.write(29, Saturation.mid);
      EEPROM.write(30, Cab.high);
      EEPROM.commit();
      exitFlag = false;
      enableSelector = false;
      modeFlag = 0;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      SetMainDisplayData();
      startedTime=0;
  }
}

//COMMON FUNCTIONS:
// modeFlag == 0 - default view
// modeFlag == 1 - Select Amp Mode
// modeFlag == 2 - Select Cab Mode
// modeFlag == 3 - EQ mode.
// modeFlag == 4 - No activity mode.
// modeFlag == 5 - MuteAll mode. 

//AMPLIFIER ENCODER FUNCTION FOR EACH MODES:
void CheckAmpEncoders(ESPRotary& encoder){
  switch(modeEncFlag){
    case 0: //default view
      modeBtnFlag = 4;
      Amp.volume = encoder.getPosition();
      if (Amp.volume >= 0 && Amp.volume <= 64){
          if (Amp.volume != Amp.lastVolume){
            DisplayVolume(Amp.volume, "Amplifier Volume", 15);
            AmpVolume(Amp.volume, "1");
            Amp.lastVolume = Amp.volume;
            startedTime = currentTime;
          }
      }
      else{
        if (Amp.volume < 0){
          Amp.encoder.resetPosition();
        }
        if (Amp.volume > 64){
          Amp.encoder.resetPosition(64);
        }
      }
      break;
      
    case 1: //Select Amp Mode
      Amp.selector = encoder.getPosition();
      if (Amp.selector >= 0 && Amp.selector <= 3){
        AmpSelector(amplifierName[Amp.selector], "Bass Amplifier");
        delay(10); //small debounce delay
      }
      else {
        if (Amp.selector < 0){
          encoder.resetPosition();
        }
        if (Amp.selector > 3){
          encoder.resetPosition(3);
        }
      }
      break;
    case 2: //Select Cab Mode
      break;
    case 3: //EQ mode
      Amp.low = encoder.getPosition();
      if (Amp.low>= -32 && Amp.low <= 32){
            Serial.println("IN Set EQ mode");
            EQSend(Amp.low, 63, 1.05, "LOW", "1");
            ChangeFreq(Amp.low, "LOW");
          }
      else{
        if (Amp.low < -32){
          Amp.encoder.resetPosition(-32);
        }
        if (Amp.low > 32){
          Amp.encoder.resetPosition(32);
        }
      }
      break;
    case 4: //No activity mode.
      break;
    case 5: //MUTE-ALL Mode
      break;
  }
}

//CABINET ENCODER FUNCTION FOR EACH MODES:
void CheckCabEncoder(ESPRotary& encoder){
  switch(modeEncFlag){
    case 0:
      modeBtnFlag = 4;
      Cab.volume = encoder.getPosition();
      if (Cab.volume >= 0 && Cab.volume <= 64){
          if (Cab.volume != Cab.lastVolume){
            DisplayVolume(Cab.volume, "Cabinet Volume", 22);
            CabVolume(Cab.volume, "1");
            Cab.lastVolume = Cab.volume;
            startedTime = currentTime;
          }
      }
      else{
        if (Cab.volume < 0){
          Cab.encoder.resetPosition();
        }
        if (Cab.volume > 64){
          Cab.encoder.resetPosition(64);
        }
      }
      break;
    case 1:
      break;
    case 2:
      Cab.selector = encoder.getPosition();
      if (Cab.selector >= 0 && Cab.selector <= 3){
        CabSelector(cabinetName[Cab.selector], "INTERNAL");
        delay(10);
      }
      else {
        if (Cab.selector < 0){
          encoder.resetPosition();
        }
        if (Cab.selector > 3){
          encoder.resetPosition(3);
        }
      }
      break;
    case 3:
      Cab.high = encoder.getPosition();
      if (Cab.high>= -32 && Cab.high <= 32){
            Serial.println("IN Set EQ mode");
            EQSend(Cab.high, 5000, 0.8, "HIGH", "1");
            ChangeFreq(Cab.high, "HIGH");
          }
      else{
        if (Cab.high < -32){
          Cab.encoder.resetPosition(-32);
        }
        if (Cab.high > 32){
          Cab.encoder.resetPosition(32);
        }
      }
      break;
    case 4:
      break;
    case 5:
      break;
  }
}

//SATURATION/OVERDRIVE ENCODER FUNCTION FOR EACH MODES:
void CheckSatEncoder(ESPRotary& encoder){
  switch(modeEncFlag){
    case 0:
      modeBtnFlag = 4;
      Saturation.gain = encoder.getPosition();
      if (Saturation.gain>= 0 && Saturation.gain <= 64){
          if (Saturation.gain != Saturation.lastGain){
            SaturationGain(Saturation.gain, "1");
            DisplayVolume(Saturation.gain, "Saturation Gain", 18);
            Saturation.lastGain = Saturation.gain;
            startedTime = currentTime;
          }
      }
      else{
        if (Saturation.gain < 0){
          Saturation.encoder.resetPosition(0);
        }
        if (Saturation.gain > 64){
          Saturation.encoder.resetPosition(64);
        }
      }
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      Saturation.mid = encoder.getPosition();
      if (Saturation.mid>= -32 && Saturation.mid <= 32){
            Serial.println("IN Set EQ mode");
            EQSend(Saturation.mid, 800, 0.75, "MID", "1");
            ChangeFreq(Saturation.mid, "MID");
          }
      else{
        if (Saturation.mid < -32){
          Saturation.encoder.resetPosition(-32);
        }
        if (Saturation.mid > 32){
          Saturation.encoder.resetPosition(32);
        }
      }
      break;
    case 4:
      break;
    case 5:
      break;
  }
}
//ENCODER SWITCHS FUNCTIONS:

//AMPLIFIER ENCODER SWITCH for SHORT CLICK FUNCTIONS FOR ALL MODES
void CheckAmpButton(Button2& btn) {
  switch(modeBtnFlag){
    case 0:
      Amp.on = !Amp.on;
      toggle(ADDR_AMP, Amp.on, "1");
      AmpOnOff(Amp.on);
      startedTime = currentTime;
      Amp.swFlag = false;
      delay(100);
      break;
    case 1:
      if(btn.getClickType() !=4 && btn.getClickType() != 0){
        Amp.lastVolume = Amp.volume;
        SetMainDisplayData();
        delay(100);
        startedTime = currentTime;
      
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      }
      break;
    case 2:
      //Cab.selector = Cab.savedName;
      //Cab.volume = Cab.savedVolume;
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break;
    case 3:
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      delay(300);
      break;
    case 4:
      break; 
    case 5:
      startedTime=0;
      delay(400);
      //send unmute for dac/adc
      Wire.beginTransmission(0x34);
      Wire.write(0x081C >> 8); //MSB, shift to 8bit
      Wire.write(0x081C & 0xFF); //LSB specify end
      //send 0000000000011100 - 4th bit - dac/adc unmuted
      Wire.write(0x00);
      Wire.write(0x1C);
      Wire.endTransmission();
      modeEncFlag = 0;
      modeBtnFlag = 0;
      SetMainDisplayData();
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break; 
  }
}
//CABINET ENCODER SWITCH FUNCTIONS for SHORT CLICK FOR ALL MODES
void CheckCabButton(Button2& btn) {
  switch(modeBtnFlag){
    case 0:
      Cab.on = !Cab.on;
      toggle(ADDR_CAB, Cab.on, "1");
      CabOnOff(Cab.on);
      startedTime = currentTime;
      Cab.swFlag = false;
      delay(100);
      break;
    case 1:
      //Amp.selector = Amp.savedName;
      //Amp.volume = Amp.savedVolume;
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break;
    case 2:
      if(btn.getClickType() !=4 && btn.getClickType() != 0){
        SaveCab(Cab.selector);
        //Cab.lastVolume = Cab.savedVolume;
        Cab.volume = Cab.lastVolume;
        SetMainDisplayData();
        delay(100);
        startedTime = currentTime;
        modeEncFlag = 0;
        modeBtnFlag = 0;
        Saturation.encoder.resetPosition(Saturation.gain);
        Amp.encoder.resetPosition(Amp.volume);
        Cab.encoder.resetPosition(Cab.volume);
      }
      break;
    case 3:
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break; 
    case 4:
      break;  
    case 5:
      startedTime=0;
      delay(400);
      //send unmute for dac/adc
      Wire.beginTransmission(0x34);
      Wire.write(0x081C >> 8); //MSB, shift to 8bit
      Wire.write(0x081C & 0xFF); //LSB specify end
      //send 0000000000011100 - 4th bit - dac/adc unmuted
      Wire.write(0x00);
      Wire.write(0x1C);
      Wire.endTransmission(); //end transmission
      modeEncFlag = 0;
      modeBtnFlag = 0;
      SetMainDisplayData();
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break; 
  }
}
//SATURATION/OVERDRIVE ENCODER SWITCH for SHORT CLICK FUNCTIONS FOR ALL MODES
void CheckSatButton(Button2& btn) {
  switch(modeBtnFlag){
    case 0:
      Saturation.on = !Saturation.on;
      toggle(ADDR_OVER, Saturation.on, "1");
      SatOnOff(Saturation.on);
      startedTime = currentTime;
      Saturation.swFlag = false;
      delay(100);
      break;
    case 1:
      //Amp.selector = Amp.savedName;
      Amp.lastVolume = Amp.volume;
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break;
    case 2:
      Cab.lastVolume = Cab.volume;
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break;
    case 3:
      SetMainDisplayData();
      delay(100);
      startedTime = currentTime;
      modeEncFlag = 0;
      modeBtnFlag = 0;
      Saturation.lastGain = Saturation.gain;
      Amp.lastVolume = Amp.volume;
      Cab.lastVolume = Cab.volume;
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break;  
    case 4:
      break; 
    case 5:
      startedTime=0;
      delay(400);
      //send unmute for dac/adc
      Wire.beginTransmission(0x34);
      Wire.write(0x081C >> 8); //MSB, shift to 8bit
      Wire.write(0x081C & 0xFF); //LSB specify end
      //send 0000000000011100 - 4th bit - dac/adc unmuted
      Wire.write(0x00);
      Wire.write(0x1C);
      Wire.endTransmission(); //end transmission
      modeEncFlag = 0;
      modeBtnFlag = 0;
      SetMainDisplayData();
      Saturation.encoder.resetPosition(Saturation.gain);
      Amp.encoder.resetPosition(Amp.volume);
      Cab.encoder.resetPosition(Cab.volume);
      break; 
  }
}
//AMPLIFIER ENCODER SWITCH for LONG CLICK FUNCTIONS FOR ALL MODES
void CheckAmpButtonLong(Button2& btn) {
    if(Saturation.button.isPressed()){
      modeEncFlag = 4;
      modeBtnFlag = 4;
      muteAll();
    }
  switch(modeBtnFlag){
    case 0:
      modeEncFlag = 1;
      modeBtnFlag = 1;
      //Amp.savedVolume = Amp.lastVolume;
      AmpSelector(amplifierName[Amp.selector], "Bass Amplifier");
      Amp.encoder.resetPosition(Amp.selector);
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break; 
    case 4:
      break;  
    case 5:
      break; 
  }
}
//CABINET ENCODER SWITCH for LONG CLICK FUNCTIONS FOR ALL MODES
void CheckCabButtonLong(Button2& btn) {
  switch(modeBtnFlag){
    case 0:
      modeEncFlag = 2;
      modeBtnFlag = 2;
      //Cab.savedVolume = Cab.lastVolume;
      CabSelector(cabinetName[Cab.savedName], "INTERNAL");
      Cab.encoder.resetPosition(Cab.selector);
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;   
    case 4:
      break;
    case 5:
      break; 
  }
}
//SATURATION/OVERDRIVE ENCODER SWITCH for LONG CLICK FUNCTIONS FOR ALL MODES
void CheckSatButtonLong(Button2& btn) {
  switch(modeBtnFlag){
    case 0:
      Saturation.savedGain = Saturation.gain;
      SetEQ();
      modeEncFlag = 3;
      modeBtnFlag = 3;
      Amp.encoder.resetPosition(Amp.low);
      Cab.encoder.resetPosition(Cab.high);
      Saturation.encoder.resetPosition(Saturation.mid);
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;  
    case 4:
      break; 
    case 5:
      break; 
  }
}

//UI FUNCTIONS:
//MAIN UI VIEW
void SetMainDisplayData() {  //MAIN UI VIEW FUNCTION
  display.clearDisplay();
  display.setRotation(0);
  display.setFont();
  display.drawRoundRect(0,0,22,10,2, 1);
  display.setTextSize(1);
  display.fillRect(1,1,20,8,0);
  display.fillRect(1,1,20,8,Amp.on);
  display.setTextColor(Amp.on ? 0 : 1);
  digitalWrite(PIN_AMP_LED, Amp.on ? HIGH : LOW);
  display.setCursor(3,1);
  display.print("Amp");
  display.setTextColor(1);
  display.setCursor(25,1);
  display.print(amplifierName[Amp.selector].substring(0, 6));
  display.drawLine(23,9,61,9,WHITE);

  //Cab On/Off Section
  display.drawRoundRect(64,0,22,10,2,1);
  display.fillRect(65,1,20,8,0);

  display.fillRect(65,1,20,8,Cab.on);
  display.setTextColor(Cab.on ? 0 : 1);
  digitalWrite(PIN_CAB_LED, Cab.on ? HIGH : LOW);

  display.setCursor(67,1);
  display.print("Cab");
  display.setTextColor(1);
  display.setCursor(89,1);
  display.print(cabinetName[Cab.selector].substring(0, 6));
  display.drawLine(87,9,126,9,WHITE);

  display.setCursor(18,13);
  display.print("Amplifier Volume");
  display.drawRoundRect(0,22,128,9,2,WHITE);

  //Overdrive On/Off section
  display.drawRoundRect(60,32,68,9,2,1);
  display.drawRoundRect(49,32,9,9,2,Saturation.on);
  display.fillRect(50,33,7,7,Saturation.on);
  digitalWrite(PIN_OVER_LED, Saturation.on ? HIGH: LOW);
  display.setCursor(0,33);
  display.print("saturate");
  display.setTextColor(1);
  display.setCursor(22,46);
  display.print("Cabinet Volume");
  display.drawRoundRect(0,55,128,9,2,WHITE);
  display.fillRect(1,23,Amp.volume*2,7,WHITE);
  display.fillRect(1,56,Cab.volume*2,7,WHITE);
  display.fillRect(61,33,Saturation.gain*1,7,WHITE);
  display.display();
}

//EQ VIEW:
void SetEQ(){
  display.clearDisplay();
  display.setFont();
  display.setRotation(3);
  display.setTextSize(1);
  display.setTextColor(1);
  
  display.setCursor(24,8);
  display.print("LOW");
  display.drawRoundRect(0,22,64,9,2,WHITE);
  display.drawLine(32, 18, 32, 21, WHITE);
  display.drawLine(32, 31, 32, 34, WHITE);
  display.setCursor(24,47);
  display.print("MID");
  display.drawRoundRect(0,62,64,9,2,WHITE);
  display.drawLine(32, 58, 32, 61, WHITE);
  display.drawLine(32, 71, 32, 74, WHITE);
  display.setCursor(20,90);
  display.print("HIGH");
  display.drawRoundRect(0,105,64,9,2,WHITE);
  display.drawLine(32, 102, 32, 104, WHITE);
  display.drawLine(32, 113, 32, 116, WHITE);

  if (Amp.low >= 0){
    display.fillRect(32,23,Amp.low,7,WHITE);
  }
  else if (Amp.low < 0)
  {
    int l = abs(Amp.low);
    display.fillRect(32-l,23,l+1,7,WHITE);
  }

  if (Saturation.mid>= 0){
    display.fillRect(32,63,Saturation.mid,7,WHITE);
  }
  else if (Saturation.mid < 0)
  {
    int m = abs(Saturation.mid);
    display.fillRect(32-m,63,m+1,7,WHITE);
  }
  
  if (Cab.high>= 0){
    display.fillRect(32,106,Cab.high,7,WHITE);
  }
  else if (Cab.high < 0)
  {
    int h = abs(Cab.high);
    display.fillRect(32-h,106,h+1,7,WHITE);
  }
  
  display.display();
}
//FREQURENCY GAIN CHANGES VIEW
void ChangeFreq(int value, String freq){
  
  if (freq == "LOW"){
    display.fillRect(1,23,62, 7,0);
    if (value >= 0){
      display.fillRect(32,23,value,7,WHITE);
    }
    else if (value < 0)
    {
      int l = abs(value);
      display.fillRect(32-l,23,l+1,7,WHITE);
       
    } 
  }
  if (freq == "MID"){
    display.fillRect(1,63,62,7,0);
    if (value>= 0){
        display.fillRect(32,63,value,7,WHITE);
      }
      else if (value < 0)
      {
        int m = abs(value);
        display.fillRect(32-m,63,m+1,7,WHITE);
      } 
  }
  if (freq == "HIGH"){
    display.fillRect(1,106,62,7,0);
    if (value >= 0){
      display.fillRect(32,106,value,7,WHITE);
    }
    else if (value < 0)
    {
      int h = abs(value);
      display.fillRect(32-h,106,h+1,7,WHITE);
    } 
  }
   display.display();
}

//AMPLIFIER ON/OFF VIEW
void AmpOnOff(bool state){  //ENABLE/DISABLE SELECTED AMPLIFIER UI
  display.setTextSize(1);
  display.fillRect(1,1,20,8,state);
  display.setTextColor(state ? 0 : 1);
  digitalWrite(PIN_AMP_LED, state ? HIGH : LOW);
  display.setCursor(3,1);
  display.print("Amp");
  display.display();
}

//CABINET ON/OFF VIEW
void CabOnOff(bool state){ //ENABLE/DISABLE SELECTED CABINET UI
  display.setTextSize(1);
  display.fillRect(65,1,20,8,state);
  display.setTextColor(state ? 0 : 1);
  digitalWrite(PIN_CAB_LED, state ? HIGH: LOW);
  display.setCursor(67,1);
  display.print("Cab");
  display.display();
}
//EQ ON/OFF VIEW
void SatOnOff(bool state){ //ENABLE/DISABLE OVERDRIVE/SATURATION UI
  display.setTextSize(1);
  display.drawRoundRect(49,32,9,9,2,state);
  display.fillRect(50,33,7,7,state);
  digitalWrite(PIN_OVER_LED, state ? HIGH : LOW);
  display.display();
}

//AMPLIFIER SELECTION UI FUNCTION
void AmpSelector(String ampName, String ampType){  
  display.clearDisplay();
  display.setFont();
  display.setCursor(6,25);
  display.setTextColor(1);
  display.print(ampType);
  display.setFont(&FreeSansBold9pt7b);
  display.fillRect(0,0,128,20,1);
  display.setCursor(5,15);
  display.setTextColor(0);
  display.print("AMPLIFIER:");
  display.setCursor(5,55);
  display.setTextColor(1);
  display.print(ampName);
  display.display();
  if(low(PIN_OVER_BTN) && enableSelector){  //Back Function from selector mode
    enableSelector = false;
    Saturation.swFlag = false;
    SetMainDisplayData();
  }
}

//Cabinet SELECTION UI FUNCTION
void CabSelector(String cabName, String cabSource){  
  display.clearDisplay();
  display.setFont(&Picopixel);
  display.setCursor(3,27);
  display.setTextColor(1);
  display.print(cabSource);
  display.setFont(&FreeSansBold9pt7b);
  display.fillRect(0,0,128,20,1);
  display.setCursor(1,15);
  display.setTextColor(0);
  display.print("IR-Cabinet:");
  display.setFont();
  display.setCursor(3,40);
  display.setTextColor(1);
  if(cabName.length()>=20){
    display.println(cabName.substring(0, 20));
  }else{
    display.print(cabName);
  }
  display.display();
  if(low(PIN_OVER_BTN) && enableSelector){ //Back Function from selector mode
    enableSelector = false;
    Saturation.swFlag = false;
    SetMainDisplayData();
  }
}

//AMPLIFIER VOLUME UPDATER UI FUNCTION
void DisplayVolume(int value, String name, int pos){   
  display.clearDisplay();
  display.setFont();
  display.setCursor(pos,2);
  display.setTextColor(1);
  display.print(name);
  display.fillRect(0,12,128,52,0);
  display.fillRect(0,12,value*2,52,1);
  display.display();
}

//FIRST SCREEN FOR SELECTOR UI VIEW
void EnableSelectorOptions(){ 
  display.clearDisplay();
  display.setFont(&Picopixel);
  display.setTextSize(1);
  display.setTextColor(1);
  display.setCursor(53,7);
  display.print("ROTATE");
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(15,24);
  display.print("AMPLIFIER");
  display.setFont(&TomThumb);
  display.setCursor(60,34);
  display.print("OR");
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(25,50);
  display.print("CABINET");
  display.setFont(&Picopixel);
  display.setCursor(21,61);
  display.print("KNOB FOR SETTINGS CHANGE");
  //display.print("Rotate \nAMPLIFIER \n\n     or \n\n CABINET \n\nknob for change your settings.");
  display.display();
  exitFlag = true;
  delay(1000);
  Amp.savedVolume = Amp.volume;
  Cab.savedVolume = Cab.volume;
  Amp.savedName = Amp.selector;
  Cab.savedName = Cab.selector;

  while(true){
    if(low(35)){
      startedTime=0;
      AmpSelector(amplifierName[Amp.selector], "Bass Amplifier");
      delay(300);
      break;
    }
    if(low(16)){
      startedTime=0;
      CabSelector(cabinetName[Cab.selector], "INTERNAL");
      delay(300);
      break;
    }
    if(low(PIN_OVER_BTN)){
      startedTime=0;
      enableSelector = false;
      Saturation.swFlag = false;
      delay(300);
      SetMainDisplayData();
      break;
    }
  }
  return;
}

//BACK-END, COMMUNICATION, CONVERSION/CALCULATE FUNCTIONS 
//Convert from wav to fir-table.(NOT USED IN CURRENT RELEASE)
void IRConverter(String fileName){ 
  File irFile = FFat.open("/"+ fileName, "r");
  fileName.replace(".wav", ".fir");
  String localFileName = fileName;

  File baseFile = FFat.open("/"+ localFileName, "w");
  if(!irFile){
    Serial.println("Failed to open file for reading");
    return;
  }

  uint32_t irArray[865];
  int reading = 0;

  Serial.println("File Content:");
  uint8_t buff[4];
  irFile.seek(36);
  irFile.read(buff, sizeof(buff));
  String irHeader;
  String irData;

  for (int i=0;i<sizeof(buff); i++){
    irHeader += String(buff[i], HEX);
  }
    Serial.println(irHeader);
    if (irHeader == "64617461"){
      Serial.println("IR File is OK");
      uint8_t bytes[3];
      irFile.seek(44);

      for (int i=0; i<875; i++){

        irData = "";
        irFile.read(bytes, sizeof(bytes));
        //uint8_t new_bytes[3] = [bytes[2], bytes[1], bytes[0]];
        uint32_t new_bytes = (bytes[2] << 16) | (bytes[1] << 8 ) | bytes[0];

        //Serial.println(new_bytes);
        if(new_bytes > 0x800000){
          new_bytes = new_bytes | 0xff000000;
        }
        else{ new_bytes = new_bytes | 0x00000000; }

        //Serial.println(new_bytes);
        uint32_t bytes = new_bytes >> 24;
        baseFile.write(bytes & 0xff);
        bytes = new_bytes >> 16;
        baseFile.write(bytes & 0xff);
        bytes = new_bytes >> 8;
        baseFile.write(bytes & 0xff);
        bytes = new_bytes;
        baseFile.write(bytes);

//        for (int i=0;i<sizeof(bytes); i++){
//          String lengthCheck = String(bytes[i], HEX);
//          if (lengthCheck.length() < 2){
//            lengthCheck = '0' + lengthCheck;
//          }
//          irData = lengthCheck+irData;
//        }
//        Serial.println(irData);
      }

    }
    else{Serial.println("IR File not supported!");}

    irFile.close();
    baseFile.close();

  return;
}

//READ CONVERTED FIR-FILE from local storage (NOT USED IN CURRENT RELEASE)
void ReadIR(String localFile){ 
  File irLocalFile = FFat.open("/" + localFile, "r");

  Serial.print("File Size: ");
  Serial.println(irLocalFile.size());

  while(irLocalFile.available()){
    for (int i = 0; i<3; i++){
      irLocalFile.read();
    }
    irLocalFile.read();
  }
return;
}

//SET CABINET FUNCTION:
void SaveCab(int selector){
  switch (selector) {
            case 0:
              SendIR(EfremoffBassCabinet);
              break;
            case 1:
              SendIR(EfremoffHartkeFourTen);
              break;
            case 2:
              SendIR(EfremoffAmpegEightTen);
              break;
            case 3:
              SendIR(EfremoffMesaTwoTwelve);
              break;
            default:
              SendIR(EfremoffBassCabinet);
              break;
          }
}
void SaveLastCab(int selector){
  switch (selector) {
            case 0:
              SendStartupIR(EfremoffBassCabinet);
              break;
            case 1:
              SendStartupIR(EfremoffHartkeFourTen);
              break;
            case 2:
              SendStartupIR(EfremoffAmpegEightTen);
              break;
            case 3:
              SendStartupIR(EfremoffMesaTwoTwelve);
              break;
            default:
              SendStartupIR(EfremoffBassCabinet);
              break;
          }
}

//SEND SELECTED FIR-TABLE (or external variable with table) to DSP-PROCESSOR
void SendIR(float IR[]){  
  display.clearDisplay();
  display.setFont();
  display.setCursor(25,30);
  display.print("Loading IR...");
  display.display();
  //MUTE DAC:
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000010100 - 4th bit - dac muted
  Wire.write(0x00);
  Wire.write(0x14);
  Wire.endTransmission();

  uint16_t counter = 0x00;
  int irsAddress = 84;
  for(int i = 0; i <= 860; i++)
  {
    sendBytes(0x34, irsAddress, IR[i], "1");
    irsAddress++;
  }
  
  display.clearDisplay();
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(25,35);
  display.print("LOADED!");
  display.display();
  //UNMUTE DAC:
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000011100 - 4th bit - dac/adc unmuted
  Wire.write(0x00);
  Wire.write(0x1C);
  Wire.endTransmission();
  delay(500);

  return;
}
//SEND SELECTED FIR-TABLE (or external variable with table) to DSP-PROCESSOR on STARTUP (no UI):
void SendStartupIR(float IR[]){  
  //MUTE DAC:
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000010100 - 4th bit - dac muted
  Wire.write(0x00);
  Wire.write(0x14);
  Wire.endTransmission();

  uint16_t counter = 0x00;
  int irsAddress = 84;
  for(int i = 0; i <= 860; i++)
  {
    sendBytes(0x34, irsAddress, IR[i], "1");
    irsAddress++;
  }
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000011100 - 4th bit - dac/adc unmuted
  Wire.write(0x00);
  Wire.write(0x1C);
  Wire.endTransmission();
  Serial.println("Startup IR Loaded");
  return;
}

//SEND DISABLE/ENABLE DAC to DSP-PROCESSOR (MUTE/UNMUTE):
void muteAll(){  
  display.clearDisplay();
  display.setRotation(0);
  display.setFont(&FreeSansBold18pt7b);
  display.drawRoundRect(0,0,128,64,3, 1);
  display.setCursor(0,42);
  display.print("MUTED");
  display.display();

  //send mute for dac/adc
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000010100 - 4th bit - dac muted
  
  Wire.write(0x00);
  Wire.write(0x14);
  Wire.endTransmission(); //end transmission
  Serial.println("Muted!");
  digitalWrite(PIN_CAB_LED, LOW);
  digitalWrite(PIN_AMP_LED, LOW);
  digitalWrite(PIN_OVER_LED, LOW);
  delay(1000);
  while(true){
    if (!Amp.button.isPressedRaw()){
      if(!Saturation.button.isPressedRaw()){
        Serial.println("Buttons realised!");
        modeEncFlag = 5;
        modeBtnFlag = 5;
        break;
      }
    }
  }
  return;
}

//SEND SPECIFIED VALUE TO DSP-PROC to CORRESPOND REGISTER using SAFELOAD Registers. 
void sendBytes(uint8_t adauAddr, uint16_t paramAddr,  float value, String mod){  
    int shamanConstant = 8388608;
    float bigValue = value*shamanConstant;
    uint32_t hexValue = (int) bigValue;
    Wire.beginTransmission(adauAddr);  //Send SafeLoad data parameter value.
    Wire.write(0x0810 >> 8);  //SafeLoad Data 1 byte
    Wire.write(0x0810 & 0xFF); //SafeLoad Data 2 byte
    Wire.write(0x00);   //SafeLoad Data 3 byte
    if (value < 0){
      Wire.write(0xFF);
    }
    else if(value <=2){
      Wire.write(0x00);
    }
    else {
      Wire.write((hexValue & 0xFF000000) >> 24);
    }
    Wire.write((hexValue & 0xFF0000) >> 16);
    Wire.write((hexValue & 0xFF00) >> 8);
    Wire.write((hexValue & 0xFF));
    Wire.endTransmission();
    Wire.beginTransmission(adauAddr); // Send Safeload target address
    Wire.write(0x0815 >> 8);  //SafeLoad Address 1 byte
    Wire.write(0x0815 & 0xFF);  //SafeLoad Address 2 byte
    Wire.write(paramAddr >> 8); //MSB, shift to 8bit
    Wire.write(paramAddr & 0xFF); //LSB specify end
    Wire.endTransmission();
    if (mod == "STARTUP"){
      Wire.beginTransmission(adauAddr); //Initiate SafeLoad
      Wire.write(0x081C >> 8); //IST register 1byte
      Wire.write(0x081C & 0xFF);  //IST register 2byte
      Wire.write(0x00);  //IST register value 1byte
      Wire.write(0x34);  //IST register value 2byte
      Wire.endTransmission();
      
    }
    else{
      Wire.beginTransmission(adauAddr); //Initiate SafeLoad
      Wire.write(0x081C >> 8); //IST register 1byte
      Wire.write(0x081C & 0xFF);  //IST register 2byte
      Wire.write(0x00);  //IST register value 1byte
      Wire.write(0x3C);  //IST register value 2byte
      Wire.endTransmission();
    }
    return;
}


//COMMON SERVICE FUNCTION FOR CHECK AVAILABLE I2C Devices (NOT USED BY DEFAULT)
void i2cScanner(){  

  byte error, address; //variable for error and I2C address
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmission to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");


  return;
}

//Common service function which show list of files on SD-CARD (NOT USED BY DEFAULT)
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){  
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  String fileName = "";

  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
      fileName = file.name();
      fileName.replace("/", "");
      cabinetName[Cab.counter] = fileName;
    }
    file = root.openNextFile();
    Cab.counter++;
  }
  Serial.println(Cab.counter);
}
