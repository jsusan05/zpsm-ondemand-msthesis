obj-m := zpsmclientmod.o

zpsmclientmod-objs := zpsm_client_mod.o zpsm_send_mod.o ../common/zpsm_kern_debug.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: 
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o zpsm_client zpsm_client.c seriallisten.c ../common/zpsm_debug.c -lm -lpthread	
	rm *.o *.symvers *.order *.mod.c
clean: 
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm zpsm_client
