/* Calculates CRC - Uses the ZIP polynomial specified here: https://www.mrob.com/pub/comp/crc-all.html */
#include "crc.h"

/* CRC32 table for checksum calculation */
static uint32_t crc32_table[256];
static int crc32_initialized = 0;

/* Initialize CRC32 table - Based on ZIP Polynomial for 32 bits*/
static void init_crc32_table() {
    if (crc32_initialized) return;
    
    uint32_t polynomial = 0xEDB88320;  // ZIP Coefficient 
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (size_t j = 0; j < 8; j++) {
            if (c & 1) {
                c = polynomial ^ (c >> 1);
            } else {
                c >>= 1;
            }
        }
        crc32_table[i] = c;
    }
    crc32_initialized = 1;
}

/* Calculate CRC32 checksum 
 * CRC works by picking the next piece of an input data (1 byte) and the low N bits of previous CRC value (usually also 1 byte)
 * Shift hre previous 32-bit CRC value by N bits
 * Exclusive OR the polynomial together with the shifted CRC value 
 * Repeat for each block 
 * Then after all blocks processed, XOR the CRC with 0xFFFFFFF (or doing a binary NOT on CRC)
 * */
uint32_t calculate_crc32(const void* data, size_t length) {
    if (!crc32_initialized) init_crc32_table();
    
    const uint8_t* buf = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

