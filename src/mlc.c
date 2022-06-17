#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "mlc.h"
#include "io.h"

struct mlc_base {
       const void *phys;
       void *virt;
};

struct mlc_base __mlc[NUMBER_OF_MLC_MODULE] = {
        [0] = { (const void*)PHY_BASEADDR_MLC0, NULL },
        [1] = { (const void*)PHY_BASEADDR_MLC1, NULL },
};

void hw_mlc_dump(int dev, struct mlc_reg *mlc)
{
        struct mlc_reg *reg;

        assert(__mlc[dev].virt);
        reg = (struct mlc_reg *)__mlc[dev].virt;

        memcpy(mlc, reg, sizeof(struct mlc_reg));
}

void hw_mlc_save(int dev, struct mlc_reg *mlc)
{
        assert(__mlc[dev].virt);
}

unsigned int hw_mlc_get_size(int dev)
{
        if (dev >= NUMBER_OF_MLC_MODULE)
                return 0;
        return sizeof(struct mlc_reg);
}

const void *hw_mlc_get_base(int dev)
{
        if (dev >= NUMBER_OF_MLC_MODULE)
                return NULL;

        return __mlc[dev].phys;
}

int hw_mlc_set_base(int dev, void *base)
{
        if (dev >= NUMBER_OF_MLC_MODULE)
                return -EINVAL;

        __mlc[dev].virt = (struct __mlc*)base;
        return 0;
}


