/*
 * Nand simulator modelled on a Samsung K9K2G08U0A 8-bit 
 *
 * Page size 2k + 64
 * Block size 64 pages
 * Dev size 256 Mbytes
 */

#include "nandsim.h"

#define BYTES_PER_PAGE 2112
#define PAGES_PER_BLOCK 64
#define BLOCKS_PER_DEVICE ((256 * 1024 * 1024)/(128 * 1024))

static uint8_t nand_data[BLOCKS_PER_DEVICE][PAGES_PERBLOCK][BYTES_PER_PAGE];

static uint8_t data_buffer[BYTES_PER_PAGE];
static uint8_t addr_buffer[5];

static int addrOffset;
static int readOffset;
static int writeOffset;
static int busy;
static int status;


static void Idle(void)
{
	readOffset = -1;
	writeOffset = -1;
	addrOffset = -1;
	lastCmd = 0xff;
	busy = 0;
	status = 0;
}


/* Read status register
 */

static void ReadStatusRegister(void)
{
	if(busy > 0)
		busy--;
	status = 1;   
}


/* Reading
 * 0x00, 5xaddr bytes, 0x30, wait not busy, read out data
 */

 
static void Read0(void)
{
	CheckLast(0xff);
	addrOffset = 0;
	lastCmd = 0x00;
}

void Read1(void)
{
	CheckLast(0x00);
	CheckAddress();
	load_read_buffer();
	busy = 2;
}

void Program0(void)
{
	CheckLast(0xff);
	addrOffset = 0;
	lastCmd = 0x80;
}

void Program1(void)
{
	CheckLast(0x80);
	CheckAddress();
}



void CLWrite(uint8_t val)
{
	switch(val){
		case 0x00:
			Read0();
			break;
		case 0x30:
			Read1();
			break;
		case 0x80:
			Program0();
			break;
		case 0x10:
			PageProgram1();
			break;
		case 0xFF:
			Reset0();
			break;

		case 0x70:
			ReadStatusRegister();
			break;
		case 0x05:
			RandomDataOutput0();
			break;
		case 0xE0:
			RandomDataOutput1();
			break;
		case 0x31:
			CacheRead1();
			break;
		case 0x3F:
			ExitCacheRead0();
			break;
		case 0x85:
			RandomDataInput0();
			break;
		case 0x60:
			BlockErase0();
			break;
		case 0xD0:
			BlockErase1();
			break;
		case 0x90:
			ReadSignature0();
			break;
		case 0xECh:
			ReadONFIParameter(0);
			break;
		default:
			printf("CLE Written with %02X.\n",val);
		
	}
}


static void ALWrite(uint8_t val)
{
	if(addrOffset < 0 || addrOffset >= sizeof(addr_buffer))
		report_error("Address write when not expected");
	else {
		addr_buffer[addrOffset] = val;
		addrOffset++;
	}
}

static void DLWrite(uint8_t val)
{
}

static uint8_t DLRead(void)
{
	uint8_t retval;
	if(status){
		retval = 0xff;
		if(busy)
			retval&= ~(1<<6);
		if(write_prog_error)
			retval &= ~(1<<7);
		status = 0;
	} else if(busy){
		report_error("Read while still busy");
		retval = 0;
	} else if(readOffset < 0 || readOffset >= BYTES_PER_PAGE){
		report_error("Read with no data available");
		retval = 0;
	} else {
		retval = data_buffer[readOffset];
		readOffset++;
	}
	
	return retval;	
}

static int handle;

void nandsim_Save(void)
{
	lseek(handle,0, SEEK_SET);
	write(handle, nand_data, sizeof(nand_data));	
}

void nandsim_Initialise(void)
{
	int fsize;
	Idle();

	handle = open(EMFILE,O_RDWR | O_CREAT);
	if(handle > 0){
		fsize = lseek(handle,0,SEEK_END);
		lseek(handle,0,SEEK_SET);

		if (fsize != sizeof(nand_data) {
			memset(nand_data,0xff,sizeof(nand_data));
			nandsim_Save();
		} else
			nbytes = read(handle,nand_data,sizeof(nand_data));
	}
}
