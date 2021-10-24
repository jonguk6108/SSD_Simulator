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

int pmt[N_BANKS][SIZE_BANK];
typedef unsigned int u32;

int main(int argc, char *argv[])
{
	int NBANKS, NBLOCKS, NPAGES;
	if (argc >= 2 && !freopen(argv[1], "r", stdin)) {
		perror("freopen in");
		return EXIT_FAILURE;
	}
	if (argc >= 3 && !freopen(argv[2], "w", stdout)) {
		perror("freopen out");
		return EXIT_FAILURE;
	}

	scanf("I %d %d %d", &NBANKS, &NBLOCKS, &NPAGES);

	if (nand_init(NBANKS, NBLOCKS, NPAGES)) {
		fprintf(stderr, "init failed\n");
		return EXIT_FAILURE;
	}

	printf("Init(%d,%d,%d): success\n", NBANKS, NBLOCKS, NPAGES);

	while (1) {
		int i;
		int rc;
		char op;
		int bank, blk, page;
		u32 data[PAGE_DATA_SIZE / sizeof(u32)] = { 0 };
		u32 spare[PAGE_SPARE_SIZE/ sizeof(u32)] = { 0 };

		if (scanf(" %c", &op) < 1)
			break;
		switch (op) {
		case 'R':
			scanf("%d %d %d", &bank, &blk, &page);
			printf("Read(%d,%d,%d): ", bank, blk, page);
			rc = nand_read(bank, blk, page, data, &spare);
			switch (rc) {
			case NAND_SUCCESS:
				printf("success, data = [ ");
				for (i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
					printf("%8x ", data[i]);
				printf("] spare = [ ");
				for (i = 0; i < PAGE_SPARE_SIZE / sizeof(u32); i++)
					printf("%8x ", spare[i]);
				printf("]\n");
				break;
			case NAND_ERR_INVALID:
				printf("failed, invalid address\n");
				break;
			case NAND_ERR_EMPTY:
				printf("failed, trying to read empty page\n");
				break;
			default:
				printf("Unkown return code\n");
			}
			break;
		case 'W':
			scanf("%d %d %d", &bank, &blk, &page);
			for (i = 0; i < PAGE_DATA_SIZE / sizeof(u32); i++)
				scanf("%x", &data[i]);
			for (i = 0; i < PAGE_SPARE_SIZE / sizeof(u32); i++)
				scanf("%x", &spare[i]);
			printf("Write(%d,%d,%d): ", bank, blk, page);
			rc = nand_write(bank, blk, page, data, spare);
			switch (rc) {
			case NAND_SUCCESS:
				printf("success\n");
				break;
			case NAND_ERR_INVALID:
				printf("failed, invalid address\n");
				break;
			case NAND_ERR_OVERWRITE:
				printf("failed, the page was already written\n");
				break;
			case NAND_ERR_POSITION:
				printf("failed, the page is not being sequentially written\n");
				break;
			default:
				printf("Unkown return code\n");
			}
			break;
		case 'E':
			scanf("%d %d", &bank, &blk);
			printf("Erase(%x,%x): ", bank, blk);
			rc = nand_erase(bank, blk);
			switch (rc) {
			case NAND_SUCCESS:
				printf("success\n");
				break;
			case NAND_ERR_INVALID:
				printf("failed, invalid address\n");
				break;
			case NAND_ERR_EMPTY:
				printf("failed, trying to erase a free block\n");
				break;
			}
			break;
		default:
			fprintf(stderr, "Wrong op type\n");
			return EXIT_FAILURE;
		}
	}

	return 0;
}

