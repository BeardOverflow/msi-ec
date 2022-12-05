VERSION         := 0.08
DKMS_ROOT_PATH  := /usr/src/msi_ec-$(VERSION)

obj-m += msi-ec.o


all: modules

modules:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules

regkeys:
	cat << EOF > x509-configuration.ini
	[ req ]
	default_bits = 4096
	distinguished_name = req_distinguished_name
	prompt = no
	string_mask = utf8only
	x509_extensions = myexts

	[ req_distinguished_name ]
	O = msiec
	CN = msiec
	emailAddress = emailAddress

	[ myexts ]
	basicConstraints=critical,CA:FALSE
	keyUsage=digitalSignature
	subjectKeyIdentifier=hash
	authorityKeyIdentifier=keyid
	EOF

	openssl req -x509 -new -nodes -utf8 -sha256 -days 36500 -batch -config x509-configuration.ini -outform DER -out public_key.der -keyout private_key.priv
	echo "The command prompts you to enter and confirm a password for the MOK enrollment request. You can use any password for this purpose(once only)"
	echo "1.Enroll key 2.Continue 3.password 4.continue to boot."
	mokutil --import public_key.der
signkeys:
	mkdir -p /lib/modules/$(shell uname -r)/extra
	cp msi-ec.ko /lib/modules/$(shell uname -r)/extra
	depmod -a
	echo msi-ec > /etc/modules-load.d/msi-ec.conf
	sudo /usr/src/kernels/$(uname -r)/scripts/sign-file sha256 ~/private_key.priv  ~/public_key.der msi-ec.ko

	

clean:
	@$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean

load:
	insmod msi-ec.ko

unload:
	-rmmod msi-ec

install:
	mkdir -p /lib/modules/$(shell uname -r)/extra
	cp msi-ec.ko /lib/modules/$(shell uname -r)/extra
	depmod -a
	echo msi-ec > /etc/modules-load.d/msi-ec.conf
	perl /usr/src/kernels/$(uname -r)/scripts/sign-file sha256 ~/private_key.priv  ~/public_key.der msi-ec.ko
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
	cp $(CURDIR)/constants.h $(DKMS_ROOT_PATH)

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
