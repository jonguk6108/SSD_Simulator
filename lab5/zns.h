#pragma once

#include "nand.h"

typedef unsigned char u8;
typedef unsigned int u32;

extern int NBANK;	// #banks
extern int NBLK;	// #blocks per bank
extern int NPAGE;	// #pages per block (power-of-two)
#define NSECT 8		// #sectors per page

#define SECT_SIZE (PAGE_DATA_SIZE / NSECT)

extern int DEG_ZONE;	// degree of zone (power-of-two)
extern int MAX_OPEN_ZONE;

#define MAX_ZONE (NBANK / DEG_ZONE * (NBLK-MAX_OPEN_ZONE))
#define ZONE_SIZE (DEG_ZONE * NPAGE * NSECT)
#define MAX_LBA (MAX_ZONE * ZONE_SIZE)

static inline int lba_to_zone(int lba)
{
	return lba / ZONE_SIZE;
}

enum zone_state {
	ZONE_EMPTY	= 0,
	ZONE_OPEN	= 1,
	ZONE_FULL	= 2,
	ZONE_TLOPEN	= 3,
};

struct zone_desc {
	int state;
	int slba;
	int wp;
};

/* ZNS */
void zns_init(int nbank, int nblk, int npage, int dzone, int max_open_zone);
int zns_write(int lba, int nsect, u32 *data);
void zns_read(int lba, int nsect, u32 *data);
int zns_reset(int lba);
void zns_get_desc(int lba, int nzone, struct zone_desc *descs);

/* ZNS Plus */
int zns_izc(int src_zone, int dest_zone, int copy_len, int *copy_list);
int zns_tl_open(int zone, u8 *bitmap);
