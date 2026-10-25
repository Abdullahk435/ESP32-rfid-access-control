
# ESP32-Based RFID Access Control and Audit Logger

An RFID-based access control system simulated using ESP32 and Wokwi. The system checks RFID card UIDs and controls a servo-based door lock.

## Project Status

Simulation prototype — tested features are documented below.
Physical hardware implementation and validation are future work.

## Features

- RFID card identification using MFRC522
- Authorized and unauthorized card handling
- Servo-based door-lock simulation
- OLED status display
- Green and red LED indicators
- Buzzer alerts
- Failed-attempt lockout
- Serial Monitor access logs
- Authorized UID management using ESP32 Preferences


## Hardware Components

- ESP32 DevKit V1
- MFRC522 RFID reader
- RFID cards
- SG90 servo motor
- SSD1306 I2C OLED display
- Green and red LEDs
- Current-limiting resistors
- Buzzer

## Software and Tools

- Arduino C++
- ESP32 Arduino framework
- Wokwi simulator
- SPI communication
- I2C communication
- GitHub

## How to Run

1. Open the project in the Wokwi online simulator.
2. Set up the circuit according to the provided circuit diagram (`diagram.json`).
3. Open the source code (`sketch.ino`) and ensure the required libraries are available.
4. Start the simulation.
5. Scan an authorized or unauthorized RFID card using the virtual RFID reader.
6. Observe the OLED display, LEDs, buzzer, servo motor, and Serial Monitor to verify the system's response.
7. Refer to the source code and circuit diagram to understand the implementation.

**Note:** This project is a Wokwi simulation prototype and has not necessarily been tested on physical hardware.
## Test Results

Add your actual results for:
- Authorized card scan
- Unauthorized card scan
- Servo unlock and relock
- Failed-attempt lockout
- Serial Monitor output

## Security Limitations

This project uses RFID UID-based authorization for demonstration.
UID-only identification is not strong security because some card UIDs can be copied or emulated. Additional authentication and secure administration are required for production use.

## Future Improvements

- Physical hardware implementation
- Stronger card authentication
- Secure administrator access
- Real-time clock for date and time logging
- Web dashboard and IoT monitoring
