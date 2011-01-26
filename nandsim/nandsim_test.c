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
#include <stdlib.h>
#include <string.h>
#include "nanddrv.h"
#include "nandsim_file.h"


#define BUF_SIZE	(2048 + 64)
unsigned char buffer0[BUF_SIZE];
unsigned char buffer1[BUF_SIZE];

static void print_buf(unsigned char *buf, int n)
{
	while(n > 0){
		printf("[%02X]",*buf);
		buf++;
		n--;
	}
	printf("\n");
}

static void verify_page(struct nand_chip *chip, int page, unsigned char val)
{
	int i;
	int ok;

	nanddrv_read(chip, page, 0, buffer0, BUF_SIZE);
	print_buf(buffer0,20);
	ok = 1;
	for(i = 0; i < BUF_SIZE && ok; i++){
		if (buffer0[i]!= val){
			printf("Buffer not %02X at %d\n",val,i);
			ok = 0;
		}
	}
	printf("Read of page %d was %s\n", page, ok ? "ok" : "bad");
}

static void randomise_buffer(unsigned char *buffer)
{
	int i;
	for(i = 0; i < BUF_SIZE; i++)
		buffer[i] = random();
}


int main(int argc, char *argv[])
{
	struct nand_chip * chip;

	chip = nandsim_file_init("nand-em-file", 256, 64, 2048, 64);

	nanddrv_erase(chip,0);

	verify_page(chip, 4,0xFF);

	randomise_buffer(buffer0);
	nanddrv_write(chip, 4, 0, buffer0, BUF_SIZE);

	memset(buffer1,0xaa,sizeof(buffer1));
	nanddrv_write(chip, 5, 0, buffer1, BUF_SIZE);
	nanddrv_read(chip, 4, 0, buffer1, BUF_SIZE);
	if(memcmp(buffer0, buffer1, BUF_SIZE))
		printf("Buffers should be the same but are not\n");
	else
		printf("Buffers were the same. Good.\n");
	verify_page(chip, 5, 0xaa);

	nanddrv_erase(chip, 0);
	verify_page(chip, 4, 0xff);
	verify_page(chip, 5, 0xff);

	return 0;
}
