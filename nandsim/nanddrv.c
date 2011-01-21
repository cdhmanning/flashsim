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

static void nanddrv_send_cmd(struct nand_chip *this, char cmd)
{
	this->set_cle(this, 1);
	this->write_cycle(this, cmd);
	this->set_cle(this, 0);
}

static int nanddrv_wait_not_busy(struct nand_chip *this)
{
	if(this->check_busy){
		while(this->check_busy(this)){
			if(this->idle_fn)
				this->idle_fn(this);
		}
		return 0;
	} else {
		nanddrv_send_cmd(this, 0x70);
		while(this->read_cycle(this) & 1){
			if(this->idle_fn)
				this->idle_fn(this);
		}
		return 1;
	}
}

int nanddrv_read(struct nand_chip *this, int page, int offset,
		int n_bytes, char *buffer)
{
	nanddrv_send_cmd(this, 0x00);
	nanddrv_send_addr(this, page, n_bytes);
	nanddrv_send_cmd(this, 0x30);
	if(nanddrv_wait_not_busy(this))
		nanddrv_send_cmd(this, 0x30);
	return 0;

}

int nanddrv_write(struct nand_chip *this, int page, int offset,
		int n_bytes, const char *buffer)
{
	return -1;
}

int nanddrv_erase(struct nand_chip *this, int block)
{
	return -1;
}



