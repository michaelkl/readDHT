/*
 Reads data from DHT11/DHT21/DHT22/AM2302 temperature-humidity sensor.
 It is for Banana Pi M1 single-board computer !ONLY!

 It uses 'gpio_lib_A20' library by akarnaukh (https://github.com/akarnaukh)
 which is based on 'gpio_lib' library by Stefan Mavrodiev.

 (c) 2015 Michael Klimenko
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "gpio_lib.h"

#define DHT11 11
#define DHT22 22
#define AM2302 22

// Returns sunxi pin numbers for BananaPi M1 IO pins
// (see http://hardware-libre.fr/2014/07/banana-pi-gpio-now-supported)
const int IO_PIN[] = {
  SUNXI_GPI(19),  // BananaPI IO-0
  SUNXI_GPH( 2),  // BananaPI IO-1
  SUNXI_GPI(18),  // BananaPI IO-2
  SUNXI_GPI(17),  // BananaPI IO-3
  SUNXI_GPH(20),  // BananaPI IO-4
  SUNXI_GPH(21),  // BananaPI IO-5
  SUNXI_GPI(16)   // BananaPI IO-6
};

void readDHT(int type, int pin);
void throw(int error_code);

int main(int argc, char **argv)
{
   sunxi_gpio_init();

   int type = 0;
   int dhtpin = 0;
   if (argc != 3) {
      printf("usage: %s [11|21|22|2302] BPI-M1-GPIO-PIN#\n", argv[0]);
      printf("example: %s 2302 4 - Read from an AM2302 connected to GPIO #4\n", argv[0]);
      return 2;
   } else {
      if (strcmp(argv[1], "11") == 0) type = DHT11;
      if (strcmp(argv[1], "21") == 0) type = DHT22;
      if (strcmp(argv[1], "22") == 0) type = DHT22;
      if (strcmp(argv[1], "2302") == 0) type = AM2302;
      if (type == 0) {
         printf("Select 11, 21, 22, 2303 as type!\n");
         return 3;
      }
      dhtpin = atoi(argv[2]);
   }

   if (dhtpin < 0 || dhtpin > 6) {
      printf("Please select a valid GPIO pin #\n");
      return 3;
   }

#ifdef DEBUG
   printf("Using pin #%d\n", dhtpin);
#endif

//   readDHT(type, SUNXI_GPI(19));
   readDHT(type, IO_PIN[dhtpin]);

   sunxi_gpio_cleanup();

   return 0;
} // main

/////////////////////////////////////

void throw(int error_code)
{
   switch (error_code) {
   case 1:
      puts("No response from sensor\n");
      break;
   case 2:
      puts("Bit transmission-start level not found\n");
      break;
   case 3:
      puts("Bit data level not found\n");
      break;
   case 4:
      puts("Checksum error\n");
      break;
   }
   exit(error_code);
}

/////////////////////////////////////

void readDHT(int type, int pin) {
   // Set GPIO pin to output
   sunxi_gpio_set_cfgpin(pin, SUNXI_GPIO_OUTPUT);
   // Send start signal
   sunxi_gpio_output(pin, LOW);
   usleep(500);
   sunxi_gpio_output(pin, HIGH);
//   usleep(5);
  
   // Set GPIO pin to intput
   sunxi_gpio_set_cfgpin(pin, SUNXI_GPIO_INPUT);
  
   // Skip host's HIGH level
   const long MAX_COUNTER = 2000;
   long counter = 0;
   while(sunxi_gpio_input(pin)==HIGH) if (counter++>MAX_COUNTER) throw(1);
   // Sensor pulls down 80us
   counter = 0;
   while(sunxi_gpio_input(pin)==LOW) if (counter++>MAX_COUNTER) throw(1);
   // Sensor pulls up 80us
   counter = 0;
   while(sunxi_gpio_input(pin)==HIGH) { if (counter++>MAX_COUNTER) throw(1); }
  
   // Read data
   const int BITS_COUNT = 40;
   long data2[BITS_COUNT];
   for(int idx=0; idx<BITS_COUNT; idx++) data2[idx]=0;

   for (int idx=0; idx<BITS_COUNT; idx++) {
      // skip LOW (start transmit of 1 bit)
      counter = 0;
      while (sunxi_gpio_input(pin)==LOW) if (counter++>MAX_COUNTER) throw(2);
      // count HIGH (data)
      counter = 0;
      while (sunxi_gpio_input(pin)==HIGH) {
         if (counter++>MAX_COUNTER) {
#ifdef DEBUG
            printf("idx=%d, counter overloaded\n",idx);
#endif
            throw(3);
         }
      }
      data2[idx] = counter;
   }
  
   // find average duration on 1's and 0's
   long min_dur = data2[0];
   long max_dur = data2[0];
   for (int idx=1; idx<BITS_COUNT; idx++) {
      if (data2[idx] > max_dur) max_dur = data2[idx];
      if (data2[idx] < min_dur) min_dur = data2[idx];
   }
   long dur_boundary = (max_dur+min_dur)/2;

#ifdef DEBUG
   printf("dur_boundary=%ld\n", dur_boundary);
   for (int idx=0; idx<8; idx++)   printf("%d ",data2[idx]>dur_boundary?1:0);
   printf("- RH\n");
   for (int idx=8; idx<16; idx++)  printf("%d ",data2[idx]>dur_boundary?1:0);
   printf("- RH\n");
   for (int idx=16; idx<24; idx++) printf("%d ",data2[idx]>dur_boundary?1:0);
   printf("- T\n");
   for (int idx=24; idx<32; idx++) printf("%d ",data2[idx]>dur_boundary?1:0);
   printf("- T\n");
   for (int idx=32; idx<40; idx++) printf("%d ",data2[idx]>dur_boundary?1:0);
   printf(" - checksum\n");
#endif

   // Convert byte-array to bit-array
   unsigned char data[5] = {0,0,0,0,0};
   for(int bytenum=0; bytenum<5; bytenum++)
      for(int bitnum=0; bitnum<8; bitnum++)
         data[bytenum] |= 
            ((data2[bitnum+8*bytenum]>dur_boundary?1:0) << (7-bitnum));

#ifdef DEBUG
   printf("Data: %d %d %d %d %d\n", data[0],data[1],data[2],data[3],data[4]);
#endif
 
   unsigned char checksum;
   checksum  = data[0];
   checksum += data[1];
   checksum += data[2];
   checksum += data[3];
   
   float temp=0.0;
   float hum=0.0;

   // checksum control
   if (checksum != data[4]) { // checksum error
      throw(4);
   } else { // checksum ok
      switch(type) {
      case 21:
      case 22:
         // calculate temperature
         temp = (data[2] & 0b01111111);
         temp *= 256;
         temp += data[3];
         temp /= 10;
         if (data[2] & 0b10000000) temp = -temp;
         
         // calculate humidity
         hum = data[0];
         hum *= 256;
         hum += data[1];
         hum /= 10;
         break;
      case 11:
         temp = data[2];
         hum = data[0];
         break;
      }
   }
   
   printf("%.1f %.1f\n", temp, hum);

   return;
}

