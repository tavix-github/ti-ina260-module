
KERNEL_BUILD := /home/tavix/work/src-design/linux-xlnx-zynq-v2018.3

obj-m += ti_ina260.o

all:
	make -C $(KERNEL_BUILD) M=$(shell pwd) modules
	
clean:
	make -C $(KERNEL_BUILD) M=$(shell pwd) clean
