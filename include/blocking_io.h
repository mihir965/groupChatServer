#ifndef BLOCKING_IO_H
#define BLOCKING_IO_H

#include <stddef.h>

int bio_init(const char *path, size_t blob_size);

void bio_read_4k(void);

void bio_close(void);

#endif
