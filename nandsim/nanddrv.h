#ifndef __NAND_DRIVER_H__
#define __NAND_DRIVER_H__

int ndrv_Initialise(void);
int ndrv_Read(int pageId, int offset, int size, char *buffer);
int ndrv_Write(int pageId, int offset, int size, const char *buffer);
int ndrv_Erase(int blockId);

#endif

