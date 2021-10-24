/*
 * Lab #1 : NAND Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2021)
 *
 * Sep. 16, 2021.
 *
 * TA: Youngjae Lee, Jeeyoon Jung
 * Prof: Dongkun Shin
 * Embedded Software Laboratory
 * Sungkyunkwan University
 * http://nyx.skku.ac.kr
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nand.h"

/*
 * define your own data structure for NAND flash implementation
 */
typedef unsigned int u32;

u32**** ndt; // bank blk page data
u32**** nsp; // bank blk page check+spare
int dsize[3]; // bank blk page
// 2017312848 박종욱
/*
 * initialize the NAND flash memory
 * @nbanks: number of bank
 * @nblks: number of blocks per bank
 * @npages: number of pages per block
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if given dimension is invalid
 */
int nand_init(int nbanks, int nblks, int npages)
{
	if( nbanks <= 0 || nblks <= 0 || npages <= 0) return NAND_ERR_INVALID;

    int i, j, k, l;

    ndt = (u32****)malloc(sizeof(u32***) * nbanks);
    nsp = (u32****)malloc(sizeof(u32***) * nbanks);
    for(i = 0; i < nbanks; i++) {
        ndt[i] = (u32***)malloc(sizeof(u32**) * nblks);
        nsp[i] = (u32***)malloc(sizeof(u32**) * nblks);
        for(j = 0; j < nblks; j++) {
            ndt[i][j] = (u32**)malloc(sizeof(u32*) * npages);
            nsp[i][j] = (u32**)malloc(sizeof(u32*) * npages);
            for(k = 0; k < npages; k++) {
                ndt[i][j][k] = (u32*)malloc(sizeof(u32) * (PAGE_DATA_SIZE / sizeof(u32)) ) ;
                nsp[i][j][k] = (u32*)malloc(sizeof(u32) * (PAGE_SPARE_SIZE / sizeof(u32) + 1));
            }
        }
    }


    for(i = 0; i < nbanks; i++) {
        for(j = 0; j < nblks; j++) {
            for(k = 0; k < npages; k++) {
                for(l = 0; l < PAGE_DATA_SIZE / sizeof(u32); l++)
                    ndt[i][j][k][l] = 0;
                for(l = 0; l <= PAGE_SPARE_SIZE / sizeof(u32); l++)
                    nsp[i][j][k][l] = 0;
            }
        }
    }

	dsize[0] = nbanks;
	dsize[1] = nblks;
	dsize[2] = npages;
	return NAND_SUCCESS;
}

/*
 * write data and spare into the NAND flash memory page
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if target flash page address is invalid
 *   NAND_ERR_OVERWRITE if target page is already written
 *   NAND_ERR_POSITION if target page is empty but not the position to be written
 */
int nand_write(int bank, int blk, int page, void *data, void *spare)
{
    if( bank >= dsize[0] || blk >= dsize[1] || page >= dsize[2] || bank < 0 || blk < 0 || page < 0) return NAND_ERR_INVALID;
    if( nsp[bank][blk][page][0] == 1 ) return NAND_ERR_OVERWRITE;
    if( page != 0)
        if( nsp[bank][blk][page-1][0] == 0 )
            return NAND_ERR_POSITION;

    
    nsp[bank][blk][page][0] = 1;
    int i;
    for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++) {
        ndt[bank][blk][page][i] = *(u32 *)data;
        data = data + 4;
    }
    for(i = 0; i < PAGE_SPARE_SIZE / sizeof(u32); i++) {
        nsp[bank][blk][page][i+1] = *(u32 *)spare;
        spare = spare + 4;
    }
	return NAND_SUCCESS; 
}


/*
 * read data and spare from the NAND flash memory page
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if target flash page address is invalid
 *   NAND_ERR_EMPTY if target page is empty
 */
int nand_read(int bank, int blk, int page, void *data, void *spare)
{
    if( bank >= dsize[0] || blk >= dsize[1] || page >= dsize[2] || bank < 0 || blk < 0 || page < 0) return NAND_ERR_INVALID;
    if( nsp[bank][blk][page][0] == 0 ) return NAND_ERR_EMPTY;
  
    int i;
    for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++) {
        *(u32 *)data = ndt[bank][blk][page][i];
        data = data + 4;
    }
    for(i = 0; i < PAGE_SPARE_SIZE / sizeof(u32); i++) {
        *(u32 *)spare = nsp[bank][blk][page][i+1];
        spare = spare + 4;
    }
	return NAND_SUCCESS;
}

/*
 * erase the NAND flash memory block
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if target flash block address is invalid
 *   NAND_ERR_EMPTY if target block is already erased
 */
int nand_erase(int bank, int blk)
{
    if( bank >= dsize[0] || blk >= dsize[1] || bank < 0 || blk < 0) return NAND_ERR_INVALID;
    int j;
    int check = 0;
    for(j = 0; j < dsize[2]; j++)
        if( nsp[bank][blk][j][0] == 1 ) check = 1;
    if(check == 0) return NAND_ERR_EMPTY;

    int i;
    for(j = 0; j < dsize[2]; j++) {
        nsp[bank][blk][j][0] = 0;
        for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
            ndt[bank][blk][j][i] = 0;
        for(i = 0; i < PAGE_SPARE_SIZE / sizeof(u32); i++)
            nsp[bank][blk][j][i+1] = 0;
    }
	return NAND_SUCCESS;
}
