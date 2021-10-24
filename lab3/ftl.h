/*
 * Lab #3 : Page Mapping FTL Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2021)
 *
 * Sep. 30, 2021.
 *
 * TA: Youngjae Lee, Jeeyoon Jung
 * Prof: Dongkun Shin
 * Embedded Software Laboratory
 * Sungkyunkwan University
 * http://nyx.skku.ac.kr
 */
#pragma once

#include "nand.h"

typedef unsigned int u32;

#define N_BANKS			8
#define BLKS_PER_BANK	    32
#define PAGES_PER_BLK		32

#define N_BUFFERS		10
#define BUFFER_SIZE		(N_BUFFERS * SECTORS_PER_PAGE * SECTOR_SIZE)

#define SECTOR_SIZE		sizeof(u32)
#define SECTORS_PER_PAGE	(PAGE_DATA_SIZE / SECTOR_SIZE)

#define N_GC_BLOCKS		1

/* Per Bank */
#define OP_RATIO		7
#define N_PPNS_PB		(BLKS_PER_BANK * PAGES_PER_BLK)
#define N_USER_BLOCKS_PB	((BLKS_PER_BANK - N_GC_BLOCKS) * 100 / (100 + OP_RATIO))
#define N_OP_BLOCKS_PB		(BLKS_PER_BANK - N_USER_BLOCKS_PB)
#define N_LPNS_PB		(N_USER_BLOCKS_PB * PAGES_PER_BLK)

#define N_PPNS			(N_PPNS_PB * N_BANKS)
#define N_BLOCKS		(BLKS_PER_BANK * N_BANKS)
#define N_USER_BLOCKS		(N_USER_BLOCKS_PB * N_BANKS)
#define N_OP_BLOCKS		(N_OP_BLOCKS_PB * N_BANKS)

#define N_LPNS			(N_LPNS_PB * N_BANKS)

#define CHECK_PPAGE(ppn)	assert(ppn >= 0 && ppn < N_PPNS_PB)
#define CHECK_LPAGE(lpn)	assert((lpn) < N_LPNS)

enum gc_policy {
	gc_greedy,
	gc_cb,
	gc_cat
};

#define GC_POLICY		gc_cat

struct ftl_stats {
	int gc_cnt;
	int host_write; // in sector
	int nand_write; // in page
	int nand_read; // in page
	int gc_write; // in page
};

extern struct ftl_stats stats;

void ftl_open();
void ftl_write(u32 lba, u32 nsect, u32 *write_buffer);
void ftl_read(u32 lba, u32 nsect, u32 *read_buffer);
void ftl_trim(u32 lpn, u32 npage);
void ftl_flush();
