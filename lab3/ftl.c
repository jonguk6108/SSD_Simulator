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
 *
 * Oct. 06, 2021. v3. 2017312848 Park JongUk
 * late submitted
 */
#include <stdio.h>
#include <float.h>
#include "stdlib.h"
#include "ftl.h"
#include "nand.h"
#define SIZE_BANK  (BLKS_PER_BANK) * (PAGES_PER_BLK)

int** pmt;  // pmt[bank][lpn] : ppn, if pmt[bank][lpn] == -1 no ppn, pmt[bank][lpn] < -1 : buf
int*** val;
int* spblk;
int** buf;  //if buf[page][8] == -1, no lba, and [8] in lba - off
//
int** valbuf; //if valbuf[page][0] == 0 -> blank, 1 -> fill
// valbuf[page][8] == whole
int** ageblk; // bank, blk agevalue
int** eraseblk;

void flush(int tar);

static void garbage_collection(u32 bank)
{
	stats.gc_cnt++;
    int freeblk = spblk[bank];
    int blk, page;
    int tarblk = -1;

    if(GC_POLICY == gc_greedy) {
        int cnt;
        int tarcnt = -1;
    
        for(blk = 0; blk < BLKS_PER_BANK; blk++) {
            cnt = 0;
            for(page = 0; page < PAGES_PER_BLK; page++)
                if(val[bank][blk][page] == 2)
                    cnt++;
            if(cnt > tarcnt) {
                tarcnt = cnt;
                tarblk = blk;
            }
        }
    }
    else {
        double tu = 0;
        double u = -1;
        tarblk = 0;
        if(GC_POLICY == gc_cat)
            u = DBL_MAX;
        for(blk = 0; blk < BLKS_PER_BANK; blk++) {
            if(blk == freeblk) continue;
            tu = 0;
            for(page = 0; page < PAGES_PER_BLK; page++)
                if(val[bank][blk][page] == 1)
                    tu = tu + 1;
            tu = tu / (double)PAGES_PER_BLK;
            if(GC_POLICY == gc_cb) {
                if(tu == 0) {
                    tarblk = blk;
                    break;
                }
                tu = (1-tu) / (2*tu);
                tu = tu * (double)ageblk[bank][blk];
                if(tu > u) {
                    u = tu;
                    tarblk = blk;
                }
            }
            if(GC_POLICY == gc_cat) {
                if(tu == 1 || ageblk[bank][blk] == 0) {
                    continue;
                }
                tu = tu / (1-tu);
                tu = tu / ((double)ageblk[bank][blk]);
                tu = tu * ((double)eraseblk[bank][blk]);
                if(tu < u) {
                    u = tu;
                    tarblk = blk;
                }
            }
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

    if(GC_POLICY != gc_greedy) {
        if(GC_POLICY == gc_cat)
            eraseblk[bank][tarblk]++;
        ageblk[bank][freeblk] = -1;
        ageblk[bank][tarblk] = -2;
        for(int i = 0; i < BLKS_PER_BANK; i++)
            ageblk[bank][i]++;

    }
	return;

}

void ftl_open()
{
    int i, j, k;

    pmt = (int**)malloc(sizeof(int**) * N_BANKS);
    val = (int***)malloc(sizeof(int**) * N_BANKS);
    spblk = (int*)malloc(sizeof(int) * N_BANKS);
    buf = (int**)malloc(sizeof(int**) * N_BUFFERS);
    valbuf = (int**)malloc(sizeof(int**) * N_BUFFERS);
    ageblk = (int**)malloc(sizeof(int**) * N_BANKS);
    eraseblk = (int**)malloc(sizeof(int**) * N_BANKS);
    for(i = 0; i < N_BANKS; i++) {
        pmt[i] = (int*)malloc(sizeof(int) * SIZE_BANK);
        val[i] = (int**)malloc(sizeof(int*) * BLKS_PER_BANK);
        ageblk[i] = (int*)malloc(sizeof(int) * BLKS_PER_BANK);
        eraseblk[i] = (int*)malloc(sizeof(int) * BLKS_PER_BANK);
        for(j = 0; j < BLKS_PER_BANK; j++)
            val[i][j] = (int*)malloc(sizeof(int) * PAGES_PER_BLK);
    }
    for(i = 0; i < N_BUFFERS; i++)
        buf[i] = (int*)malloc(sizeof(int) * (PAGE_DATA_SIZE / sizeof(u32) + 1));
    for(i = 0; i < N_BUFFERS; i++)
        valbuf[i] = (int*)malloc(sizeof(int) * (PAGE_DATA_SIZE / sizeof(u32) + 1));

    nand_init(N_BANKS, BLKS_PER_BANK, PAGES_PER_BLK);
    for(i = 0; i < N_BANKS; i++) {
        for(j = 0; j < SIZE_BANK; j++)
            pmt[i][j] = -1;
        
        for(j = 0; j < BLKS_PER_BANK; j++)
            for(k = 0; k < PAGES_PER_BLK; k++)
                val[i][j][k] = 0;
        
        spblk[i] = BLKS_PER_BANK - 1;

        for(j = 0; j < BLKS_PER_BANK; j++) {
            ageblk[i][j] = BLKS_PER_BANK - j - 2;
            eraseblk[i][j] = 1;
        }
    }
 
    for(i = 0; i < N_BUFFERS; i++)
        for(j = 0; j < (PAGE_DATA_SIZE / sizeof(u32) + 1); j++) {
            buf[i][j] = -1;
            valbuf[i][j] = 0;
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
    if(ppn < -1) {
        *blk = -1;
        *page = -ppn - 2;
        return;
    }
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
            nand_read(bank, blk, page, data, spare);
        }
        else data[offset] = 0xffffffff;

        for(int i = 0; i < N_BUFFERS; i++)
            if(valbuf[i][8] == 1) 
                if(buf[i][8] == lba - offset)
                    if(valbuf[i][offset] == 1) {
                        data[offset] = buf[i][offset];

                    }

        *read_buffer = data[offset];
        read_buffer = read_buffer + 1;
        lba++;
        nsect--;
    }
    return;
}

int find_buf(int lba) {
    int i = 0;
    for(i = 0; i < N_BUFFERS; i++)
        if(lba == buf[i][8])
            return i;
    for(i = 0; i < N_BUFFERS; i++)
        if(valbuf[i][8] == 0)
            return i;

    return i;
}

void find_nand(int bank, int *blk, int *page) {
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

int buf_empty() {  //emptybufnumbers
    int bpage, ans = 0;
    for(bpage = 0; bpage < N_BUFFERS; bpage++)
        if(valbuf[bpage][8] == 1) ans++;
    return N_BUFFERS - ans;
}

void ftl_write(u32 lba, u32 nsect, u32 *write_buffer)
{
	int bank, blk, page, lpn, ppn;
    int offset;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };
    u32 pagedata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 valpagedata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };

	stats.host_write += nsect;

    int pos;
    int ovpos = 0;  // same numbers with buffer
    lba2lpn(lba, &bank, &lpn, &offset);
    
    pos = offset + (nsect-1); // numbers to write pages by the host.
    pos = pos / ( PAGE_DATA_SIZE / sizeof(u32) ) + 1; 
    // if pos > N_BUFFERS, erase buffer and nand write
    // else, if buffer is excessed, flush and use buffer
    // else, else use buffer
    int bpage = 0;
    int ptmp = pos;
    int ltmp = (lba / SECTORS_PER_PAGE ) * SECTORS_PER_PAGE;

/***************************************************************/
    if(pos > N_BUFFERS) {    
        while( nsect > 0 ) {
            int i;
            for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++) {
                pagedata[i] = -1;
                valpagedata[i] = 0;
            }
    
            while(1) {
                lba2lpn(lba, &bank, &lpn, &offset);
                if(offset == ( (PAGE_DATA_SIZE / sizeof(u32) ) -1  ) || nsect == 1) break;
                else {
                    pagedata[offset] = *write_buffer;
                    valpagedata[offset] = 1;
                    write_buffer++;
                    lba++;
                    nsect--;
                }
            }
            pagedata[offset] = *write_buffer;
            valpagedata[offset] = 1;
            write_buffer++;
         
            lba2lpn(lba, &bank, &lpn, &offset);
            find_nand(bank, &blk, &page);
            if(spblk[bank] == blk) {
                garbage_collection(bank);
                find_nand(bank, &blk, &page);
            }

            int readblk, readpage;
            ppn = pmt[bank][lpn];
            if(ppn != -1) {
                ppn2bp(ppn, &readblk, &readpage);
                if(val[bank][readblk][readpage] == 1) {
                nand_read(bank, readblk, readpage, data, spare);
                val[bank][readblk][readpage] = 2;
                }
            }
            else {
                for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
                    data[i] = 0xffffffff;
                spare[0] = lpn;
            }

            int bpage;
            bpage = find_buf( lba - offset );
            if(bpage != N_BUFFERS) 
                if( lba-offset == buf[bpage][8] ) {
                    for(int j = 0; j < PAGE_DATA_SIZE / sizeof(u32); j++)
                        if(valbuf[bpage][j] == 1)
                            data[j] = buf[bpage][j];
                    for(int j = 0; j <= PAGE_DATA_SIZE / sizeof(u32); j++) {
                        valbuf[bpage][j] = 0;
                        buf[bpage][j] = 0;
                    }
                    buf[bpage][PAGE_DATA_SIZE / sizeof(u32)] = -1;
                }
    
            for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
                if(valpagedata[i] == 0)
                    pagedata[i] = data[i];

            pmt[bank][lpn] = blk * PAGES_PER_BLK + page;    
            nand_write(bank, blk, page, pagedata, spare);
            stats.nand_write++;
            val[bank][blk][page] = 1;
            lba++;
            nsect--;
        }
        return;
    }

/*********************************************/
    for(bpage = 0; bpage < N_BUFFERS; bpage++) {
        if(valbuf[bpage][8] == 0) continue;
        while(ptmp > 0) {
            if(buf[bpage][8] == ltmp) ovpos++;
            ltmp = ltmp + SECTORS_PER_PAGE;
            ptmp--;
        }
        ptmp = pos;
        ltmp = (lba/SECTORS_PER_PAGE)* SECTORS_PER_PAGE;
    }
    pos = pos - ovpos;
    if(pos > buf_empty()) ftl_flush();

    while( nsect > 0 ) {
        int i;
        for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++) {
            pagedata[i] = -1;
            valpagedata[i] = 0;
        }

        while(1) {
            lba2lpn(lba, &bank, &lpn, &offset);
            if(offset == ( (PAGE_DATA_SIZE / sizeof(u32) ) -1  ) || nsect == 1) break;
            else {
                pagedata[offset] = *write_buffer;
                valpagedata[offset] = 1;
                write_buffer++;
                lba++;
                nsect--;
            }
        }
        pagedata[offset] = *write_buffer;
        valpagedata[offset] = 1;
        write_buffer++;
        lba++;
        nsect--;
     
        int bpage;
        bpage = find_buf( lba - offset - 1 );
        for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++) {
            if(valpagedata[i] == 1) {
                valbuf[bpage][i] = 1;
                buf[bpage][i] = pagedata[i];
            }
        }
        valbuf[bpage][i] = 1;
        buf[bpage][i] = lba - offset - 1;
    }
	return;	
}

void ftl_flush() {
    int bpage = 0;
    for(bpage = 0; bpage < N_BUFFERS; bpage++)
        if(valbuf[bpage][8] == 1)
            flush(bpage);
}

void flush(int tar) { //tar == target buf page
    int lba, bank, lpn, offset;
    int ppn, blk, page, readblk, readpage;
    int i;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

    lba = buf[tar][8];
    lba2lpn(lba, &bank, &lpn, &offset);

    find_nand(bank, &blk, &page);
    if(spblk[bank] == blk) {
        garbage_collection(bank);
        find_nand(bank, &blk, &page);
    }

    ppn = pmt[bank][lpn];
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

    for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++) 
        if(valbuf[tar][i] == 1)
            data[i] = buf[tar][i];
    nand_write(bank, blk, page, data, spare);
    stats.nand_write++;
    val[bank][blk][page] = 1;

    for(i = 0; i <= PAGE_DATA_SIZE / sizeof(u32); i++) {
        valbuf[tar][i] = 0;
        buf[tar][i] = 0;
    }
    buf[tar][PAGE_DATA_SIZE / sizeof(u32)] = -1;
}


void ftl_trim(u32 lpn, u32 npage) {
    int lba = lpn * SECTORS_PER_PAGE;
    int i = 0;
    int ppn, bank, blk, page, offset;
    int tlpn;

    while(npage > 0) {
        for(i = 0; i < N_BUFFERS; i++)
            if(valbuf[i][8] == 1 && buf[i][8] == lba) {
                for(int j = 0; j <= PAGE_DATA_SIZE / sizeof(u32); j++) {
                    valbuf[i][j] = 0;
                    buf[i][j] = 0;
                }
                buf[i][8] = -1;
            }
        lba2lpn(lba, &bank, &tlpn, &offset);
        ppn = pmt[bank][tlpn];
        if(ppn != -1) {
            ppn2bp(ppn, &blk, &page);
            pmt[bank][tlpn] = -1;
            val[bank][blk][page] = 2;
        }
        lba = lba + SECTORS_PER_PAGE;
        npage--;
    }
}
