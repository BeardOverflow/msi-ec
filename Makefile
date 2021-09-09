obj-m += msi-ec.o

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
load:
	insmod msi-ec.ko
unload:
	-rmmod msi-ec
install:
	cp msi-ec.ko /lib/modules/$(shell uname -r)/extra
	depmod -a
	echo msi-ec > /etc/modules-load.d/msi-ec.conf
	modprobe -v msi-ec

uninstall:
	-modprobe -rv msi-ec
	rm -f /lib/modules/$(shell uname -r)/extra/msi-ec.ko
	depmod -a
	rm -f /etc/modules-load.d/msi-ec.conf

dev: modules unload load
