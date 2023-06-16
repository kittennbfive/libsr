#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <zip.h>

#include "libsr.h"

/*
This file is part of libsr, a small helper for processing Sigrok-captures (.sr) in C

(c) 2023 kittennbfive
https://github.com/kittennbfive/

AGPLv3+ and NO WARRANTY!

Please read the fine manual.

version 0.01
*/

typedef struct
{
	uint_fast8_t number;
	char name[LENGTH_NAME_MAX+1];
} channel_t;

static channel_t * channels;
static uint_fast8_t nb_channels_parsed=0;

static uint_fast8_t unitsize=0; //bytes per sample

static zip_t * zip=NULL;

static uint8_t * data=NULL;
static uint64_t bytes_in_buffer_total=0;

static bool get_file_size(char const * const fname, zip_uint64_t * const filesize)
{
	zip_stat_t stat;
	if(zip_stat(zip, fname, 0, &stat))
	{
		zip_error_t * error=zip_get_error(zip);
		if(error->zip_err==ZIP_ER_NOENT)
		{
			(*filesize)=0;
			zip_error_fini(error);
			return false;
		}
		else
			errx(1, "libsr: zip_stat(%s) failed", fname);
	}
	
	if(!(stat.valid&ZIP_STAT_SIZE))
		errx(1, "libsr: zip_stat(%s) did not return a valid filesize", fname);

	(*filesize)=stat.size;

	return true;
}

static bool get_file(char const * const fname, uint8_t * const buffer, zip_uint64_t * const filesize, const bool c_string)
{
	zip_uint64_t fsize;
	
	if(!get_file_size(fname, &fsize))
		return false;
	
	if(filesize)
		(*filesize)=fsize;
	
	zip_file_t * file=zip_fopen(zip, fname, 0);
	if(!file)
		errx(1, "libsr: zip_open(%s) failed", fname);
	
	zip_int64_t bytes_read=zip_fread(file, buffer, fsize);
	if(bytes_read<0 || (zip_uint64_t)bytes_read!=fsize)
		errx(1, "libsr: zip_fread(%s) failed", fname);
	
	if(zip_fclose(file))
		errx(1, "libsr: zip_fclose(%s) failed", fname);
	
	if(c_string)
		buffer[fsize]='\0';
	
	return true;
}

static bool get_logic_file(const uint_fast32_t nb_logic_file, uint8_t * buffer, zip_uint64_t * const filesize)
{
	char name_logic_file[50];
	sprintf(name_logic_file, "logic-1-%lu", nb_logic_file);
	
	return get_file(name_logic_file, buffer, filesize, false);
}

static void check_version(void)
{
	uint8_t * version=NULL;
	zip_uint64_t sz_version;
	
	if(!get_file_size("version", &sz_version))
		errx(1, "libsr: file \"version\" not found");
	
	version=malloc(sz_version*sizeof(uint8_t));
	
	get_file("version", version, NULL, false);
	
	if(sz_version!=1 || version[0]!='2')
		errx(1, "libsr: version %u of sr-format not supported (only version 2 supported)", version[0]);
	
	free(version);
}

static void parse_line(char * l)
{
	if(!strlen(l))
		return;
	
	unsigned int number;
	char name[LENGTH_NAME_MAX+1];
	
	if(sscanf(l, "probe%u=%s", &number, name)==2)
	{
		//that's not pretty but we don't know the max number of channels until the last line (unitsize)
		//the number of channels should be pretty small so this way it should be ok...
		channels=realloc(channels, (nb_channels_parsed+1)*sizeof(channel_t));
		channels[nb_channels_parsed].number=number;
		strncpy(channels[nb_channels_parsed].name, name, LENGTH_NAME_MAX);
		nb_channels_parsed++;
		//printf("parsed channel %s, now %u channels\n", name, nb_channels_parsed);
	}
	else if(sscanf(l, "unitsize=%hhu", &unitsize)==1)
	{
		//printf("unitsize: %u\n", unitsize);
	}
}

static void parse_metadata(void)
{
	char * metadata=NULL;
	zip_uint64_t sz_metadata;
	
	if(!get_file_size("metadata", &sz_metadata))
		errx(1, "libsr: file \"metadata\" not found");
	
	metadata=malloc(sz_metadata*sizeof(uint8_t));
	
	if(!get_file("metadata", (uint8_t*)metadata, NULL, true))
		errx(1, "libsr: could not read metadata");
	
	char * l;
	l=strtok(metadata, "\n");
	parse_line(l);
	while((l=strtok(NULL, "\n")))
		parse_line(l);
	
	free(metadata);
	
	if(unitsize==0)
		errx(1, "libsr: invalid unitsize 0, parser problem?");
}

static void parse_data(void)
{
	char name_logic_file[50];
	uint_fast32_t nb_logic_file=1;
	uint64_t fsize;
	do
	{
		sprintf(name_logic_file, "logic-1-%lu", nb_logic_file);
		if(!get_file_size(name_logic_file, &fsize))
			break;
		
		bytes_in_buffer_total+=fsize;
		nb_logic_file++;
	} while(1);
	
	//printf("bytes_in_buffer_total %lu\n", bytes_in_buffer_total);
	
	data=malloc(bytes_in_buffer_total*sizeof(uint8_t));
	
	uint_fast32_t nb_files=nb_logic_file;
	uint64_t pos=0;
	
	for(nb_logic_file=1; nb_logic_file<nb_files; nb_logic_file++)
	{
		get_logic_file(nb_logic_file, &data[pos], &fsize);
		pos+=fsize;
	}
}


void sr_open(char const * const filename)
{
	int err;
	zip=zip_open(filename, ZIP_RDONLY, &err);
	if(zip==NULL)
		errx(1, "libsr: zip_open(%s) failed with error %d", filename, err); //TODO: is there a way to get a string-representation for err?
	
	check_version();
	
	parse_metadata();
	
	parse_data();
}

uint_fast8_t sr_get_nb_channels(void)
{
	return nb_channels_parsed;
}

uint_fast8_t sr_get_channel_bitpos(char const * const name)
{
	uint_fast8_t i;
	for(i=0; i<nb_channels_parsed; i++)
	{
		if(!strcmp(channels[i].name, name))
			return channels[i].number-1;
	}
	
	errx(1, "libsr: could not find channel %s", name);
}

uint64_t sr_get_nb_samples(void)
{
	return bytes_in_buffer_total/unitsize;
}

bool sr_get_sample(const uint_fast8_t pos_channel, const uint64_t sample)
{
	uint_fast8_t offset=pos_channel/8;
	uint_fast8_t shift=pos_channel%8;
		
	return (data[unitsize*sample+offset]>>shift)&1;
}

void sr_close(void)
{
	free(data);
	free(channels);
}
