/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2010-2011 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include "nanddrv.h"
#include "nandsim.h"


#define BUF_SIZE	(2048 + 64)
char buf[BUF_SIZE];

int main(int argc, char *argv[])
{
	struct nand_chip * chip;

	chip = nandsim_init("nand-em-file", 256, 64, 2048, 64);

	nanddrv_read(chip, 4, 0, BUF_SIZE, buf);
	return 0;
}
