#ifndef __LIBSR_H__
#define __LIBSR_H__

#include <stdint.h>
#include <stdbool.h>

/*
This file is part of libsr, a small helper for processing Sigrok-captures (.sr) in C

(c) 2023 kittennbfive
https://github.com/kittennbfive/

AGPLv3+ and NO WARRANTY!

Please read the fine manual.

version 0.01
*/

#define LENGTH_NAME_MAX 20

void sr_open(char const * const filename);
uint_fast8_t sr_get_nb_channels(void);
uint_fast8_t sr_get_channel_bitpos(char const * const name);
uint64_t sr_get_nb_samples(void);
bool sr_get_sample(const uint_fast8_t pos_channel, const uint64_t sample);
void sr_close(void);

#endif
