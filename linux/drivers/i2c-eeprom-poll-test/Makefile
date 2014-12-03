obj-m = eeprom.o

KERNELSRC = /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNELSRC) M=$(PWD) modules

clean:
	make -C $(KERNELSRC) M=$(PWD) clean
