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
/*
 * Nand simulator modelled on a Samsung K9K2G08U0A 8-bit
 *
 * Page size 2k + 64
 * Block size 64 pages
 * Dev size 256 Mbytes
 *
 *  Need to implement basic commands first:
 *  Status
 *  Reset
 *  Read
 *  Random read (ie set read position within current page)
 *  Write
 *  Erase
 */

#include "nandsim.h"
#include "nand_chip.h"

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#if 0
#define debug(n, fmt, ...) \
	do { \
	if(n > 0) \
		printf(fmt, ## __VA_ARGS__); \
	} while(0)

#else
#define debug(n, fmt, ...) do {} while(0)
#endif

struct nandsim_private {

	struct nand_store *store;

	/*
	* Access buffers.
	* Address buffer has two parts to it:
	* 2 byte column (byte) offset
	* 3 byte row (page) offset
	*/

	unsigned char *buffer;
	int buff_size;

	unsigned char addr_buffer[5];

	/*
	* Offsets used to access address, read or write buffer.
	* If the offset is negative then accessing is illegal.
	*/

	int addr_offset;
	int addr_expected;
	int addr_received;

	int read_offset;
	int write_offset;
	int read_started;

	/*
	* busy_count: If greater than zero then the device is busy.
	* Busy count is decremented by check_busy() and by read_status()
	*/
	int busy_count;
	int write_prog_error;
	int reading_status;
	unsigned char last_cmd_byte;

	int ale;
	int cle;
};

static void last_cmd(struct nandsim_private *ns, unsigned char val)
{
	ns->last_cmd_byte = val;
}

static void check_last(struct nandsim_private *ns, unsigned char should_be, int line)
{
	if(ns->last_cmd_byte != should_be)
		debug(1, "At line %d:, last_cmd should be %02x, but is %02x\n",
			line, should_be, ns->last_cmd_byte);
}

static void idle(struct nandsim_private *ns, int line)
{
	ns->read_offset = -1;
	ns->write_offset = -1;
	ns->addr_offset = -1;
	ns->addr_expected = -1;
	ns->addr_received = -1;
	last_cmd(ns, 0xff);
	ns->busy_count = 0;
	ns->reading_status = 0;
	ns->read_started = 0;
}


static void expect_address(struct nandsim_private *ns, int nbytes, int line)
{
	switch (nbytes){
	case 2:
	case 3:
	case 5:
		ns->addr_offset = 5 - nbytes;
		ns->addr_expected = nbytes;
		ns->addr_received = 0;
		debug(1, "Expect %d address bytes\n",nbytes);
		break;
	default:
		debug(1, "expect_address illegal value %d called at line %d\n",
			nbytes, line);
	}
}

static int get_page_address(struct nandsim_private *ns)
{
	int addr;

	addr = (ns->addr_buffer[2]) |
		(ns->addr_buffer[3] << 8) |
		(ns->addr_buffer[4] << 16);
	return addr;
}

static int get_page_offset(struct nandsim_private *ns)
{
	int offs;

	offs = (ns->addr_buffer[0]) |
		(ns->addr_buffer[1] << 8);
	return offs;
}

static void check_address(struct nandsim_private *ns, int nbytes, int line)
{
	if(ns->addr_expected != 0)
		debug(1, "Still expecting %d address bytes at line: %d\n",
					ns->addr_expected, line);
	if(ns->addr_received != nbytes)
		debug(1, "Only received %d address bytes instead of %d at line %d\n",
					ns->addr_received, nbytes, line);
}

static void set_offset(struct nandsim_private *ns)
{
	ns->read_offset = -1;
	ns->write_offset = -1;

	if(ns->last_cmd_byte == 0x80 )
		ns->write_offset = get_page_offset(ns);
	else if(ns->last_cmd_byte == 0x00)
		ns->read_offset = get_page_offset(ns);
	debug(1, "Buffer offsets set to read %d write %d\n",
			ns->read_offset, ns->write_offset);
}

static void load_read_buffer(struct nandsim_private *ns)
{
	ns->store->retrieve(ns->store, get_page_address(ns),ns->buffer);
}
static void save_write_buffer(struct nandsim_private *ns)
{
	ns->store->store(ns->store, get_page_address(ns),ns->buffer);
}

static void check_read_buffer(struct nandsim_private *ns, int line)
{
}

static void end_cmd(struct nandsim_private *ns, int line)
{
	ns->last_cmd_byte = 0xff;
}

static void set_busy(struct nandsim_private *ns, int cycles, int line)
{
	ns->busy_count = cycles;
}

static int check_not_busy(struct nandsim_private *ns, int line)
{
	if(ns->busy_count > 0)
		debug(1, "Busy check failed at line %d\n",line);
	return (ns->busy_count < 1);
}

/*
 * Reset
 * Cmd: 0xff
 */
static void reset_0(struct nandsim_private *ns)
{
	last_cmd(ns, 0xff);
	idle(ns, __LINE__);
	end_cmd(ns, __LINE__);
}

/*
 * Read
 * cmd: 0x00, 5 address bytes, cmd: 0x30, wait not busy, read out data
 *
 * Note that 0x30 can also be used to exit the status reading state
 * and return to data reading state.
 *
 * The following sequence uses the busy pin to wait:
 *
 *  Write cmd 0x00
 *  Write 5 address bytes
 *  Write cmd 0x30. Device now goes busy
 *  Wait for busy pin to go idle
 *  Read data bytes.
 *
 * The following sequence uses the status read to wait:
 *  Write cmd 0x00
 *  Write 5 address bytes
 *  Write cmd 0x30. Device now goes busy
 *  Write 0x70: (status)
 *  Read status until device no longer busy
 *  Write command byte 0x30 to exit status read. Can now read data
 *  Read data bytes.

 */


static void read_0(struct nandsim_private *ns)
{
	check_last(ns, 0xff, __LINE__);
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	expect_address(ns, 5, __LINE__);
	last_cmd(ns, 0x00);
	ns->read_started = 1;
}

static void read_1(struct nandsim_private *ns)
{
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	if(ns->read_started){
		/* Doing a read */
		ns->read_started = 0;
		check_address(ns, 5, __LINE__);
		load_read_buffer(ns);
		set_busy(ns, 2, __LINE__);
		end_cmd(ns, __LINE__);
	} else {
		/* reenter read mode after a status check */
		end_cmd(ns, __LINE__);
	}
}

/*
 * Program page
 * Cmd: 0x80, 5-byte address, data bytes,  Cmd: 0x10, wait not busy
 */

static void program_0(struct nandsim_private *ns)
{
	check_last(ns, 0xff, __LINE__);
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	expect_address(ns, 5, __LINE__);
	last_cmd(ns, 0x80);
}

static void program_1(struct nandsim_private *ns)
{
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	check_last(ns, 0x80, __LINE__);
	check_address(ns, 5, __LINE__);
	save_write_buffer(ns);
	set_busy(ns, 2, __LINE__);
	end_cmd(ns, __LINE__);
}

/*
 * Random Data Output (sets read position in current page)
 * Cmd: 0x05, 2-byte address, cmd: 0xE0. No busy
 */

static void random_data_output_0(struct nandsim_private *ns)
{
	check_last(ns, 0xff, __LINE__);
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	check_read_buffer(ns, __LINE__);
	expect_address(ns, 2, __LINE__);
	last_cmd(ns, 0x05);
}

static void random_data_output_1(struct nandsim_private *ns)
{
	check_last(ns, 0xE0, __LINE__);
	check_address(ns, 2, __LINE__);
}

/*
 * Block erase
 * Cmd: 0x60, 3-byte address, cmd: 0xD0. Wait not busy.
 */
static void block_erase_0(struct nandsim_private *ns)
{
	check_last(ns, 0xff, __LINE__);
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	expect_address(ns, 3, __LINE__);
	last_cmd(ns, 0x60);
}

static void block_erase_1(struct nandsim_private *ns)
{
	check_last(ns, 0x60, __LINE__);
	if(check_not_busy(ns, __LINE__))
		ns->reading_status = 0;
	check_address(ns, 3, __LINE__);
	set_busy(ns, 5, __LINE__);
	ns->store->erase(ns->store, get_page_address(ns));
	end_cmd(ns, __LINE__);
}
/*
 * Read stuatus
 * Cmd 0x70
 */
static void read_status(struct nandsim_private *ns)
{
	ns->reading_status = 1;
}

static void read_id(struct nandsim_private *ns)
{
}

static void random_data_input(struct nandsim_private *ns)
{
}

static void unsupported(struct nandsim_private *ns)
{
}

static void nandsim_cl_write(struct nandsim_private *ns, unsigned char val)
{
	switch(val){
		case 0x00:
			read_0(ns);
			break;
		case 0x05:
			random_data_output_0(ns);
			break;
		case 0x10:
			program_1(ns);
			break;
		case 0x15:
			unsupported(ns);
			break;
		case 0x30:
			read_1(ns);
			break;
		case 0x35:
			unsupported(ns);
			break;
		case 0x60:
			block_erase_0(ns);
			break;
		case 0x70:
			read_status(ns);
			break;
		case 0x80:
			program_0(ns);
			break;
		case 0x85:
			random_data_input(ns);
			break;
		case 0x90:
			read_id(ns);
			break;
		case 0xD0:
			block_erase_1(ns);
			break;
		case 0xE0:
			random_data_output_1(ns);
			break;
		case 0xFF:
			reset_0(ns);
			break;
		default:
			debug(1, "CLE written with invalid value %02X.\n",val);
			break;
			/* Valid codes that we don't handle */
			debug(1, "CLE written with invalid value %02X.\n",val);
	}
}


static void nandsim_al_write(struct nandsim_private *ns, unsigned char val)
{
	check_not_busy(ns, __LINE__);
	if(ns->addr_expected < 1 ||
		ns->addr_offset < 0 ||
		ns->addr_offset >= sizeof(ns->addr_buffer)){
		debug(1, "Address write when not expected\n");
	} else {
		debug(1, "Address write when expecting %d bytes\n",
			ns->addr_expected);
		ns->addr_buffer[ns->addr_offset] = val;
		ns->addr_offset++;
		ns->addr_received++;
		ns->addr_expected--;
		if(ns->addr_expected == 0)
			set_offset(ns);
	}
}

static void nandsim_dl_write(struct nandsim_private *ns, unsigned char val)
{
	check_not_busy(ns, __LINE__);
	if( ns->write_offset < 0 || ns->write_offset >= ns->buff_size){
		debug(1, "Write at illegal buffer offset %d\n",
				ns->write_offset);
	} else {
		ns->buffer[ns->write_offset] = val;
		ns->write_offset++;
	}
}

static unsigned char nandsim_dl_read(struct nandsim_private *ns)
{
	unsigned char retval;
	if(ns->reading_status){
		retval = 0xff;
		if(ns->busy_count > 0){
		    	ns->busy_count--;
			retval&= ~(1<<6);
		}
		if(ns->write_prog_error)
			retval &= ~(1<<7);
		debug(1, "Read status returning %02X\n",retval);
	} else if(ns->busy_count > 0){
		debug(1, "Read while still busy\n");
		retval = 0;
	} else if(ns->read_offset < 0 || ns->read_offset >= ns->buff_size){
		debug(1, "Read with no data available\n");
		retval = 0;
	} else {
		retval = ns->buffer[ns->read_offset];
		ns->read_offset++;
	}

	return retval;
}


static struct nandsim_private *
nandsim_init_private(struct nand_store *store)
{
	struct nandsim_private *ns;
	unsigned char *buffer;
	int buff_size;

	buff_size = (store->data_bytes_per_page + store->spare_bytes_per_page);

	ns = malloc(sizeof(struct nandsim_private));
	buffer = malloc(buff_size);
	if(!ns || !buffer){
		free(ns);
		free(buffer);
		return NULL;
	}

	memset(ns, 0, sizeof(struct nandsim_private));
	ns->buffer = buffer;
	ns->buff_size = buff_size;
	ns->store = store;
	idle(ns, __LINE__);
	return ns;
}


static void nandsim_set_ale(struct nand_chip * this, int ale)
{
	struct nandsim_private *ns =
		(struct nandsim_private *)this->private_data;
	ns->ale = ale;
}

static void nandsim_set_cle(struct nand_chip * this, int cle)
{
	struct nandsim_private *ns =
		(struct nandsim_private *)this->private_data;
	ns->cle = cle;
}

static unsigned char nandsim_read_cycle(struct nand_chip * this)
{
	unsigned char retval;
	struct nandsim_private *ns =
		(struct nandsim_private *)this->private_data;

	if (ns->cle || ns->ale){
		debug(1, "Read cycle with CLE %s and ALE %s\n",
			ns->cle ? "high" : "low",
			ns->ale ? "high" : "low");
		retval = 0;
	} else {
		retval =nandsim_dl_read(ns);
	}
	debug(1, "Read cycle returning %02X\n",retval);
	return retval;
}

static void nandsim_write_cycle(struct nand_chip * this, unsigned char b)
{
	struct nandsim_private *ns =
		(struct nandsim_private *)this->private_data;
	const char *x;

	if(ns->ale && ns->cle)
		x = "ALE AND CLE";
	else if (ns->ale)
		x = "ALE";
	else if (ns->cle)
		x = "CLE";
	else
		x = "data";

	debug(1, "Write %02x to %s\n",
			b, x);
	if (ns->cle && ns->ale)
		debug(1, "Write cycle with both ALE and CLE high\n");
	else if (ns->cle)
		nandsim_cl_write(ns, b);
	else if (ns->ale)
		nandsim_al_write(ns, b);
	else
		nandsim_dl_write(ns, b);
}

static int nandsim_check_busy(struct nand_chip * this)
{
	struct nandsim_private *ns =
		(struct nandsim_private *)this->private_data;


	if (ns->busy_count> 0){
		ns->busy_count--;
		debug(1, "Still busy\n");
		return 1;
	} else {
		debug(1, "Not busy\n");
		return 0;
	}
}

static void nandsim_idle_fn(struct nand_chip *this)
{
	struct nandsim_private *ns =
		(struct nandsim_private *)this->private_data;
	ns = ns;
}

struct nand_chip *nandsim_init(struct nand_store *store)
{
	struct nand_chip *chip = NULL;
	struct nandsim_private *ns = NULL;

	chip = malloc(sizeof(struct nand_chip));
	ns = nandsim_init_private(store);

	if(chip && ns){
		memset(chip, 0, sizeof(struct nand_chip));;

		chip->private_data = ns;
		chip->set_ale = nandsim_set_ale;
		chip->set_cle = nandsim_set_cle;
		chip->read_cycle = nandsim_read_cycle;
		chip->write_cycle = nandsim_write_cycle;
		chip->check_busy = nandsim_check_busy;
		chip->idle_fn = nandsim_idle_fn;

		chip->blocks = ns->store->blocks;
		chip->pages_per_block = ns->store->pages_per_block;
		chip->data_bytes_per_page = ns->store->data_bytes_per_page;
		chip->spare_bytes_per_page = ns->store->spare_bytes_per_page;

		return chip;
	} else {
		free(chip);
		free(ns);
		return NULL;
	}

}
