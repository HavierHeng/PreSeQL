#ifndef CRC_ALGORITHM_H
#define CRC_ALGORITHM_H

#include <stdint.h>
#include <unistd.h>

uint32_t calculate_crc32(const void *data, size_t length);

#endif
