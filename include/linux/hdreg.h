#ifndef _HDREG_H
#define _HDREG_H

struct partition
{
	unsigned char boot_ind;
	unsigned char head;			/* ? */
	unsigned char sector;		/* ? */
	unsigned char cyl;			/* ? */
	unsigned char sys_ind;		/* ? */
	unsigned char end_head;		/* ? */
	unsigned char end_sector;	/* ? */
	unsigned char end_cyl;		/* ? */
	unsigned int start_sect;	/* starting sector counting from 0 */
	unsigned int nr_sects;		/* nr of sectors in partition */
}

#endif