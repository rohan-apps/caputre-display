#ifndef __IO_H__
#define __IO_H__

#include <stddef.h>
#include <stdint.h>

#define BIT(nr)                         (1UL << (nr))
#define setbit(_a, _bit)	        ((_a) |= (1 << _bit))
#define clrbit(_a, _bit)	        ((_a) &= ~(1 << _bit))
#define readl(_a)                       (*(volatile uint32_t *)(_a))
#define writel(_v, _a)	                (*(volatile uint32_t *)(_a) = (_v))

#define _maskbit(_p, _w)              	(((1 << _w) - 1) << _p)
#define _getbits(_i, _p, _w)       	(((_i) & _maskbit(_p, _w)) >> _p)
#define _setbits(_i, _p, _w, _v)        (((_i) & (uint32_t)~_maskbit(_p, _w)) | \
				        (uint32_t)(((_v) << _p) & _maskbit(_p, _w)))
#define writel_bits(_v, _addr, _p, _w)	writel(_setbits(readl(_addr), _p, _w, _v), _addr)
#define readl_bits(_addr, _p, _w)	_getbits(readl(_addr), _p, _w)



#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#endif
