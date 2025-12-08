##Commands
`avr-gcc -Os -DF_CPU=16000000UL -mmcu=atmega328p -c -o main.o main.c # Compile`
`avr-gcc -mmcu=atmega328p main.o -o main # Link`
`avr-objcopy -O ihex -R .eeprom main main.hex # Load`
`avrdude -F -V -c arduino -p ATMEGA328P -P COM3 -b 57600 -U flash:w:main.hex # Upload`
`avr-objcopy -O ihex -R .eeprom -R .fuse build/output.elf build/output.hex`
`avrdude -F -V -c arduino -p ATMEGA328P -P COM3 -b 57600 -U flash:w:build/output.elf:e`

