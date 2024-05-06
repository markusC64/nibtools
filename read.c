/*
 * NIBTOOL read routines
 * Copyright 2001-2005 Markus Brenner <markus(at)brenner(dot)de>
 * and Pete Rittwage <peter(at)rittwage(dot)com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef DJGPP
#include <opencbm.h>
#endif

#include "mnibarch.h"
#include "gcr.h"
#include "nibtools.h"

static BYTE diskid[3];
extern int drivetype;

BYTE read_halftrack(CBM_FILE fd, int halftrack, BYTE * buffer)
{
	BYTE density;
    int i, newtrack;
	static int lasttrack = -1;
	static BYTE last_density = -1;

	newtrack = (lasttrack == halftrack) ? 0 : 1;
	lasttrack = halftrack;

	if(newtrack)
	{
		printf("\n%4.1f: ", (float) halftrack / 2);
		fprintf(fplog, "\n%4.1f: ", (float) halftrack / 2);

		step_to_halftrack(fd, halftrack);

		if(force_density)
			density = speed_map[halftrack/2];
		else if (Use_SCPlus_IHS)
			density = Scan_Track_SCPlus_IHS(fd, halftrack, buffer);  // deep scan track density (1541/1571 SC+ compatible IHS was initially checked)
		else
			density = scan_track(fd, halftrack);

		/* Set bitrate to the default density and scan for NOSYNC/KILLER */
		/* If you don't do this, some 1541-II and 1571 drives can timeout */
		/* because they see phantom syncs in empty tracks (no flux transitions) */
		set_bitrate(fd, density&3);
		send_mnib_cmd(fd, FL_SCANKILLER, NULL, 0);
		density |= burst_read(fd);
	}
	else
	{
		// this is the same track we just read
		density = last_density;
		printf("\n      ");
		fprintf(fplog, "\n      ");
	}

	/* output current density */
	printf("(%d",density&3);
	fprintf(fplog,"(%d",density&3);

	if ( (density&3) != speed_map[halftrack/2])
		printf("!=%d", speed_map[halftrack/2]);

	if(density & BM_FF_TRACK)
	{
		printf(" KILLER");
		fprintf(fplog, " KILLER!");
	}

	if(density & BM_NO_SYNC)
	{
		printf(" NOSYNC!");
		fprintf(fplog," NOSYNC!");
	}

	printf(") ");
	fprintf(fplog,") ");

	// bail if we don't want to read killer tracks
	// some drives/disks timeout
	if ((density & BM_FF_TRACK) && (!read_killer))
	{
		memset(buffer, 0xff, NIB_TRACK_LENGTH);
		return (density);
	}

	if((density) != last_density)
	{
		set_density(fd, density&3);
		if(verbose>2) printf("[D]");
		last_density = density;
	}

	for (i = 0; i < 3; i++)
	{
		// read track
		if((ihs) && (!(density & BM_NO_SYNC)))
			send_mnib_cmd(fd, FL_READIHS, NULL, 0);
		else if (Use_SCPlus_IHS) // "-j"
			send_mnib_cmd(fd, FL_IHS_READ_SCP, NULL, 0);
		else
		{
			if ((density & BM_NO_SYNC) || (density & BM_FF_TRACK) || (force_nosync))
				send_mnib_cmd(fd, FL_READWOSYNC, NULL, 0);
			else
				send_mnib_cmd(fd, FL_READNORMAL, NULL, 0);
		}
		burst_read(fd);

		if (burst_read_track(fd, buffer, NIB_TRACK_LENGTH))
			break;
		else
		{
			// If we got a timeout, reset the port before retrying.
			printf("!");
			fprintf(fplog,"(timeout) ");
			fflush(stdout);
			burst_read(fd);
			//delay(500);
			//printf("%c ", test_par_port(fd)? '+' : '-');
			burst_read(fd);
		}
	}

	if(i == 3)
	{
		printf("\n\nNo good read of track due to timeouts.  Aborting!\n");
		exit(1);
	}

	return (density);
}

BYTE paranoia_read_halftrack(CBM_FILE fd, int halftrack, BYTE * buffer)
{
	BYTE buffer1[NIB_TRACK_LENGTH];
	BYTE buffer2[NIB_TRACK_LENGTH];
	BYTE cbuffer1[NIB_TRACK_LENGTH];
	BYTE cbuffer2[NIB_TRACK_LENGTH];
	BYTE bbuffer[NIB_TRACK_LENGTH];
	BYTE *cbufn, *cbufo, *bufn, *bufo;
	BYTE align;
	size_t leno, lenn, gcr_diff;
	BYTE denso, densn;
	size_t i, l, badgcr, retries, errors, best;
	char errorstring[0x1000];

	badgcr = 0;
	errors = 0;
	retries = 3;
	best = NIB_TRACK_LENGTH;
	denso = 0;
	densn = 0;
	leno = 0;
	lenn = 0;
	bufn = buffer1;
	bufo = buffer2;
	cbufn = cbuffer1;
	cbufo = cbuffer2;

	errorstring[0] = '\0';

	// First pass at normal track read
	for (l = 0; l <= error_retries; l ++)
	{
		memset(bufo, 0, NIB_TRACK_LENGTH);
		denso = read_halftrack(fd, halftrack, bufo);

		// if we have a killer track, exit processing
		if(denso & BM_FF_TRACK)
		{
			printf("[Killer Track] ");
			fprintf(fplog, "[Killer Track] %s (%d)", errorstring, leno);
			memcpy(buffer, bufo, NIB_TRACK_LENGTH);
			return (denso);
		}

		// Find track cycle and length
		memset(cbufo, 0, NIB_TRACK_LENGTH);
		leno = extract_GCR_track(cbufo, bufo, &align, halftrack/2, capacity_min[denso & 3], capacity_max[denso & 3]);

		printf("%d ", leno);
		fprintf(fplog, "%d ", leno);

		// If we get nothing we are on an empty track (unformatted)
		if (!leno)
		{
			printf("[Unformatted Track] ");
			fprintf(fplog, "[Unformatted Track] %s (%d)", errorstring, leno);
			memcpy(buffer, bufo, NIB_TRACK_LENGTH);
			return (denso);
		}

		/* keep best track cycle in case we don't get another good one
			1) disk is destroyed during reading)
			2) subsequest reads show no valid cycle
		*/
		if(leno < best)
		{
			best = leno;
			memcpy(bbuffer, bufo, NIB_TRACK_LENGTH);
		}

		// if we get less than what a track holds,
		// try again, probably bad read or a bad GCR match
		if (leno < capacity_min[denso & 3] - CAP_ALLOWANCE)
		{
			printf("Short Read! ");
			fprintf(fplog, "[%d<%d!] ", leno, capacity_min[denso & 3] - CAP_ALLOWANCE);
			//if(l < (error_retries - 3)) l = error_retries - 3;
			//continue;
		}

		// if we get more than capacity
		// try again to make sure it's intentional
		if (leno > capacity_max[denso & 3] + CAP_ALLOWANCE)
		{
			printf("Long Read! ");
			fprintf(fplog, "[%d>%d!] ", leno, capacity_max[denso & 3] + CAP_ALLOWANCE);
			//if(l < (error_retries - 3)) l = error_retries - 3;
			//continue;
		}

		// check for CBM DOS errors
		errors = check_errors(cbufo, leno, halftrack, diskid, errorstring);
		fprintf(fplog, "%s", errorstring);

		// If there are a lot of errors, the track probably doesn't contain
		// any CBM sectors (protection)
		if(!errors)
			printf("[CBM OK]");
		else if ((errors == sector_map[halftrack/2]) || (halftrack > 70))
			printf("[NDOS] ");
		else
			printf("%s", errorstring);

		// if we got all good sectors we dont retry
		if (errors == 0) break;

		// all bad sectors (protection) and we have a valid cycle
		if ((errors == sector_map[halftrack/2]) &&
			(leno < NIB_TRACK_LENGTH) && (l > 0) )
			break;

		// all bad sectors (protection) and no cycle, we limit retries
		if ((errors == sector_map[halftrack/2]) && (leno == NIB_TRACK_LENGTH))
		{
			if(l < (error_retries - 1))	l = error_retries - 1;
		}
	}

	/* keep best cycle if ended with none */
	if((leno == NIB_TRACK_LENGTH) && (best < leno))
	{
		printf(" (reverted) ");
		memcpy(bufo, bbuffer, NIB_TRACK_LENGTH);
	}

	// Fix bad GCR in track for compare
	if ((badgcr = check_bad_gcr(cbufo, leno)) != 0)
	{
		printf(" (weakgcr:%d) ", badgcr);
		fprintf(fplog, " (weakgcr:%d) ", badgcr);
	}

	if(track_match)
	{
		// Try to verify our read

		// Don't bother to compare unformatted or bad data
		if (leno == NIB_TRACK_LENGTH) retries = 0;

		// normal data, verify
		for (i = 0; i < retries; i++)
		{
			memset(bufn, 0, NIB_TRACK_LENGTH);
			densn = read_halftrack(fd, halftrack, bufn);

			memset(cbufn, 0, NIB_TRACK_LENGTH);
			lenn = extract_GCR_track(cbufn, bufn, &align, halftrack/2, capacity_min[densn & 3], capacity_max[densn & 3]);

			printf("%d ", lenn);
			fprintf(fplog, "%d ", lenn);

			// Fix bad GCR in track for compare
			if ((badgcr = check_bad_gcr(cbufn, lenn)) != 0)
			{
				//printf("(weakgcr:%d)", badgcr);
				//fprintf(fplog, "(weakgcr:%d) ", badgcr);
			}

			// compare raw gcr data
			gcr_diff = compare_tracks(cbufo, cbufn, leno, lenn, 1, errorstring);
			if(verbose) printf("VERIFY: diff:%.4d ", (int)gcr_diff);
			fprintf(fplog, "VERIFY: diff:%.4d ", (int)gcr_diff);
			if(gcr_diff <= 10)
			{
				if(verbose) printf("OK ");
				break;
			}

			// compare sector data
			if (compare_sectors(cbufo, cbufn, leno, lenn, diskid, diskid, halftrack, errorstring) == sector_map[halftrack/2])
			{
				if(verbose) printf(" - sector match ");
				fprintf(fplog, " - sector match ");
				break;
			}
			else
			{
				if(verbose) printf(" - NO sector match ");
				fprintf(fplog, " - NO sector match ");
				fprintf(fplog, "%s", errorstring);
				if(verbose) printf("%s", errorstring);
			}
		}
	}

	//printf("\n");
	fprintf(fplog, "%s (%d)", errorstring, leno);
	memcpy(buffer, bufo, NIB_TRACK_LENGTH);
	return denso;
}

int
read_floppy(CBM_FILE fd, BYTE *track_buffer, BYTE *track_density, size_t *track_length)
{
    int track;
    //size_t errors = 0;
    //char errorstring[0x1000];

	printf("\n");
	fprintf(fplog,"\n");

	if(!rawmode) get_disk_id(fd);

	for (track = start_track; track <= end_track; track += track_inc)
		track_density[track] = paranoia_read_halftrack(fd, track, track_buffer + (track * NIB_TRACK_LENGTH));

	step_to_halftrack(fd, 18*2);
	return 1;
}

int write_nb2(CBM_FILE fd, char * filename)
{
	BYTE density;
	FILE * fpout;
	int track, i, header_entry, pass;
	BYTE pass_density;
	BYTE buffer[NIB_TRACK_LENGTH];
	char header[0x100];

	printf("\n");
	fprintf(fplog,"\n");

	/* create output file */
	if ((fpout = fopen(filename, "wb")) == NULL)
	{
		fprintf(stderr, "Couldn't create output file %s!\n", filename);
		return 0;
	}

	/* write initial NIB-header */
	memset(header, 0x00, sizeof(header));
	sprintf(header, "MNIB-1541-RAW%c%c%c", 2, 0, 1);

	if (fwrite(header, sizeof(header), 1, fpout) != 1) {
		printf("unable to write NB2 header\n");
		return 0;
	}

	get_disk_id(fd);

	header_entry = 0;
	for (track = start_track; track <= end_track; track += track_inc)
	{
		memset(buffer, 0, sizeof(buffer));

		density = read_halftrack(fd, track, buffer);
		//density = paranoia_read_halftrack(fd, track, buffer);
		printf("\n");

		header[0x10 + (header_entry * 2)] = (BYTE) track;
		header[0x10 + (header_entry * 2) + 1] = density;
		header_entry++;

		step_to_halftrack(fd, track);

		/* make 16 passes of track, four for each density */
		for(pass_density = 0; pass_density < 4; pass_density ++)
		{
			printf("%4.1f: (%d) ", (float) track / 2, pass_density);
			fprintf(fplog, "%4.1f: (%d) ", (float) track / 2, pass_density);

			for(pass = 0; pass < 4; pass ++)
			{
				set_density(fd, pass_density);

				for (i = 0; i < 10; i++)
				{
					send_mnib_cmd(fd, FL_READWOSYNC, NULL, 0);
					burst_read(fd);

					if (burst_read_track(fd, buffer, NIB_TRACK_LENGTH))
						break;
					else
					{
						// If we got a timeout, reset the port before retrying.
						//putchar('?');
						fflush(stdout);
						burst_read(fd);
						//delay(500);
						//printf("%c ", test_par_port(fd)? '+' : '-');
						burst_read(fd);
					}
				}

				/* save track to disk */
				if (fwrite(buffer, sizeof(buffer), 1, fpout) != 1)
				{
					printf("unable to rewrite NIB track data\n");
					fclose(fpout);
					return 0;
				}
				fflush(fpout);
				printf("%d ", pass+1);
			}

			printf("\n");
			fprintf(fplog,"\n");
		}
	}

	/* fill NB2-header */
	rewind(fpout);
	if (fwrite(header, sizeof(header), 1, fpout) != 1)
	{
		printf("unable to rewrite NB2 header\n");
		return 0;
	}

	fclose(fpout);
	step_to_halftrack(fd, 18 * 2);
	return 1;
}

void get_disk_id(CBM_FILE fd)
{
		BYTE buffer[NIB_TRACK_LENGTH];

		/* read track 18 for ID checks*/
		read_halftrack(fd, 18 * 2, buffer);

		/* print cosmetic disk id */
		memset(diskid, 0, sizeof(diskid));
		printf("\nCosmetic Disk ID: ");

		if(!extract_cosmetic_id(buffer, diskid))
			fprintf(stderr, "[Cannot find directory sector!]\n");
		else
		{
			printf("'%c%c'\n", diskid[0], diskid[1]);
			fprintf(fplog, "\nCID: '%c%c'\n", diskid[0], diskid[1]);
		}

		/* determine format id for e11 checks */
		memset(diskid, 0, sizeof(diskid));
		printf("Format Disk ID: ");

		if (!extract_id(buffer, diskid, NIB_TRACK_LENGTH))
			fprintf(stderr, "[Cannot find directory sector!]\n");
		else
		{
			printf("'%c%c'\n", diskid[0], diskid[1]);
			fprintf(fplog, "FID: '%c%c'\n", diskid[0], diskid[1]);
		}

		printf("\n");
		fprintf(fplog,"\n");
}

/* $152b Density Scan */
BYTE
scan_track(CBM_FILE fd, int track)
{
	BYTE density, killer_info;
	BYTE scanned_density = 0xff;
	BYTE count;
	BYTE density_major[4], iMajorMax; /* 50% majorities for bit rate */
	BYTE density_stats[4], iStatsMax; /* total occurrences */
	int bin, i;

	/* Scan for killer track */
	density = set_default_bitrate(fd, track);
	send_mnib_cmd(fd, FL_SCANKILLER, NULL, 0);
	killer_info = burst_read(fd);

	if (killer_info & BM_FF_TRACK)
			return (density | killer_info);

	/* Floppy sends statistic data in reverse bit-rate order */
	for(i=0; i<10; i++)
	{
		memset(density_major, 0, sizeof(density_major));
		memset(density_stats, 0, sizeof(density_stats));

		/* Use medium bitrate for scan */
		//set_bitrate(fd, 2);
		set_bitrate(fd, (track/2 < 25) ? 2 : 1);
		send_mnib_cmd(fd, FL_SCANDENSITY, NULL, 0);

		for (bin=3; bin>=0; bin--)
		{
			count = burst_read(fd);

			if (count >= 0x40)
				density_major[bin]++;

			density_stats[bin] += count;
		}
		burst_read(fd);

		// calculate
		iMajorMax = iStatsMax = 0;
		for (bin=0; bin<=3; bin++)
		{
			if (density_major[bin] > density_major[iMajorMax])
				iMajorMax = (BYTE) bin;
			if (density_stats[bin] > density_stats[iStatsMax])
				iStatsMax = (BYTE) bin;
		}

		if (density_major[iMajorMax] > 0)
			scanned_density = iMajorMax;
		else if (density_stats[iStatsMax] > density_stats[density])
			scanned_density = iStatsMax;

		if(scanned_density == speed_map[track/2])
			break;
	}

	if(scanned_density == 0xff)
	{
		density = speed_map[track/2];
		printf("{NONGCR:%d}",density);
	}
	else
		density = scanned_density;

	return (density | killer_info);
}

// Track Alignment Report, by Arnd
int TrackAlignmentReport(CBM_FILE fd)
{
	int i, m, track, res, NumSync;
	BYTE density;
	BYTE EvenTrack;
	int dump_retry = 10;
	BYTE buffer[NIB_TRACK_LENGTH];

	/* this check is temporary for now */
	if(drivetype != 1571)
	{
		printf("Only 1571 index hole sensor supported.\n");  /* for now */
		exit(0);
	}

	printf("\nStarting Track Alignment Analysis.\n");
	printf("Make sure a disk is in the drive turned to side 1 ONLY!\n\n");

	if (track_inc == 1)
	{
		printf("    |           Full Track           |       Half Track (+0.5)       \n");
		printf(" #T +--------------------------------+-------------------------------\n");
		printf(" RA |      pre  #sync   data bytes   |      pre  #sync   data bytes  \n");
		printf(" CK | BR  lo hi lo hi A1 A2 A3 A4 A5 | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
		printf("----+--------------------------------+-------------------------------");
	}
	else
	{
		printf("    |             Full Track        \n");
		printf("    |      pre  #sync   data bytes  \n");
		printf("    | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
		printf("----+-------------------------------");
	}

	motor_on(fd);

	for (track = start_track; track <= end_track; track += track_inc)
	{
		step_to_halftrack(fd, track);
		density = scan_track(fd, track);
		set_density(fd, density&3);

		if ( (EvenTrack =(track == (track/2)*2)) )
			printf("\n %2.2d ", track/2);
		if (density & BM_FF_TRACK)
		{
			printf("| <*> KILLER                     ");
			continue;
		}
		else if (density & BM_NO_SYNC)
		{
			printf("| <*> NOSYNC                     ");
			continue;
		}
		else
			printf("| <%d> ", density);

		// Try to get track dump "dump_retry" times
		for (i = 0; i < dump_retry; i++)
		{
			memset(buffer, 0x00, NIB_TRACK_LENGTH);
			send_mnib_cmd(fd, FL_READIHS, NULL, 00);
			burst_read(fd);

			res = cbm_parallel_burst_read_track(fd, buffer, NIB_TRACK_LENGTH);
			if (!res)
			{
				printf("(timeout #%d: T%d D%d)", i+1, track, density); // &3
				fflush(stdout);
				burst_read(fd);
				delay(500);
				burst_read(fd);
			}
			else
				break;
		}
		if (res)
		{
			// Evaluate track image
			// Find first sync (we checked for NOSYNC)
			for (i = 0; i < NIB_TRACK_LENGTH; i++)
				if (buffer[i] == 0xFF) break;

			// Print number of data bytes before first sync
			printf("%2.2X %2.2X ", i%256, i/256); // lo/hi

			// Find end of sync, count 0xFFs
			NumSync = 0;
			for (m = i; m < NIB_TRACK_LENGTH; m++)
				if (buffer[m] == 0xFF) NumSync++;
				else break;

			// Print number of 0xFF sync bytes
			printf("%2.2X %2.2X ", NumSync%256, NumSync/256); // lo/hi

			// Dump first 5 data bytes after sync
			for (i = 0; i < 5; i++)
			{
				if (m+i >= NIB_TRACK_LENGTH)
					printf("   ");
				else // (0xFF possible)
					printf("%2.2X ", buffer[m+i]);
			}
		}
		else
		{
			// Call to "burst_read_track" was 10x unsuccessful.
			printf("\nToo many errors.");
			exit(2);
		}
	}
	printf("\n");

	exit(1);
}
