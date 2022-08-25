# RP2040_M6N_parser
Parser for standard NMEA 0183 messages, focused on use cases involving the RP2040

# Hardware Connections
- UART1 Utilized to communicate with the NEO-M6N/other GPS module
- Make sure to provide external 3V3 as the base Pi Pico cannot source enough current and will brown out
- USB connection (Serial over USB) from RP2040 board to host PC/MCU/whatever else

# Included Features
- Serial messaging over USB connection to PC
- Lat, Lon, Time broken out into uint/int/float where applicable
- Fix and Connected Sats broken out into uint32 status value
- Easy modification to strip print features and just use as inline functions or convert to general use library
- string -> uint32_t conversion functions that are very portable

# Unfinished Features
- Fixed Point Support (working on it slowly)
