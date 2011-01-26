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


#include "nanddrv.h"
#include "nand_chip.h"

int nanddrv_initialise(void)
{
	return 0;
}

static void nanddrv_send_addr(struct nand_chip *this, int page, int offset)
{
	this->set_ale(this,1);
	if(offset >= 0){
	    this->write_cycle(this, offset & 0xff);
	    this->write_cycle(this, (offset>>8) & 0x0f);
	}

	if(page >= 0){
	    this->write_cycle(this, page & 0xff);
	    this->write_cycle(this, (page>>8) & 0xff);
	    this->write_cycle(this, (page>>16) & 0xff);
	}
	this->set_ale(this,0);
}

static void nanddrv_send_cmd(struct nand_chip *this, unsigned char cmd)
{
	this->set_cle(this, 1);
	this->write_cycle(this, cmd);
	this->set_cle(this, 0);
}

static int nanddrv_wait_not_busy(struct nand_chip *this)
{
	unsigned char status;

	nanddrv_send_cmd(this, 0x70);
	do {
		status = this->read_cycle(this);
	} while ((status &  (1 << 6)) == 0);
	return 0;
}

int nanddrv_read(struct nand_chip *this, int page, int offset,
		unsigned char *buffer, int n_bytes)
{
	nanddrv_send_cmd(this, 0x00);
	nanddrv_send_addr(this, page, offset);
	nanddrv_send_cmd(this, 0x30);
	nanddrv_wait_not_busy(this);
	nanddrv_send_cmd(this, 0x30);
	while(n_bytes > 0) {
		*buffer = this->read_cycle(this);
		n_bytes--;
		buffer++;
	}
	return 0;
}
/*
 * Program page
 * Cmd: 0x80, 5-byte address, data bytes,  Cmd: 0x10, wait not busy
 */
int nanddrv_write(struct nand_chip *this, int page, int offset,
		const unsigned char *buffer, int n_bytes)
{
	nanddrv_send_cmd(this, 0x80);
	nanddrv_send_addr(this, page, offset);
	while(n_bytes > 0) {
		this->write_cycle(this, *buffer);
		n_bytes--;
		buffer++;
	}
	nanddrv_send_cmd(this, 0x10);
	nanddrv_wait_not_busy(this);
	return 0;
}

/*
 * Block erase
 * Cmd: 0x60, 3-byte address, cmd: 0xD0. Wait not busy.
 */
int nanddrv_erase(struct nand_chip *this, int block)
{
 	nanddrv_send_cmd(this, 0x60);
	nanddrv_send_addr(this, block * this->pages_per_block, -1);
	nanddrv_send_cmd(this, 0xD0);
	if(nanddrv_wait_not_busy(this))
		nanddrv_send_cmd(this, 0x30);
	return 0;
}



