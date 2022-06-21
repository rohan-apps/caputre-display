#ifndef __IO_MAP_H__
#define __IO MAP_H__

#define	IO_MMAP_DEVICE          "/dev/mem"
#define	IO_MMAP_ALIGN           (4096)
#define MAP_ALIGN_ADDR(p)       (p & ~((IO_MMAP_ALIGN) - 1))
#define MAP_ALIGN_SIZE(s)       ((s & ~((IO_MMAP_ALIGN) - 1)) + IO_MMAP_ALIGN)

void *iomem_map(const void *addr, size_t length, void *map_addr);
void iomem_free(void *addr, size_t length);

#endif
