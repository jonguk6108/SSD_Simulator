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
#include <stdio.h>
#include <stdlib.h>
#include "nand.h"

/************ constants ************/
int NBANK;
int NBLK;
int NPAGE;

int DEG_ZONE;
int MAX_OPEN_ZONE;

int NUM_FCG;
/********** do not touch ***********/
int** ZONE; // [zone number][state, write pointer, state lba]; // 0 empty, 1 open, 2 full, 3 tlopen
u32** BUFFER; // [zone number][value of sectors + lba];
int* ZTF; //[zone number] = blk number;
int* FB; //[queue] = size(numbers of freeblk), freeblk;
int** TL_BITMAP; // [zone number][zone_tlnum, bitmap];

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

  ZONE = (int **)malloc(sizeof(int*) * max_open_zone);
  for(int i = 0; i < max_open_zone; i++)
    ZONE[i] = (int *)malloc(sizeof(int) * 3);
  BUFFER = (u32 **)malloc(sizeof(u32*) * max_open_zone);
  for(int i = 0; i < max_open_zone; i++)
    BUFFER[i] = (u32 *)malloc(sizeof(u32) * NSECT);
  ZTF = (int *)malloc(sizeof(int) * max_open_zone);
  FB = (int *)malloc(sizeof(int) * nblk);
  TL_BITMAP = (int **)malloc(sizeof(int*) * max_open_zone);
  for(int i = 0; i < max_open_zone; i++)
    TL_BITMAP[i] = (int *)malloc(sizeof(int) * dzone * NSECT * NPAGE);
 
  for(int i = 0; i < max_open_zone; i++) {
    ZONE[i][0] = 0;
    ZONE[i][1] = i * DEG_ZONE * NSECT * NPAGE;
    ZONE[i][2] = i * DEG_ZONE * NSECT * NPAGE;
  }
  for(int i = 0; i < max_open_zone; i++)
    for(int j = 0; j < NSECT; j++)
      BUFFER[i][j] = - 1;
  for(int i = 0; i < max_open_zone; i++)
    ZTF[i] = -1;
  for(int i = 0; i <= NBLK; i++)
    FB[i] = i;
  for(int i = 0; i < max_open_zone; i++)
    for(int j = 0; j < DEG_ZONE * NSECT * NPAGE; j++)
      TL_BITMAP[i][j] = 0;

}

int zns_write(int start_lba, int nsect, u32 *data)
{
  int i_sect = 0;
  while(i_sect < nsect) {
    int c_lba = start_lba + i_sect;
    int lba = start_lba + i_sect;
    int c_sect = lba % NSECT;
    lba = lba / NSECT;
    int b_offset = lba % DEG_ZONE;
    lba = lba / DEG_ZONE;
    int p_offset = lba % NPAGE;
    lba = lba / NPAGE;
    int c_fcg = lba % NUM_FCG;

    int c_zone = lba;
    int c_bank = c_fcg * DEG_ZONE + b_offset;

    if(ZONE[c_zone][0] == 0 || ZONE[c_zone][0] == 1) {

      if(ZONE[c_zone][1] != c_lba)
        return -1;

      if(ZONE[c_zone][0] == 0) {
        if(FB[NBLK] == NBLK - MAX_OPEN_ZONE) return -1;
        ZTF[c_zone] = FB[0];

        for(int i = 0; i < NBLK-1; i++)
          FB[i] = FB[i+1];
        FB[NBLK-1] = -1;
        FB[NBLK] += 1; // use free blk!!
        ZONE[c_zone][0] = 1;
      }
     
      ZONE[c_zone][1]++;
      BUFFER[c_zone][c_sect] = data[i_sect];

      if(c_sect == NSECT-1) {
        u32 spare[] = {c_lba};
        nand_write(c_bank, ZTF[c_zone], p_offset, BUFFER[c_zone], spare);
      }

      if(ZONE[c_zone][1] == ZONE[c_zone][2] + DEG_ZONE * NSECT * NPAGE) //full checking
        ZONE[c_zone][0] = 2;
    }

    i_sect++;
  }
  return 0;
}

void zns_read(int start_lba, int nsect, u32 *data)
{
  int i_sect = 0;
  while(i_sect < nsect) {
    int c_lba = start_lba + i_sect;
    int lba = start_lba + i_sect;
    int c_sect = lba % NSECT;
    lba = lba / NSECT;
    int b_offset = lba % DEG_ZONE;
    lba = lba / DEG_ZONE;
    int p_offset = lba % NPAGE;
    lba = lba / NPAGE;
    int c_fcg = lba % NUM_FCG;

    int c_zone = lba;
    int c_bank = c_fcg * DEG_ZONE + b_offset;

    if(ZONE[c_zone][0] == 0)
      return;
    if(ZONE[c_zone][0] == 1) {
      if(ZONE[c_zone][1] <= c_lba) {
        data[i_sect] = -1;
        i_sect++;
        continue;
      }

      if( (ZONE[c_zone][1] / NSECT) * NSECT <= c_lba && c_sect != NSECT  )
       data[i_sect] = BUFFER[ c_zone ][c_sect];
      else {
        u32 r_data[NSECT];
        u32 spare[1];
        nand_read(c_bank, ZTF[c_zone], p_offset, r_data, spare);
        data[i_sect] = r_data[c_sect];
      }
    }

    i_sect++;
  }
  return;
}

int zns_reset(int lba)
{
  lba = lba / NSECT;
  int b_offset = lba % DEG_ZONE;
  lba = lba / DEG_ZONE;
  lba = lba / NPAGE;
  int c_fcg = lba % NUM_FCG;

  int c_zone = lba;
  int c_bank = c_fcg * DEG_ZONE + b_offset;

  if(ZONE[c_zone][0] != 2)
    return -1;

  ZONE[c_zone][0] = 0;
  ZONE[c_zone][1] = ZONE[c_zone][2];
  nand_erase(c_bank, ZTF[c_zone]);
  
  FB[ FB[NBLK] ] = ZTF[c_zone];
  FB[NBLK] += 1;
  ZTF[c_zone] = -1;

  return 0;
}

void zns_get_desc(int lba, int nzone, struct zone_desc *descs)
{
  return;
}

int zns_izc(int src_zone, int dest_zone, int copy_len, int *copy_list)
{
  return 0;
}

int zns_tl_open(int zone, u8 *valid_arr)
{
  return 0;
}