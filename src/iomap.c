#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "iomap.h"

void *iomem_map(const void *addr, size_t length, void *mapped)
{
	void *mem;
	size_t physical = (size_t)addr;
	int fd;

	fd = open(IO_MMAP_DEVICE, O_RDWR | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "Fail open %s", IO_MMAP_DEVICE);
		perror(" - erro");
		return 0;
	}

	physical = MAP_ALIGN_ADDR(physical);
	length = MAP_ALIGN_SIZE(length);

	mem = mmap((void *)0, length,
		   PROT_READ | PROT_WRITE, MAP_SHARED,
		   fd, (off_t)physical);
	if (mem == MAP_FAILED) {
		fprintf(stderr, "Fail map addr 0x%x length %d",
			physical, length);
		perror(" - erro");
		close(fd);
		return mem;
	}

	if (mapped)
		mapped = mem;

	close(fd);

	return mem + ((size_t)addr - physical);
}

void iomem_free(void *addr, size_t length)
{
	if (addr && length)
		munmap(addr, length);
}
