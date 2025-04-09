ifeq ($(KERNELRELEASE),)  

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)  

.PHONY: build clean all

all: build sneaky_process

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules  

sneaky_process: sneaky_process.c
	gcc -o sneaky_process sneaky_process.c

clean:
	rm -rf *.o *~ core .depend .*.cmd *.order *.symvers *.ko *.mod.c 
	rm -f sneaky_process
else  

$(info Building with KERNELRELEASE = ${KERNELRELEASE}) 
obj-m :=    sneaky_mod.o  

endif
