/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2010-2011 Aleph One Ltd.
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
#include "nandsim.h"


#define BUS_WIDTH_BYTES	2
#define BUF_SIZE	(2048 + 64)
unsigned char buffer0[BUF_SIZE];
unsigned char buffer1[BUF_SIZE];
unsigned char buffer2[BUF_SIZE];

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
	struct nanddrv_transfer tr;

	tr.buffer = buffer0;
	tr.offset = 0;
	tr.nbytes = BUF_SIZE;

	nanddrv_read_tr(chip, page,&tr, 1);
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
	struct nanddrv_transfer tr[2];

	chip = nandsim_file_init("nand-em-file", 256, 64, 2048, 64, BUS_WIDTH_BYTES);

	nanddrv_erase(chip,0);

	verify_page(chip, 4,0xFF);

	/* Write a random pattern to buffer0 and page 4 */

	randomise_buffer(buffer0);
	tr[0].buffer = buffer0;
	tr[0].offset = 0;
	tr[0].nbytes = BUF_SIZE;
	nanddrv_write_tr(chip, 4, tr, 1);

	/* Write AA pattern to buffer1 and page 5 */
	memset(buffer1,0xaa,sizeof(buffer1));
	tr[0].buffer = buffer1;
	tr[0].offset = 0;
	tr[0].nbytes = BUF_SIZE;
	nanddrv_write_tr(chip, 5, tr, 1);

	/* Read back buffer1 from page 4 and compare */
	memset(buffer1, 0, BUF_SIZE);
	tr[0].buffer = buffer1;
	tr[0].offset = 0;
	tr[0].nbytes = BUF_SIZE;
	nanddrv_read_tr(chip, 4, tr, 1);

	if(memcmp(buffer0, buffer1, BUF_SIZE))
		printf("Buffers should be the same but are not\n");
	else
		printf("Buffers were the same. Good.\n");


	printf("\n\n\nTesting multi-tr read\n\n\n\n");
	memset(buffer1, 0, BUF_SIZE);
	memset(buffer1, 0, BUF_SIZE);
	tr[0].buffer = buffer1;
	tr[0].offset = 0;
	tr[0].nbytes = 1000;
	tr[1].buffer = buffer2;
	tr[1].offset = 2000;
	tr[1].nbytes = 100;
	nanddrv_read_tr(chip, 4, tr, 2);

	if (memcmp(buffer0, buffer1, 1000))
		printf("Buffer should be the same but are not\n");
	else
		printf("Buffers were the same. Good.\n");

	if (memcmp(buffer0 + 2000, buffer2, 100))
		printf("Buffers should be the same but are not\n");
	else
		printf("Buffers were the same. Good.\n");


	nandsim_set_debug(0);

	printf("\n\n\nTesting multi-tr write\n\n\n\n");
	memset(buffer0, 0XAA, BUF_SIZE);
	memset(buffer1, 0x55, BUF_SIZE);
	tr[0].buffer = buffer0;
	tr[0].offset = 0;
	tr[0].nbytes = 1024;
	tr[1].buffer = buffer1;
	tr[1].offset = 2048;
	tr[1].nbytes = 64;
	nanddrv_write_tr(chip, 6, tr, 2);

	memset(buffer2, 0, BUF_SIZE);
	tr[0].buffer = buffer2;
	tr[0].offset = 0;
	tr[0].nbytes = BUF_SIZE;
	nanddrv_read_tr(chip, 6, tr, 1);

	if (memcmp(buffer0, buffer2, 1024))
		printf("Buffer should be the same but are not\n");
	else
		printf("Buffers were the same.Good\n");

	memset(buffer0,0xff,BUF_SIZE);
	if (memcmp(buffer0, buffer2 + 1024, 1024))
		printf("Buffer should be the same but are not\n");
	else
		printf("Buffers were the same. Good.\n");

	if (memcmp(buffer1, buffer2 + 2048, 64))
		printf("Buffer should be the same but are not\n");
	else
		printf("Buffers were the same. Good.\n");

	verify_page(chip, 5, 0xaa);

	printf("Erase and verify all erased\n");
	nanddrv_erase(chip, 0);
	verify_page(chip, 4, 0xff);
	verify_page(chip, 5, 0xff);

	return 0;
}
