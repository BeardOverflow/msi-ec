VERSION         := 0.08
DKMS_ROOT_PATH  := /usr/src/msi_ec-$(VERSION)

ccflags-y := -std=gnu11 -Wno-declaration-after-statement

obj-m += msi-ec.o


all: modules

modules:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules

clean:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean

load:
	insmod msi-ec.ko

load-debug:
	insmod msi-ec.ko debug=1

unload:
	-rmmod msi-ec

reload: unload load

reload-debug: unload load-debug

install:
	mkdir -p /lib/modules/$(shell uname -r)/extra
	cp msi-ec.ko /lib/modules/$(shell uname -r)/extra
	depmod -a
	echo msi-ec > /etc/modules-load.d/msi-ec.conf
	modprobe -v msi-ec

uninstall:
	-modprobe -rv msi-ec
	rm -f /lib/modules/$(shell uname -r)/extra/msi-ec.ko
	depmod -a
	rm -f /etc/modules-load.d/msi-ec.conf

dkms-install:
	dkms --version >> /dev/null
	mkdir -p $(DKMS_ROOT_PATH)
	cp $(CURDIR)/dkms.conf $(DKMS_ROOT_PATH)
	cp $(CURDIR)/Makefile $(DKMS_ROOT_PATH)
	cp $(CURDIR)/msi-ec.c $(DKMS_ROOT_PATH)
	cp $(CURDIR)/ec_memory_configuration.h $(DKMS_ROOT_PATH)

	sed -e "s/@CFLGS@/${MCFLAGS}/" \
	    -e "s/@VERSION@/$(VERSION)/" \
	    -i $(DKMS_ROOT_PATH)/dkms.conf

	dkms add msi_ec/$(VERSION)
	dkms build msi_ec/$(VERSION)
	dkms install msi_ec/$(VERSION)
	echo msi-ec > /etc/modules-load.d/msi-ec.conf

dkms-uninstall:
	dkms remove msi_ec/$(VERSION) --all
	rm -rf $(DKMS_ROOT_PATH)
	rm -f /etc/modules-load.d/msi-ec.conf

dev: modules unload load
