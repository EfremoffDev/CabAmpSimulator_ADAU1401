# CabAmpSimulator_ADAU1401

This project is a base and guitar amplifier simulator + IR based cabinet simulator. Uses ADAU1401 Digital Signal Processor and ARM-microcontroller (ex. Raspberry Pi Pico or ESP32 (prefered)). 

## Functionality:

v.1.0.1 (old) - Only Raspberry Pi Pico on micropython
- Output amplifier emulator (bass 6L6 'overdrived' SVT style). 
- IR-Based cabinet simulator.
- Two push/rotary encoders for Volume and on/off control
- Balanced output (XLR-socket)
- Unbalanced input
- Standard 9V power supply. 

__v.1.0.2 (actual). - ONLY ESP32 Arduino IDE project.__
- Load third-party IR (.wav files) via microSD card. 
- i2c OLED screen (1.3 inch) for UI (select and load IR from list of files on sd-card, visual presentation of volumes, loaded IR, selected amplifier emulator
- additional 8 types of output amplifier (4 for bass, 4 for guitar)
- Additional encoder with toggle switch for enable/disable 'output saturation' function. 
- Mute function
- Setup of EQ for selected amplifier.

## Project contain:

- Sprint Layout 6.0 PCB (PCB folder) for Raspberry Pi Pico project.
- MicroPython Script with nessesary functions and code for comunication between user actions and DSP for raspberry pi pico, with specified additional libs. (see readme in Python folder)
- __ESP32 project with Arduino IDE - Arduino Folder__
- __Gerber files for PCB production for ESP32 project - (PCB -> ESP32 folder).__
- SigmaStudio 4.6 project with demo, based on bass amplifier (SVT4PRO style) and FIR-Table based on own bass cabinet. (Project folder)
-
## ESP32 Public Project

- For some purpose ESP32 will be more flexible and more reiteratable for this project. For this purpose created a open project on EasyEDA/OSHWLab:
- https://oshwlab.com/Aiefremov/adau_main_board_gainta_bs13
