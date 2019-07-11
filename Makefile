PORT=/dev/ttyUSB0
MCU=atmega2560
CFLAGS=-g -Wall -mcall-prologues -mmcu=$(MCU) -Os -DAVR
LDFLAGS=-Wl,-gc-sections -Wl,-relax
CC=avr-gcc
TARGET=main
OBJECT_FILES=main.o

all: 	$(TARGET).hex
	sudo avrdude -p atmega2560 -c STK500 -P /dev/ttyUSB0 -U flash:w:main.hex
#	sudo cat /home/manu/Dokumente/Code/tool_mySmartUSB-Kommandos_de_en_fr/BoardPowerOn.txt > /dev/ttyUSB0
	sudo sh -c 'cat /home/manu/Dokumente/Code/tool_mySmartUSB-Kommandos_de_en_fr/BoardPowerOn.txt > /dev/ttyUSB0'
	
clean:
	rm -f *.o *.hex *.obj *.hex

%.hex: %.obj
	avr-objcopy -R .eeprom -O ihex $< $@
	

%.obj: $(OBJECT_FILES)
	$(CC) $(CFLAGS) $(OBJECT_FILES) $(LDFLAGS) -o $@

program: $(TARGET).hex
	sudo avrdude -p m2560 -c STK500 -P /dev/ttyUSB0 -U flash:w:main.hex
	sudo -c 'cat /home/manu/Dokumente/Code/tool_mySmartUSB-Kommandos_de_en_fr/BoardPowerOn.txt > /dev/ttyUSB0'
