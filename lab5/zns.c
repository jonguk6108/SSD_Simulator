/*
 * Lab #5 : ZNS+ Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2021)
 *
 * Oct. 21, 2021.
 *
 * TA: Youngjae Lee, Jeeyoon Jung
 * Prof: Dongkun Shin
 * Embedded Software Laboratory
 * Sungkyunkwan University
 * http://nyx.skku.ac.kr
 */
#include "zns.h"

/************ constants ************/
int NBANK;
int NBLK;
int NPAGE;

int DEG_ZONE;
int MAX_OPEN_ZONE;

int NUM_FCG;
/********** do not touch ***********/


void zns_init(int nbank, int nblk, int npage, int dzone, int max_open_zone)
{
	// constants
	NBANK = nbank;
	NBLK = nblk;
	NPAGE = npage;
	DEG_ZONE = dzone;
	MAX_OPEN_ZONE = max_open_zone;
	NUM_FCG = NBANK / DEG_ZONE;

	// nand
	nand_init(nbank, nblk, npage);

}

int zns_write(int start_lba, int nsect, u32 *data)
{
}

void zns_read(int start_lba, int nsect, u32 *data)
{
}

int zns_reset(int lba)
{
}

void zns_get_desc(int lba, int nzone, struct zone_desc *descs)
{
}

int zns_izc(int src_zone, int dest_zone, int copy_len, int *copy_list)
{
}

int zns_tl_open(int zone, u8 *valid_arr)
{
}
