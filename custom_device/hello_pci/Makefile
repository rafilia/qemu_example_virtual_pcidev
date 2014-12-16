obj-m := hello_pci.o

all:
	make -C ../linux-kernel M=$(PWD) modules

clean:
	make -C ../linux-kernel M=$(PWD) clean
