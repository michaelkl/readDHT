CFLAGS=-std=gnu99 -O2 -Wall #-DDEBUG
INCLUDE=-I../gpio_lib_A20

readDHT: ../gpio_lib_A20/gpio_lib.o readDHT.c
	gcc $(CFLAGS) $(INCLUDE) -lrt -o $@ ../gpio_lib_A20/gpio_lib.o readDHT.c

#gpio_lib.o: ../gpio_lib/gpio_lib.h ../gpio_lib/gpio_lib.c 
#	gcc $(CFLAGS) $(INCLUDE) -c -o $@ ../gpio_lib/gpio_lib.c

clean:
#	rm *.o
	rm readDHT

