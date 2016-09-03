//
// Created by asus on 2016/8/6.
//


#include <stddef.h>
#include "globals.h"
#include <string.h>
#include "checksum.h"

unsigned int adler32(char *data, size_t len) /* data: Pointer to the data to be summed; len is in bytes */
{

    unsigned int a = 1, b = 0;
    /* Loop over each byte of data, in order */
    for (size_t index = 0; index < len; ++index) {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}