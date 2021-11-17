/*
 * Lab #4 : DFTL Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2021)
 *
 * Oct. 07, 2021.
 *
 * TA: Youngjae Lee, Jeeyoon Jung
 * Prof: Dongkun Shin
 * Embedded Software Laboratory
 * Sungkyunkwan University
 * http://nyx.skku.ac.kri
 *
 * Nov. 17, 2022. v6
 * 2017312848 Park Jonguk
 * Submitted (finished) -> change gc
 */
#include <stdio.h>
#include <stdlib.h>
#include "ftl.h"
#include "nand.h"
#define NSECT    PAGE_DATA_SIZE / (int)sizeof(u32)		// #sectors per page

int** gtd; // gtd[bank][map page] -> (ppn)
int*** cmt; // cmt[bank][cache_slot][0~ N_MAP_ENTRIES_PER_PAGE -1] : data_ppn / cmt[N_MAP_ENTRIES_PER_PAGE] : map page / cmt[N_MAP_ENTRIES_PER_PAGE + 1] -1 : no use, 1 : use
int** agecmt;
    // cmt[N_MAP_ENTRIES_PER_PAGE + 1] : dirty? 1 -> write, 0 -> read
int** useblk; // useblk[bank][blk] : 0 -> none, 1 -> datablk, 2 -> transblk (not full)
int*** val; // val[bank][blk][page] : 0 -> none, 1 -> 
int** buf; // if buf[page][8] == -1, no lba, and [8] in lba - off

static void map_write(u32 bank, u32 map_page, u32 cache_slot);
static void map_read(u32 bank, u32 map_page, u32 cache_slot);
static void map_garbage_collection(u32 bank);
void cache_ftl_read(u32 lba, int bufpage);
void cache_ftl_write(u32 lba, int bufpage);
int pcache;

void find_ppn(int bank, int p, int *ppn) {
    int numblk = 0, i = 0, j = 0, size;
    if(p == 1) size = N_PHY_DATA_BLK;
    else size = N_PHY_MAP_BLK;

    for(i = 0; i < BLKS_PER_BANK; i++) {
        for(j = 0; j < PAGES_PER_BLK; j++)
            if(useblk[bank][i] == p && val[bank][i][j] == 0) {
                *ppn = i* PAGES_PER_BLK + j;
                return;
            }
        if(useblk[bank][i] == p)
            numblk++;
    }
    if(numblk != size- 1) {
        for(i = 0; i < BLKS_PER_BANK; i++) {
            for(j = 0; j < PAGES_PER_BLK; j++)
                if(useblk[bank][i] == 0 && val[bank][i][j] == 0) {
                    *ppn = i * PAGES_PER_BLK + j;
                    return;
                }    
        }
    }
    *ppn = -1;
}

void lba2lpn(u32 lba, int *bank, int *lpn, int *offset) {
    *lpn = lba / SECTORS_PER_PAGE;
    *bank = *lpn % N_BANKS;
    *lpn = *lpn / N_BANKS;
    *offset = lba % SECTORS_PER_PAGE;
    return;
}
void ppn2bp(int ppn, int *blk, int *page) {
    if(ppn == -1) {
        *blk = -1;
        *page = -1;
        return;
    }
    *blk = ppn / PAGES_PER_BLK;
    *page = ppn % PAGES_PER_BLK;
    return;
}
void cache_garbage_collection(int bank) {
    int min = agecmt[bank][0];
    if(pcache == 0) min = agecmt[bank][1];

    int mincache = -1, i = 0;
    for(i = 0; i < N_CACHED_MAP_PAGE_PB; i++)
        if(min >= agecmt[bank][i] && i != pcache) {
            min = agecmt[bank][i];
            mincache = i;
        }
    if(N_CACHED_MAP_PAGE_PB == 1) mincache = 0;
    map_write(bank, cmt[bank][mincache][N_MAP_ENTRIES_PER_PAGE], mincache);
    return;
}
void use_cmt(int bank, int cache) {
    int max = agecmt[bank][0], i = 0;
    for(i = 0; i < N_CACHED_MAP_PAGE_PB; i++)
        if(max < agecmt[bank][i])
            max = agecmt[bank][i];
    agecmt[bank][cache] = max + 1;
    return;
}

static void map_write(u32 bank, u32 map_page, u32 cache_slot) // from cmt to nand
{
    if(cmt[bank][cache_slot][N_MAP_ENTRIES_PER_PAGE+1] == -1) {
        agecmt[bank][cache_slot] = 0;
        for(unsigned int i = 0; i < N_MAP_ENTRIES_PER_PAGE + 2; i++)
            cmt[bank][cache_slot][i] = -1;    
        return;
    }

    int ppn, blk, page;
    int readppn, readblk, readpage;
    unsigned int i = 0;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

    for(i = 0; i < NSECT; i++)
        data[i] = cmt[bank][cache_slot][i];
    spare[0] = map_page;
    
    readppn = gtd[bank][map_page];
    if(readppn != -1) {
        ppn2bp(readppn, &readblk, &readpage);
        val[bank][readblk][readpage] = 2;
    }

    find_ppn(bank, 2, &ppn);
    if(ppn == -1) {
        map_garbage_collection(bank);
        find_ppn(bank, 2, &ppn);
    }
    ppn2bp(ppn, &blk, &page);

    if( nand_write(bank, blk, page, data, spare) != 0) abort();
    val[bank][blk][page] = 1;
    useblk[bank][blk] = 2;
    gtd[bank][map_page] = blk*PAGES_PER_BLK + page;
    agecmt[bank][cache_slot] = 0;
    for(i = 0; i < N_MAP_ENTRIES_PER_PAGE + 2; i++)
        cmt[bank][cache_slot][i] = -1;   
    stats.map_write++;
    return;
}

static void map_read(u32 bank, u32 map_page, u32 cache_slot)
{
    stats.cache_miss++;
    int ppn = gtd[bank][map_page];
    int blk, page;
    unsigned int i;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

    for(i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
        data[i] = -1;

    if(ppn != -1) {
        ppn2bp(ppn, &blk, &page);
        if(nand_read(bank, blk, page, data, spare) != 0) abort();
    }
    for(i = 0; i < N_MAP_ENTRIES_PER_PAGE; i++)
        cmt[bank][cache_slot][i] = data[i];

    cmt[bank][cache_slot][N_MAP_ENTRIES_PER_PAGE] = map_page;
    cmt[bank][cache_slot][N_MAP_ENTRIES_PER_PAGE+1] = -1;
    return;
}

static void map_garbage_collection(u32 bank)
{
    int max = -1, maxblk = -1;
    int cnt = 0;
    int i = 0, j = 0;
    int tarblk = -1, tarpage = 0;
    int mappb = -1;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

    for(i = 0; i < BLKS_PER_BANK; i++) {
        if(useblk[bank][i] == 2) {
            cnt = 0;
            for(j = 0; j < PAGES_PER_BLK; j++)
                if(val[bank][i][j] == 2) 
                    cnt++;
            if(cnt > max) {
                max = cnt;
                maxblk = i;
            }
        }
    }

    for(i = 0; i < BLKS_PER_BANK; i++) {
        if(useblk[bank][i] == 0)
            break;
    }
    tarblk = i;
    useblk[bank][tarblk] = 2;
    tarpage = 0;
    for(i = 0; i < PAGES_PER_BLK; i++)
        if(val[bank][maxblk][i] == 1) {
            nand_read(bank, maxblk,i, data, spare);
            if( nand_write(bank, tarblk, tarpage, data, spare) != 0) abort();
            val[bank][tarblk][tarpage] = 1;
            mappb = spare[0];
            gtd[bank][mappb] = tarblk * PAGES_PER_BLK + tarpage;
            tarpage++;
            stats.map_gc_write++;
        }

    for(i = 0; i < PAGES_PER_BLK; i++)
        val[bank][maxblk][i] = 0;
    useblk[bank][maxblk] = 0;
    nand_erase(bank, maxblk);

    for(; tarpage < PAGES_PER_BLK; tarpage++)
        val[bank][tarblk][tarpage] = 0;
    stats.map_gc_cnt++;
    return;
}


static void garbage_collection(u32 bank)
{

    int max = -1, maxblk = -1;
    int cnt = 0;
    int i = 0;
    unsigned int j = 0;
    int tarblk = -1, tarpage = 0;
    int mappb = -1;
    int cache;
    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };
    for(i = 0; i < BLKS_PER_BANK; i++) {
        if(useblk[bank][i] == 1) {
            cnt = 0;
            for(j = 0; j < PAGES_PER_BLK; j++)
                if(val[bank][i][j] == 2) 
                    cnt++;
            if(cnt > max) {
                max = cnt;
                maxblk = i;
            }
        }
    }
    for(i = 0; i < BLKS_PER_BANK; i++) {
        if(useblk[bank][i] == 0)
            break;
    }
    tarblk = i;
    useblk[bank][tarblk] = 1;
    tarpage = 0;
    for(i = 0; i < PAGES_PER_BLK; i++) {
        if(val[bank][maxblk][i] != 1) continue;
        if( nand_read(bank, maxblk,i, data, spare) != 0) abort();
        mappb = spare[0];

        int usecache = 0;
        for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++) 
            if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == mappb) {
                stats.cache_hit++;
                usecache = 1;
                break;
            }
        
        if(usecache == 1) {
          for(j = 0; j < NSECT; j++)
              if( cmt[bank][cache][j] == maxblk * PAGES_PER_BLK + i)
                cmt[bank][cache][j] = tarblk * PAGES_PER_BLK + tarpage;
          use_cmt(bank, cache);
          cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE + 1] = 1;
        }
        if(usecache == 0) {
          int writeppn, writeblk, writepage;
          find_ppn(bank, 2, &writeppn);
          if(writeppn == -1) {
            map_garbage_collection(bank);
            find_ppn(bank, 2, &writeppn);
          }
          ppn2bp(writeppn, &writeblk, &writepage);
          
          int readppn = gtd[bank][mappb];
          int readblk, readpage;
          u32 mapdata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
          u32 mapspare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

          if(readppn != -1) {
            ppn2bp(readppn, &readblk, &readpage);
            nand_read(bank, readblk, readpage, mapdata, mapspare);
            val[bank][readblk][readpage] = 2;
          }

          for(j = 0; j < NSECT; j++)
              if( mapdata[j] ==  (unsigned int) maxblk * PAGES_PER_BLK + i)
                mapdata[j] = tarblk * PAGES_PER_BLK + tarpage;
          if( nand_write(bank, writeblk, writepage, mapdata, mapspare) != 0) abort();
          val[bank][writeblk][writepage] = 1;
          useblk[bank][writeblk] = 2;
          gtd[bank][mappb] = writeblk * PAGES_PER_BLK + writepage;
        }

        if(nand_write(bank, tarblk, tarpage, data, spare) != 0) abort();
        val[bank][tarblk][tarpage] = 1;
        tarpage++;
        stats.gc_write++;
    }

    for(i = 0; i <PAGES_PER_BLK; i++)
        val[bank][maxblk][i] = 0;
    useblk[bank][maxblk] = 0;
    nand_erase(bank, maxblk);

    for(; tarpage < PAGES_PER_BLK; tarpage++)
        val[bank][tarblk][tarpage] = 0;
    stats.gc_cnt++;
    return;
}

void ftl_open()
{
    int i, j, k;
    gtd = (int **)malloc(sizeof(int*) * N_BANKS);
    buf = (int **)malloc(sizeof(int*) * N_BUFFERS);
    cmt = (int ***)malloc(sizeof(int**) * N_BANKS);
    agecmt = (int **)malloc(sizeof(int*) * N_BANKS);
    useblk = (int **)malloc(sizeof(int*) * N_BANKS);
    val = (int ***)malloc(sizeof(int**) * N_BANKS);
    for(i = 0; i < N_BANKS; i++) {
        gtd[i] = (int *)malloc(sizeof(int) * N_MAP_PAGES_PB);
        cmt[i] = (int **)malloc(sizeof(int *) * N_CACHED_MAP_PAGE_PB);
        agecmt[i] = (int *)malloc(sizeof(int) * N_CACHED_MAP_PAGE_PB);
        useblk[i] = (int *)malloc(sizeof(int) * BLKS_PER_BANK);
        val[i] = (int **)malloc(sizeof(int *) * BLKS_PER_BANK);
        for(j = 0; j < N_CACHED_MAP_PAGE_PB; j++)
            cmt[i][j] = (int *)malloc(sizeof(int) * (N_MAP_ENTRIES_PER_PAGE + 2));
        for(j = 0; j < BLKS_PER_BANK; j++)
            val[i][j] = (int *)malloc(sizeof(int) * PAGES_PER_BLK);
    }
    for(i = 0; i < N_BUFFERS; i++)
      buf[i] = (int*)malloc(sizeof(int) * (PAGE_DATA_SIZE / sizeof(u32) + 1));

    nand_init(N_BANKS, BLKS_PER_BANK, PAGES_PER_BLK);
    for(i = 0; i < N_BANKS; i++) {
        for(j = 0; j < (int) N_MAP_PAGES_PB; j++)
            gtd[i][j] = -1;
        for(j = 0; j < N_CACHED_MAP_PAGE_PB; j++) {
            for(k = 0; k < (int) N_MAP_ENTRIES_PER_PAGE + 2; k++)
                cmt[i][j][k] = -1;
            agecmt[i][j] = 0;
        }
        for(j = 0; j < BLKS_PER_BANK; j++) {
            useblk[i][j] = 0;
            for(k = 0; k < PAGES_PER_BLK; k++)
                val[i][j][k] = 0;
        }
    }
    for(i = 0; i < N_BUFFERS; i++)
      for(unsigned int j = 0; j < (PAGE_DATA_SIZE / sizeof(u32) + 1); j++)
        buf[i][j] = -1;

    return;
}


int find_buf(int lba) {
  int i = 0;
  for(i = 0; i < N_BUFFERS; i++)
    if(lba == buf[i][8])
      return i;
  for(i = 0; i < N_BUFFERS; i++)
    if(buf[i][8] == -1) {
      cache_ftl_read(lba, i);
      return i;
    }

  cache_ftl_write(buf[0][NSECT], 0);
  return find_buf(lba);
}
int buf_empty() {
  int bpage, ans = 0;
  for(bpage = 0; bpage < N_BUFFERS; bpage++)
    if(buf[bpage][8] != -1)
      ans++;
  return N_BUFFERS - ans;
}


void cache_ftl_read(u32 lba, int bufpage)
{	
    int i = 0;
    int bank, lpn, offset;
    int readppn = 0, readblk = 0, readpage = 0;

    u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };

    while(1) {
        lba2lpn(lba, &bank, &lpn, &offset);
        
        int cache = 0;
        int mappb = lpn / N_MAP_ENTRIES_PER_PAGE;
        int mapof = lpn % N_MAP_ENTRIES_PER_PAGE;
        for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++) 
            if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == mappb) {
                stats.cache_hit++;
                break;
            }
        if(cache == N_CACHED_MAP_PAGE_PB) {
            for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++)
                if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == -1)
                    break;
            if(cache == N_CACHED_MAP_PAGE_PB) {
                cache_garbage_collection(bank);
                for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++)
                    if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == -1)
                        break;
            }
            map_read(bank, mappb, cache);
            stats.cache_miss++;
        }

        for(i = 0; i < NSECT; i++ )
            data[i] = 0xffffffff;

        readppn = cmt[bank][cache][mapof];
        use_cmt(bank, cache);
        if( readppn != -1 ) {
            ppn2bp(readppn, &readblk, &readpage);
            if( nand_read(bank, readblk, readpage, data, spare) != 0) abort();
            val[bank][readblk][readpage] = 2;
        }

        for(i = 0; i < NSECT; i++)
          buf[bufpage][i] = data[i];
        buf[bufpage][NSECT] = lba;
        return;
    }
}
void cache_ftl_write(u32 lba, int bufpage)
{
    int i = 0;
    int bank, lpn, offset;
    int ppn = 0, blk = 0, page = 0;

    u32 spare[PAGE_SPARE_SIZE / sizeof(u32)] = { 0 };
    u32 pagedata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };

    while(1) {
        for(i = 0; i < NSECT; i++)
            pagedata[i] = buf[bufpage][i];
       
        lba2lpn(lba, &bank, &lpn, &offset);
        int cache = 0;
        int mappb = lpn / N_MAP_ENTRIES_PER_PAGE;
        int mapof = lpn % N_MAP_ENTRIES_PER_PAGE;
        for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++) 
            if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == mappb) {
                stats.cache_hit++;
                break;
            }
        if(cache == N_CACHED_MAP_PAGE_PB) {
            for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++) 
                if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == -1)
                    break;
            if(cache == N_CACHED_MAP_PAGE_PB) {
                cache_garbage_collection(bank);
                for(cache = 0; cache < N_CACHED_MAP_PAGE_PB; cache++) 
                    if(cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE] == -1)
                        break;
            }
            map_read(bank, mappb, cache);
            stats.cache_miss++;
        }
        use_cmt(bank, cache);

        spare[0] = mappb;

        pcache = cache;
        find_ppn(bank, 1, &ppn);
        if(ppn == -1) {
            garbage_collection(bank);
            find_ppn(bank, 1, &ppn);
        }
        ppn2bp(ppn, &blk, &page);

        if( nand_write(bank, blk, page, pagedata, spare) != 0) abort();
        val[bank][blk][page] = 1;
        useblk[bank][blk] = 1;

        cmt[bank][cache][mapof] = ppn;
        cmt[bank][cache][N_MAP_ENTRIES_PER_PAGE+1] = 1;
        use_cmt(bank, cache);
        
        stats.nand_write++;
        for(int i = 0; i < NSECT + 1; i++)
          buf[bufpage][i] = -1;
        return;
    }
}

void ftl_read(u32 lba, u32 nsect, u32 *read_buffer)
{
    int bank, lpn, offset;

    while( nsect > 0 ) { 
        lba2lpn( lba, &bank, &lpn, &offset);

        int bpage = find_buf(lba - offset);

        *read_buffer = buf[bpage][offset];
        read_buffer = read_buffer + 1;
        lba++;
        nsect--;
    }
    return;
}

void ftl_write(u32 lba, u32 nsect, u32 *write_buffer)
{
    int bank, lpn;
    int offset;
    u32 pagedata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
    u32 valpagedata[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };

  	stats.host_write += nsect;

    while( nsect > 0 ) {
        unsigned int i;
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
                buf[bpage][i] = pagedata[i];
            }
        }
        buf[bpage][i] = lba - offset - 1;
    }
  	return;	
}
