CC=avr-gcc
CFLAGS=-g -Os -Wall -I../include -I../mixer -mcall-prologues -mmcu=attiny85 -DF_CPU=8000000UL
OBJ2HEX=avr-objcopy 
TARGET=ledtree

program: $(TARGET).hex 
	sudo avrdude -p t85 -P usb -c avrispmkII -Uflash:w:$(TARGET).hex 

fuses:
	sudo avrdude -p t85 -P usb -c avrispmkII -U lfuse:w:0xe2:m -U hfuse:w:0xd7:m -U efuse:w:0xFF:m -B 8

$(TARGET).hex: $(TARGET).obj
	$(OBJ2HEX) -R .eeprom -O ihex $< $@

$(TARGET).obj: $(TARGET).o
	$(CC) $(CFLAGS) -o $@ -Wl,-Map,$(TARGET).map $(TARGET).o 

clean:
	rm -f *.hex *.obj *.o *.map
