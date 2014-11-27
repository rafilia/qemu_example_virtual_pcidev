obj-m := hello.o

all:
	make -C ../linux-kernel M=$(PWD) modules

clean:
	make -C ../linux-kernel M=$(PWD) clean
	
