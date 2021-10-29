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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "zns.h"

static inline u32 get_data()
{
	return rand() & 0xff;
}

int main(int argc, char **argv)
{
	if (argc >= 2 && !freopen(argv[1], "r", stdin)) {
		perror("freopen in");
		return EXIT_FAILURE;
	}
	if (argc >= 3 && !freopen(argv[2], "w", stdout)) {
		perror("freopen out");
		return EXIT_FAILURE;
	}

	int seed, nbank, nblk, npage, dzone, max_open_zone;
	if (scanf("I %d %d %d %d %d %d", &seed, &nbank, &nblk, &npage, &dzone, &max_open_zone) < 6) {
		fprintf(stderr, "wrong input format\n");
		return EXIT_FAILURE;
	}
	srand(seed);
	zns_init(nbank, nblk, npage, dzone, max_open_zone);

	// print info
	puts("=================Setup==================");
	printf("Banks: %d\n", NBANK);
	printf("Blocks per Bank: %d\n", NBLK);
	printf("Pages per Block: %d\n", NPAGE);
	printf("Sectors per Page: %d\n", NSECT);
	printf("Max Zones: %d zones\n", MAX_ZONE);
	printf("Max LBA: %d sectors\n", MAX_LBA);
	printf("Zone Size: %d sectors\n", ZONE_SIZE);
	printf("Degree of Zone: %d\n", DEG_ZONE);
	printf("Max Open Zones: %d\n", MAX_OPEN_ZONE);
	puts("");

	puts("================Simulator==============");
	while (1) {
		int i;
		char op;
		u32 lba;
		u32 nsect;
		u32 nzone;
		u32 *buf;
		struct zone_desc *desc_buf;
		if (scanf(" %c", &op) < 1)
			break;
		switch (op) {
		case 'R':
			scanf("%d %d", &lba, &nsect);
                        assert(lba >= 0 && nsect > 0 && lba + nsect <= MAX_LBA);
			assert(lba_to_zone(lba) == lba_to_zone(lba + nsect - 1));
			buf = malloc(SECT_SIZE * nsect);
			assert(buf);
			zns_read(lba, nsect, buf);
			printf("Read(%u,%u): [ ", lba, nsect);
			for (i = 0; i < nsect; i++)
				printf("%2x ", buf[i]);
			printf("]\n");
			free(buf);
			break;
		case 'W':
			scanf("%d %d", &lba, &nsect);
                        assert(lba >= 0 && nsect > 0 && lba + nsect <= MAX_LBA);
			assert(lba_to_zone(lba) == lba_to_zone(lba + nsect - 1));
			buf = malloc(SECT_SIZE * nsect);
			assert(buf);
			for (i = 0; i < nsect; i++)
				buf[i] = get_data();
			if (zns_write(lba, nsect, buf) == 0) {
				printf("Write(%u,%u): [ ", lba, nsect);
				for (i = 0; i < nsect; i++)
					printf("%2x ", buf[i]);
				printf("]\n");
			} else {
				printf("Write(%u,%u): failed\n", lba, nsect);
			}
			free(buf);
			break;
		case 'E':
			scanf("%d", &lba);
			assert(lba % ZONE_SIZE == 0);
                        assert(lba >= 0 && lba < MAX_LBA);
			if (zns_reset(lba) == 0) {
				printf("Reset(%u)\n", lba);
			} else {
				printf("Reset(%u): failed\n", lba);
			}
			break;
		case 'D':
			scanf("%d %d", &lba, &nzone);
			assert(lba % ZONE_SIZE == 0);
			assert(lba_to_zone(lba) + nzone <= MAX_ZONE);
			desc_buf = malloc(sizeof(struct zone_desc) * nzone);
			assert(desc_buf);
			zns_get_desc(lba, nzone, desc_buf);
			printf("Get Zone Desc(%u, %u): [ ", lba, nzone);
			for (i = 0; i < nzone; i++)
				printf("%d:%d,%d,%d ", lba_to_zone(lba) + i,
						desc_buf[i].state, desc_buf[i].slba, desc_buf[i].wp);
			printf("]\n");
			free(desc_buf);
			break;
		case 'C': {
			int src_zone, dest_zone, copy_len;
			int *copy_list;
			scanf("%d %d %d", &src_zone, &dest_zone, &copy_len);
			assert(src_zone >= 0 && src_zone < MAX_ZONE);
			assert(dest_zone >= 0 && dest_zone < MAX_ZONE);
			copy_list = malloc(sizeof(int) * copy_len);
			assert(copy_list);
			for (i = 0; i < copy_len; i++)
				scanf("%d", &copy_list[i]);
			if (zns_izc(src_zone, dest_zone, copy_len, copy_list) == 0) {
				printf("Internal Zone Compaction(%d, %d)\n", src_zone, dest_zone);
			} else {
				printf("Internal Zone Compaction(%d, %d): failed\n", src_zone, dest_zone);
			}
			free(copy_list);
			break;
		}
		case 'T': {
			int i, j;
			int zone;
			u8 *valid_arr = malloc(ZONE_SIZE);
			assert(valid_arr);
			scanf("%d", &zone);
			assert(zone >= 0 && zone < MAX_ZONE);
			for (i = 0; i < ZONE_SIZE / NSECT; i++) {
				char v[9];
				scanf("%s", v);
				for (j = 0; j < NSECT; j++)
					valid_arr[NSECT * i + j] = v[j] - '0';
			}
			printf("TL Open(%d)\n", zone);
			zns_tl_open(zone, valid_arr);
			free(valid_arr);
			break;
		}
		default:
			fprintf(stderr, "Wrong op type\n");
			return EXIT_FAILURE;
		}
	}

	return 0;
}
