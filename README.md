# CabAmpSimulator_ADAU1401

This project is a base and guitar amplifier simulator + IR based cabinet simulator. Uses ADAU1401 Digital Signal Processor and ARM-microcontroller (ex. Raspberry Pi Pico). 

## Functionality:

v.0.1 (actual)
- Output amplifier emulator (bass 6L6 'overdrived' SVT style). 
- IR-Based cabinet simulator.
- Two push/rotary encoders for Volume and on/off control
- Balanced output (XLR-socket)
- Unbalanced input
- Standard 9V power supply. 

v.0.2 (not released yet, new features only).
- Load third-party IR (.wav files) via microSD card. 
- i2c OLED screen (1.3 inch) for UI (select and load IR from list of files on sd-card, visual presentation of volumes, loaded IR, selected amplifier emulator
- additional 4 types of output amplifier (one for bass, three for guitar) - EL34(more clean then 6L6), 6L6 guitar, 6V6 Clean, EL84 Crunched.
- Additional toggle switch for enable/disable 'overdrived' function. 

## Project contain:

- Sprint Layout 6.0 PCB (PCB folder)
- MicroPython Script with nessesary functions and code for comunication between user actions and DSP for raspberry pi pico, with specified additional libs. (see readme in Python folder)
- SigmaStudio 4.6 project with demo, based on bass amplifier (SVT4PRO style) and FIR-Table based on own bass cabinet. (Project folder)
