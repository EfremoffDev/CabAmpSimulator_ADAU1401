//ALL LIBS INCLUDE
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "graphics.h"
#include <ESP32Encoder.h>
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
const String amplifierName [] = {"SVT Style", "SWR Style", "EBS Style", "Mark Bass Style"}; //DEFATUL AMPLIFIERS LIST
String cabinetName [100] = {"2x10 Bass Cab byEfremoff", "4x10 Hartke XL", "8x10 Ampeg SVT HFL", "2x12 Mesa Subway"}; //DEFATUL CABS LIST


// PINS
#define PIN_AMP_SWITCH 34
#define PIN_AMP_LED 27
#define PIN_CAB_SWITCH 39
#define PIN_CAB_LED 14
#define PIN_OVER_LED 12
#define PIN_OVER_BTN 36
#define PIN_WP 33

// Addresses
#define ADDR_CAB_VOL 0x0052
#define ADDR_AMP_VOL 0x003C
#define ADDR_CAB 0x03B4
#define ADDR_AMP 0x0050
#define ADDR_OVER 0x004E


struct AmpStruct {
    int selector = 0;
    int volume;
    bool swFlag = false;
    bool on = true;
    int lastVolume = -1;
    int savedVolume = 0;
    int savedName = 0;
    ESP32Encoder encoder;
} Amp;

struct CabStruct {
    int selector = 0;
    int counter = 3;
    int savedVolume = 0;
    int savedName = 0;
    int volume = 0;
    int lastVolume = -1;
    bool swFlag = false;
    bool on = true;
    ESP32Encoder encoder;
} Cab;

struct OverStruct {
    bool swFlag = false;
    bool on = false;
} Over;

// ALL VARIABLES/FLAGS and other STUFF

bool exitFlag = false;
bool enableSelector = false;
bool holdedFlag = false;
unsigned long startedTime = 0;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void toggle(uint16_t addr, bool state){
    sendBytes(0x34, addr, state ? 0 : 1);
    sendBytes(0x34, addr + 1, state ? 1 : 0);
}

bool high(uint16_t addr){
    return digitalRead(addr) == HIGH;
}

bool low(uint16_t addr){
    return digitalRead(addr) == LOW;
}

void AmpVolume(int vol){
    float sendVolume = powf(1.017, Amp.volume) - 1;
    sendBytes(0x34, ADDR_AMP_VOL, sendVolume);
}

void CabVolume(int vol){
  float StartupCabVolume = powf(1.003, Cab.volume) - 1;
  sendBytes(0x34, ADDR_CAB_VOL, StartupCabVolume);
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
  ESP32Encoder::useInternalWeakPullResistors = NONE;

  delay(500); //Basic delay before start setup and draw UI

  //SETUP INPUTS/OUTPUTS:
  pinMode(PIN_WP, OUTPUT);
  digitalWrite(PIN_WP, HIGH);
  pinMode(PIN_AMP_SWITCH, INPUT);
  pinMode(PIN_CAB_SWITCH, INPUT);
  pinMode(PIN_CAB_LED, OUTPUT);
  pinMode(PIN_AMP_LED, OUTPUT);
  pinMode(PIN_OVER_LED, OUTPUT);
  pinMode(PIN_OVER_BTN, INPUT);

  //READ SAVED DATA FROM EEPROM:
  Amp.volume = EEPROM.read(10);
  Amp.on = (bool) EEPROM.read(11);
  Amp.selector = EEPROM.read(15);
  Amp.lastVolume = Amp.volume;

  Cab.volume = EEPROM.read(12);
  Cab.on = (bool) EEPROM.read(13);
  Cab.selector = EEPROM.read(16);
  Cab.lastVolume = Cab.volume;

  Over.on = (bool) EEPROM.read(14);

  Amp.encoder.attachSingleEdge(35, 32);
  Cab.encoder.attachSingleEdge(16, 17);

  //SET LAST SAVED PARAMETERS in DSP-PROC: DOESN'T WORK FOR NOW, DON'T KNOW WHY.
  AmpVolume(Amp.volume);
  CabVolume(Cab.volume);

  toggle(ADDR_AMP, Amp.on);
  toggle(ADDR_CAB, Cab.on);
  toggle(ADDR_OVER, Over.on);

  //SET ENCODER STATES:
  Amp.encoder.setCount(Amp.volume);
  Cab.encoder.setCount(Cab.volume);

  Serial.println("Start Graphics");
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
  display.print("v.1.0.1");
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

  delay(1000); //DELAY FOR STARTUP VIEW

  //SET INFORMATION VIEW OF LOADED AMPLIFIER AND CABINET:
  display.clearDisplay();
  Serial.println("Device is ready");
  AmpSelector(amplifierName[Amp.selector], "Bass Amplifier");
  delay(1000);
  CabSelector(cabinetName[Cab.selector], "INTERNAL");
  delay(1000);
  //SET MAIN DISPLAY UI Function:
  SetMainDisplayData();
}


void loop() {
   //MAIN LOOP FUNCITON
  unsigned long currentTime = millis();

  //WORKING in MAIN DISPLAY UI
  //CHECK AMPLIFIER VOLUME ENCODER:
  Amp.volume = Amp.encoder.getCount();
  if (Amp.volume >= 0 && Amp.volume <= 64 && high(PIN_AMP_SWITCH) && !enableSelector){
      if (Amp.volume != Amp.lastVolume){
        DisplayAmplifierVolume(Amp.volume);
        AmpVolume(Amp.volume);
        Amp.lastVolume = Amp.volume;
        startedTime = currentTime;
      }
  }
  else{
    if (Amp.volume < 0){
      Amp.encoder.setCount(0);
    }
    if (Amp.volume > 64){
      Amp.encoder.setCount(64);
    }
  }
  //CHECK CABINET VOLUME ENCODER:
  Cab.volume = Cab.encoder.getCount();
  if (Cab.volume >= 0 && Cab.volume <= 64 && high(PIN_CAB_SWITCH) && !enableSelector){
      if (Cab.volume != Cab.lastVolume){
        DisplayCabinetVolume(Cab.volume);
        CabVolume(Cab.volume);
        Cab.lastVolume = Cab.volume;
        startedTime = currentTime;
      }
  }
  else{
    if (Cab.volume < 0){
      Cab.encoder.setCount(0);
    }
    if (Cab.volume > 64){
      Cab.encoder.setCount(64);
    }
  }

  //working in Selector Mode (IF enableSelector flag == 1)
  //select amplifier
    if (Amp.volume >= 0 && Amp.volume <= 64 && enableSelector){
        if (Amp.volume != Amp.lastVolume){
            if (Amp.volume < Amp.lastVolume){
                Amp.selector = Amp.selector > 0 ? Amp.selector - 1 : 0;
            }else{
                Amp.selector = Amp.selector < 3 ? Amp.selector + 1 : 0;
            }
            AmpSelector(amplifierName[Amp.selector], "Bass Amplifier");
            Amp.lastVolume = Amp.volume;
            delay(50); //small debounce delay
        }
    }

    //select IR-Cabinet
    if (Cab.volume >= 0 && Cab.volume <= 64 && enableSelector){
        if (Cab.volume != Cab.lastVolume){
            if (Cab.volume < Cab.lastVolume){
                Cab.selector = Cab.selector > 0 ? Cab.selector - 1 : 0;
            }else{
                Cab.selector = Cab.selector < Cab.counter ? Cab.selector + 1 : Cab.counter;
            }
            if(Cab.selector > 3){
              CabSelector(cabinetName[Cab.selector], "EXTERNAL - SD-CARD"); //SOMETIMES SHOWED WHEN INTERNAL CAB USED.
            }else{
              CabSelector(cabinetName[Cab.selector], "INTERNAL");
            }
            Cab.lastVolume = Cab.volume;
            delay(50); //small debounce delay
        }
    }

  //Working with buttons (Generally is a 3 buttons in project - PIN_AMP_SWITCH, cabSwith and PIN_OVER_BTN)
  //Enable/Disable amplifier in Main Display UI:
    if (low(PIN_AMP_SWITCH) && !Amp.swFlag && !enableSelector){
          Amp.swFlag = true;
          delay(10);
    }
    if (high(PIN_AMP_SWITCH) && Amp.swFlag && !enableSelector){
        Amp.on = !Amp.on;
        toggle(ADDR_AMP, Amp.on);
        AmpOnOff(Amp.on);
        startedTime = currentTime;
        Amp.swFlag = false;
        delay(100);
    }
    //Enable/Disable Cabinet in Main Display UI:
    if (low(PIN_CAB_SWITCH) && !Cab.swFlag && !enableSelector){
        Cab.swFlag = true;
        delay(10);
  }
    if (high(PIN_CAB_SWITCH) && Cab.swFlag && !enableSelector){
        Cab.on = !Cab.on;
        toggle(ADDR_CAB, Cab.on);
        CabOnOff(Cab.on);
        startedTime = currentTime;
        Cab.swFlag = false;
        delay(100);
    }
  //Enable/Disable overdrive/saturaton in Main Display UI:
    if (low(PIN_OVER_BTN) && !Over.swFlag && !exitFlag){
        Over.swFlag = true;
        delay(100);
  }
    if (high(PIN_OVER_BTN) && Over.swFlag && !exitFlag){
        enableSelector = false;
        Over.on = !Over.on;
        toggle(ADDR_OVER, Over.on);
        OverOnOff(Over.on);
        startedTime = currentTime;
        Over.swFlag = false;
        Cab.swFlag = false;
        delay(100);
    }

  //WORKING IN SELECTOR MODE
  //Load selected ampifier (loading amplifier not realized for now, this statement only works like "back" function for Selector in selectAmpifier mode (start main UI).
    if (low(PIN_AMP_SWITCH) && !Amp.swFlag && enableSelector){
        Amp.swFlag = true;
        delay(10);
    }
    if (high(PIN_AMP_SWITCH) && Amp.swFlag && enableSelector){
        enableSelector = false;
        Amp.swFlag = false;
        exitFlag = false;
        Amp.encoder.setCount(Amp.savedVolume);
        Amp.lastVolume = Amp.savedVolume;
        Amp.volume = Amp.savedVolume;
        SetMainDisplayData();
        delay(100);
        startedTime = currentTime;
    }

  //LOAD SELECTED CABINET ON button pressed and released.
    if (low(PIN_CAB_SWITCH) && !Cab.swFlag && enableSelector){
        Cab.swFlag = true;
        delay(10);
    }
    if (high(PIN_CAB_SWITCH) && Cab.swFlag && enableSelector){
        enableSelector = false;
        exitFlag = false;
        Cab.swFlag = false;
        switch (Cab.selector) {
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
        Cab.encoder.setCount(Cab.savedVolume);
        Cab.lastVolume = Cab.savedVolume;
        Cab.volume = Cab.savedVolume;
        SetMainDisplayData();
        delay(100);
        startedTime = currentTime;
    }

  //GO TO DIFFERENT MODES BY BUTTONS HOLD:
  //ENABLE MUTE: (press and hold PIN_CAB_SWITCH and PIN_AMP_SWITCH more then 2 sec)
    if (low(PIN_CAB_SWITCH) && low(PIN_AMP_SWITCH) && !holdedFlag){
      startedTime = currentTime;
      Cab.swFlag = false;
      Amp.swFlag = false;
      holdedFlag = true;
    }
    if (low(PIN_CAB_SWITCH) && low(PIN_AMP_SWITCH) && holdedFlag){
      if (startedTime && currentTime > startedTime + 2000){
        holdedFlag = false;
        Cab.swFlag = false;
        Amp.swFlag = false;
        muteAll(); //MUTE ENABLED
      }
    }
  //GO TO SELECTOR MODE (press and hold PIN_OVER_BTN over 2 sec)
    if (low(PIN_OVER_BTN) && !enableSelector){
      startedTime = currentTime;
      Over.swFlag = false;
      enableSelector = true;

    }
    if (low(PIN_OVER_BTN) && enableSelector && !exitFlag){
      if (startedTime && currentTime > startedTime + 2000){
        Serial.println("GotoSelector!");
        Over.swFlag = false;
        EnableSelectorOptions();
      }
    }
  //BACK TO MAIN VIEW FROM SELECTOR VIEW without saving data:
    if (low(PIN_OVER_BTN) && enableSelector && exitFlag){
      enableSelector = false;
      Over.swFlag = false;
      exitFlag = false;
      Amp.selector = Amp.savedName;
      Cab.selector = Cab.savedName;
      SetMainDisplayData();
      delay(300);
    }

  //AUTOSAVE DATA IN EEPROM ONE TIME, 2sec later, after USER INTERACTION
    if (startedTime && currentTime > startedTime + 2000){
      EEPROM.write(10, Amp.volume);
      EEPROM.write(11, Amp.on);
      EEPROM.write(12, Cab.volume);
      EEPROM.write(13, Cab.on);
      EEPROM.write(14, Over.on);
      EEPROM.write(15, Amp.selector);
      EEPROM.write(16, Cab.selector);
      EEPROM.commit();
      Serial.println("EEPROM DATA WRITED:");
      Serial.println(EEPROM.read(10));
      Serial.println(EEPROM.read(11));
      Serial.println(EEPROM.read(12));
      Serial.println(EEPROM.read(13));
      Serial.println(EEPROM.read(14));
      Serial.println(EEPROM.read(15));
      Serial.println(EEPROM.read(16));
      exitFlag = false;
      enableSelector = false;
      SetMainDisplayData();
      startedTime=0;
  }
}
//END OF MAIN LOOP

//COMMON FUNCTIONS:

//UI FUNCTIONS:
void SetMainDisplayData() {  //MAIN UI VIEW FUNCTION
  display.clearDisplay();
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
  display.drawRoundRect(28,30,72,10,2,1);

  display.fillRect(29,31,70,8, Over.on);
  display.setTextColor(Over.on ? 0 : 1);
  digitalWrite(PIN_OVER_LED, Over.on ? HIGH: LOW);

  display.setCursor(34,31);
  display.print("saturation");
  display.setTextColor(1);
  display.setCursor(22,46);
  display.print("Cabinet Volume");
  display.drawRoundRect(0,55,128,9,2,WHITE);
  display.fillRect(1,23,Amp.volume*2,7,WHITE);
  display.fillRect(1,56,Cab.volume*2,7,WHITE);
  display.display();
}

void AmpOnOff(bool state){  //ENABLE/DISABLE SELECTED AMPLIFIER UI
  display.setTextSize(1);
  display.fillRect(1,1,20,8,state);
  display.setTextColor(state ? 0 : 1);
  digitalWrite(PIN_AMP_LED, state ? HIGH : LOW);
  display.setCursor(3,1);
  display.print("Amp");
  display.display();
}

void CabOnOff(bool state){ //ENABLE/DISABLE SELECTED CABINET UI
  display.setTextSize(1);
  display.fillRect(65,1,20,8,state);
  display.setTextColor(state ? 0 : 1);
  digitalWrite(PIN_CAB_LED, state ? HIGH: LOW);
  display.setCursor(67,1);
  display.print("Cab");
  display.display();
}

void OverOnOff(bool state){ //ENABLE/DISABLE OVERDRIVE/SATURATION UI
  display.setTextSize(1);
    display.fillRect(29,31,70,8, state);
    display.setTextColor(state ? 0 : 1);
    digitalWrite(PIN_OVER_LED, state ? HIGH : LOW);
  display.setCursor(34,31);
  display.print("saturation");
  display.display();
}

void AmpSelector(String ampName, String ampType){  //AMPLIFIER SELECTION UI FUNCTION
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
    Over.swFlag = false;
    SetMainDisplayData();
  }
}

void CabSelector(String cabName, String cabSource){  //Cabinet SELECTION UI FUNCTION
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
    Over.swFlag = false;
    SetMainDisplayData();
  }
}

void DisplayAmplifierVolume(int ampVol){   //AMPLIFIER VOLUME UPDATER UI FUNCTION
  display.clearDisplay();
  display.setFont();
  display.setCursor(15,2);
  display.setTextColor(1);
  display.print("Amplifier Volume:");
  display.fillRect(0,12,128,52,0);
  display.fillRect(0,12,ampVol*2,52,1);
  display.display();
}
void DisplayCabinetVolume(int cabVol){ //CABINET VOLUME UPDATER UI FUNCTION
  display.clearDisplay();
  display.setFont();
  display.setCursor(20,2);
  display.setTextColor(1);
  display.print("Cabinet Volume:");
  display.fillRect(0,12,128,52,0);
  display.fillRect(0,12,cabVol*2,52,1);
  display.display();
}

void EnableSelectorOptions(){ //FIRST SCREEN FOR SELECTOR UI VIEW
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
      Over.swFlag = false;
      delay(300);
      SetMainDisplayData();
      break;
    }
  }
  return;
}

//BACK-END, COMMUNICATION, CONVERTATION FUNCTIONS
void IRConverter(String fileName){ //Convert from wav to fir-table. Not used for now.
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

// XXX: [jack] What does this method do? Doesn't look like much
void ReadIR(String localFile){ //READ CONVERTED FIR-FILE from local storage
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

void SendIR(float IR[]){  //SEND SELECTED FIR-TABLE (or external variable with table) to DSP-PROCESSOR
  Serial.println("Start send IR");
  Serial.print("IR Array Size: 864");
  display.clearDisplay();
  display.setFont();
  display.setCursor(25,30);
  display.print("Loading IR...");
  //display.drawRoundRect(0,30,128,9,2,WHITE);
  display.display();
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000010100 - 4th bit - dac muted
  Wire.write(0x00);
  Wire.write(0x14);
  Wire.endTransmission();

  uint16_t counter = 0x00;
  int irsAddress = 83;
  for(int i = 0; i <= 864; i++)
  {
    sendBytes(0x34, irsAddress, IR[i]);
    irsAddress++;
//    display.fillRect(1,31,i/6.75,7,WHITE);
//    if(i % 200 == 0){
//      display.display();
//    }
  }
  
  display.clearDisplay();
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(25,35);
  display.print("LOADED!");
  display.display();
  Wire.beginTransmission(0x34);
  Wire.write(0x081C >> 8); //MSB, shift to 8bit
  Wire.write(0x081C & 0xFF); //LSB specify end
  //send 0000000000011100 - 4th bit - dac/adc unmuted
  Wire.write(0x00);
  Wire.write(0x1C);
  Wire.endTransmission();
  delay(800);

  return;
}

void muteAll(){  //SEND DISABLE/ENABLE DAC to DSP-PROCESSOR (MUTE/UNMUTE)
  display.clearDisplay();
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
    if(low(PIN_CAB_SWITCH) || low(PIN_AMP_SWITCH)){
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
      SetMainDisplayData();
      break;
      }
    }
  return;
}


void sendBytes(uint8_t adauAddr, uint16_t paramAddr,  float value){  //SEND SPECIFIED VALUE TO DSP-PROC to CORRESPOND REGISTER
    Serial.println(value);
    Serial.println(paramAddr);
    Serial.println(paramAddr, HEX);
    //int hexArray = [];
    int X0,X1,X2,X3;
    int shamanConstant = 8388608;
    float bigValue = value*shamanConstant;
    uint32_t hexValue = (int) bigValue;
    Wire.beginTransmission(adauAddr);
    Wire.write(paramAddr >> 8); //MSB, shift to 8bit
    Wire.write(paramAddr & 0xFF); //LSB specify end
    if (value >= 0){
      Wire.write(0x00);
    }
    else{
      Wire.write(0xFF);
    }
    Wire.write((hexValue & 0xFF0000) >> 16);
    Wire.write((hexValue & 0xFF00) >> 8);
    Wire.write((hexValue & 0xFF));
    Wire.endTransmission(); //end transmission

    return;
}



void i2cScanner(){  //COMMON FUNCTION (NOT USED BY DEFAULT) FOR CHECK AVAILABLE I2C Devices

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

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){  //Common (Not used by default) function which show list of files on SD-CARD
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
