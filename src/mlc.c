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

static struct mlc_base __mlc[NUMBER_OF_MLC_MODULE] = {
	[0] = { (const void *)PHY_BASEADDR_MLC0, NULL },
	[1] = { (const void *)PHY_BASEADDR_MLC1, NULL },
};

void hw_reg_dump(int module, struct mlc_reg *mlc)
{
	struct mlc_reg *reg;

	assert(__mlc[module].virt);
	reg = (struct mlc_reg *)__mlc[module].virt;

	memcpy(mlc, reg, sizeof(struct mlc_reg));
}

int hw_reg_get_module_num(void)
{
	return NUMBER_OF_MLC_MODULE;
}

int hw_reg_get_layer_num(int module)
{
	return NUMBER_OF_MLC_LAYER;
}

unsigned int hw_reg_get_length(int module)
{
	if (module >= NUMBER_OF_MLC_MODULE)
		return -EINVAL;

	return sizeof(struct mlc_reg);
}

const void *hw_reg_get_base(int module)
{
	if (module >= NUMBER_OF_MLC_MODULE)
		return NULL;

	return __mlc[module].phys;
}

int hw_reg_set_base(int module, void *base)
{
	if (module >= NUMBER_OF_MLC_MODULE)
		return -EINVAL;

	__mlc[module].virt = (struct __mlc *)base;

	return 0;
}
