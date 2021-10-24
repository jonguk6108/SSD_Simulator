/*
 * Lab #2 : Page Mapping FTL Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2021)
 *
 * Sep. 23, 2021.
 *
 * TA: Youngjae Lee, Jeeyoon Jung
 * Prof: Dongkun Shin
 * Embedded Software Laboratory
 * Sungkyunkwan University
 * http://nyx.skku.ac.kr
 *
 * 2021.09.30. late submit and finished
 * 2017312848 Park jonguk
 */

#include "ftl.h"
#include "nand.h"

#define SIZE_BANK  (BLKS_PER_BANK) * (PAGES_PER_BLK)

//int pmt[N_BANKS][SIZE_BANK];
//int val[N_BANKS][BLKS_PER_BANK][PAGES_PER_BLK];
//int spblk[N_BANKS];

int** pmt;
int*** val;
int* spblk;

static void garbage_collection(u32 bank)
{
    int freeblk = spblk[bank];
    int blk, page;

    int cnt;
    int tarcnt = -1;
    int tarblk = -1;

    for(blk = 0; blk < BLKS_PER_BANK; blk++) {
        cnt = 0;
        for(page = 0; page < PAGES_PER_BLK; page++) {
            if(val[bank][blk][page] == 2)
                cnt++;
        }
        if(cnt > tarcnt) {
            tarcnt = cnt;
            tarblk = blk;
        }
    } 

    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };
    int freepage = 0;

    for(page = 0; page < PAGES_PER_BLK; page++) {
        if(val[bank][tarblk][page] == 1) {
            stats.gc_write++;
            nand_read(bank , tarblk, page, data, spare);
            nand_write(bank , freeblk, freepage, data, spare);
            pmt[bank][spare[0]] = freeblk * PAGES_PER_BLK + freepage;
            val[bank][freeblk][freepage] = 1;
            freepage++;
        }
        val[bank][tarblk][page] = 0;
    }
    for(; freepage < PAGES_PER_BLK; freepage++)
        val[bank][freeblk][freepage] = 0;
    nand_erase(bank, tarblk);
    spblk[bank] = tarblk;
	stats.gc_cnt++;
	return;
}

void ftl_open()
{
    int i, j, k;

    pmt = (int**)malloc(sizeof(int**) * N_BANKS);
    val = (int***)malloc(sizeof(int**) * N_BANKS);
    spblk = (int*)malloc(sizeof(int) * N_BANKS);
    for(i = 0; i < N_BANKS; i++) {
        pmt[i] = (int*)malloc(sizeof(int) * SIZE_BANK);
        val[i] = (int**)malloc(sizeof(int*) * BLKS_PER_BANK);
        for(j = 0; j < BLKS_PER_BANK; j++) {
            val[i][j] = (int*)malloc(sizeof(int) * PAGES_PER_BLK);
        }
    }

    nand_init(N_BANKS, BLKS_PER_BANK, PAGES_PER_BLK);
    for(i = 0; i < N_BANKS; i++) {
        for(j = 0; j < SIZE_BANK; j++)
            pmt[i][j] = -1;
        
        for(j = 0; j < BLKS_PER_BANK; j++)
            for(k = 0; k < PAGES_PER_BLK; k++)
                val[i][j][k] = 0;
        
        spblk[i] = BLKS_PER_BANK - 1;
    }
    return;
}

void lba2lpn(u32 lba, int *bank, int *lpn, int *offset) {
    *lpn = lba / SECTORS_PER_PAGE;
    *bank = *lpn % N_BANKS;
    *lpn = *lpn / N_BANKS;
    *offset = lba % SECTORS_PER_PAGE;
    return;
}

void ppn2bp(int ppn, int *blk, int *page) {
    *blk = ppn / PAGES_PER_BLK;
    *page = ppn % PAGES_PER_BLK;
    return;
}

void ftl_read(u32 lba, u32 nsect, u32 *read_buffer)
{
    int bank, blk, page, lpn, ppn, offset;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

    while( nsect > 0 ) { 
        lba2lpn( lba, &bank, &lpn, &offset);
        ppn = pmt[bank][lpn];
        if(ppn != -1) {
            ppn2bp(ppn, &blk, &page);
            nand_read(bank, blk, page, data, &spare);
        }
        else data[offset] = 0xffffffff;
        *read_buffer = data[offset];
        read_buffer = read_buffer + 1;
        lba++;
        nsect--;
    }
    return;
}

void find_empty(int bank, int *blk, int *page) {
    int i, j;
    for(i = 0; i < BLKS_PER_BANK; i++) {
        if(i != spblk[bank])
            for(j = 0; j < PAGES_PER_BLK; j++) {
                if(val[bank][i][j] == 0) {
                    *blk = i;
                    *page = j;
                    return;
                }
            }
    }
    *blk = spblk[bank];
    *page = PAGES_PER_BLK;
    return;
}

void ftl_write(u32 lba, u32 nsect, u32 *write_buffer)
{

    int bank, blk, page, lpn, ppn;
    int offset;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };
    u32 pagedata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };

	stats.host_write += nsect;

    while( nsect > 0 ) {
        stats.nand_write++;
        int i;
        for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++ )
            pagedata[i] = -1;
        while(1) {
            lba2lpn( lba, &bank, &lpn, &offset );
            if(offset == ( (PAGE_DATA_SIZE / sizeof(u32) ) -1 ) || nsect == 1) break;
            else {
                pagedata[offset] = *write_buffer;
                write_buffer++;
                lba++;
                nsect--;
            }
        }
        find_empty(bank, &blk, &page);
        if(spblk[bank] == blk) {
            garbage_collection(bank);
            find_empty(bank, &blk, &page);
        }

        ppn = pmt[bank][lpn];
        int readblk, readpage;
        if(ppn != -1) {
            ppn2bp(ppn, &readblk, &readpage);
            nand_read(bank, readblk, readpage, data, spare);
            val[bank][readblk][readpage] = 2;
        }
        else {
            for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
                data[i] = 0xffffffff;
            spare[0] = lpn;
        }

        pmt[bank][lpn] = blk * PAGES_PER_BLK + page;

        pagedata[offset] = *write_buffer;
        write_buffer++;
        lba++;
        nsect--;

        for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
            if(pagedata[i] == -1)
                pagedata[i] = data[i];

        nand_write(bank, blk, page, pagedata, spare);
        val[bank][blk][page] = 1;
    }
    
	return;
}
