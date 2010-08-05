#ifndef __NANDSIM_H__
#define __NANDSIM_H__

void nandsim_Initialise(void);
void nandsim_Save(void);

void nandsim_SetALE(int high);
void nandsim_SetCLE(int high);
void nandsim_ReadCycle(unsigned char byte_val);
void nandsim_WriteCycle(unsigned char *byte_val);
int  nandsim_Busy(void);
#endif
