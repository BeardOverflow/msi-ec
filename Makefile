obj-m += msi-ec.o

all: clean modules unload load

dev: modules unload load

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
load:
	insmod msi-ec.ko
unload:
	-rmmod msi-ec

