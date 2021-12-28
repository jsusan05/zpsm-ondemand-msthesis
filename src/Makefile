obj-m := zpsmapmod.o zpsmclientmod.o

zpsmapmod-objs := ap/zpsm_ap_mod.o ap/zpsm_queue_mod.o ap/zpsm_send_mod.o common/zpsm_kern_debug.o
zpsmclientmod-objs := client/zpsm_client_mod.o client/zpsm_send_mod.o common/zpsm_kern_debug.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: 
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o zpsm_ap ap/zpsm_app.c ap/serialsend.c common/zpsm_debug.c
	gcc -o zpsm_client client/zpsm_client.c client/seriallisten.c common/zpsm_debug.c -lrt -lm -lpthread	
	rm *.o *.symvers *.order *.mod.c
clean: 
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm zpsm_ap
	rm zpsm_client
