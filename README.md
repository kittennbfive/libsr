# libsr
A small helper for processing [Sigrok](https://sigrok.org) `.sr`-files in C. Uses [libzip](https://libzip.org). Useful for fast processing of big (or small) captures with custom code.

## Licence and Disclaimer
This code is provided under AGPLv3+ and WITHOUT ANY WARRANTY! It is an early release and only sparsely tested.

## Public API
```
#define LENGTH_NAME_MAX 20

void sr_open(char const * const filename);
uint_fast8_t sr_get_nb_channels(void);
uint_fast8_t sr_get_channel_bitpos(char const * const name);
uint64_t sr_get_nb_samples(void);
bool sr_get_sample(const uint_fast8_t pos_channel, const uint64_t sample);
void sr_close(void);
```

`LENGTH_NAME_MAX` is the maximum length of the name of each channel (excluding the '\0'). 20 characters is plenty but you can always change this and recompile.

The functions should be pretty self-explanatory i guess? Please call `sr_close()` once you're done to free internal memory.

## How to compile
You will need libzip, on Debian 11 the package is called `libzip-dev`.  
Compile with something like `gcc -Wall -Wextra -Werror -O3 -o your_tool libsr.c main.c -lzip` (adjust as needed, the important bit is the `-lzip` at the end)

## Limitations
The current version will copy all the uncompressed (sr==zip basically) samples in memory, so you need enough RAM for big captures. I did this for speed, i might write a version that uses less RAM at some point.  
The code works for **digital** channels only and up 2**64 bytes of captured data total. That should be enough i think...  
If something goes wrong (for example if you call `sr_get_channel_bitpos()` with an invalid name, ...) the code will exit directly using `errx()`. This makes it non-suitable for stuff like GUI-applications.

## Example
Here is a quick and dirty parallel bus-decoder (rising edge on CLK, data 8 bits):
```
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "libsr.h"

int main(int argc, char ** argv)
{
	(void)argc;
	
	sr_open(argv[1]);
	
	uint_fast8_t channel_CLK;
	channel_CLK=sr_get_channel_bitpos("CLK");
	
	//caution: MSB/LSB?
	uint_fast8_t channel_DATA[8];
	channel_DATA[0]=sr_get_channel_bitpos("D0");
	channel_DATA[1]=sr_get_channel_bitpos("D1");
	channel_DATA[2]=sr_get_channel_bitpos("D2");
	channel_DATA[3]=sr_get_channel_bitpos("D3");
	channel_DATA[4]=sr_get_channel_bitpos("D4");
	channel_DATA[5]=sr_get_channel_bitpos("D5");
	channel_DATA[6]=sr_get_channel_bitpos("D6");
	channel_DATA[7]=sr_get_channel_bitpos("D7");
	
	uint64_t nb_samples=sr_get_nb_samples();
	uint64_t sample;
	bool clk, clk_old=0;
	uint_fast8_t bitcnt=0;
	uint_fast8_t i;
	uint8_t byte;
	
	for(sample=0; sample<nb_samples; sample++)
	{
		clk=sr_get_sample(channel_CLK, sample);
		
		if(clk && !clk_old)
		{
			if(++bitcnt==8)
			{
				bitcnt=0;
				byte=0;
				for(i=0; i<8; i++)
					byte|=sr_get_sample(channel_DATA[i], sample)<<i;
				printf("%02X\n", byte);
			}
		}
		clk_old=clk;
	}
	
	sr_close();
	
	return 0;
}
```
