obj-m := test_pci_driver.o

all:
	make -C ../linux-kernel M=$(PWD) modules

clean:
	make -C ../linux-kernel M=$(PWD) clean
