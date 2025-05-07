#include "blocking_io.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int blob_fd = -1;
static size_t blob_len = 0;
static char scratch[4096];

int bio_init(const char *path, size_t bytes) {
	blob_fd = open(path, O_RDWR | O_CREAT, 0644);
	if (blob_fd < 0)
		return -1;

	off_t sz = lseek(blob_fd, 0, SEEK_END);
	if (sz < (off_t)bytes) {
		int rnd = open("/dev/urandom", O_RDONLY);
		char buf[4096];
		while (sz < (off_t)bytes) {
			if (read(rnd, buf, sizeof buf) != sizeof buf)
				break;
			if (write(blob_fd, buf, sizeof buf) != sizeof buf) {
				perror("blob write");
				break;
			}
			sz += sizeof buf;
		}
		close(rnd);
	}
	blob_len = (size_t)sz;
	srand((unsigned)time(NULL));
	return 0;
}

void bio_read_4k(void) {
	if (blob_fd < 0 || blob_len == 0)
		return;
	off_t off = (rand() % (blob_len / 4096)) * 4096;
	pread(blob_fd, scratch, sizeof scratch, off);
}

void bio_close(void) {
	if (blob_fd >= 0)
		close(blob_fd);
}
