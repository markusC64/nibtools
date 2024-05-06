/* NIBTOOLS IHS routines */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include "mnibarch.h"
#include "gcr.h"
#include "nibtools.h"

int Use_SCPlus_IHS = 0;          // "-j"
int track_align_report = 0;      // "-x"
int Deep_Bitrate_SCPlus_IHS = 0; // "-y"
int Test_SCPlus_IHS = 0;         // "-z"
BYTE IHSresult = 0;
int use_floppycode_ihs = 0;
char *logline;

BYTE DBG_IHS = 0; // set this to "1" for IHS debugging messages

static BYTE read_mem(CBM_FILE fd, unsigned short addr);
static BYTE get_default_bitrate(int track);

// Check for 1541/1571 (SC+ compatible) IHS presence
//  0 = found
//  8 = hole not detected, or IHS not working, or IHS not present
// 16 = IHS disabled (must enable first!)
BYTE Check_SCPlus_IHS(CBM_FILE fd, BYTE KeepOn)
{
	BYTE ihs_detected = 8;
	BYTE buffer1[NIB_TRACK_LENGTH];
	BYTE *buffer = buffer1;
	int i, res;

	motor_on(fd);

	// turn SC+ IHS on
	send_mnib_cmd(fd, FL_IHS_ON, NULL, 0);
	burst_read(fd);

	for (i = 0; i < 10; i++)
	{
		memset(buffer, 0x55, NIB_TRACK_LENGTH);

		// check SC+ IHS presence
		send_mnib_cmd(fd, FL_IHS_PRESENT, NULL, 0);
		burst_read(fd);

		res = burst_read_track(fd, buffer, NIB_TRACK_LENGTH);

		if (!res)
		{
			printf("(timeout) ");
			fprintf(fplog, "(timeout) ");
			fflush(stdout);
			burst_read(fd);
			//delay(500);
			burst_read(fd);
		}
		else
			break;
	}

	if (res)
	{
		for (i = 0; i < NIB_TRACK_LENGTH; i++)
		{
			if (buffer[i] == 0) ihs_detected = 0; // 0 = found, 8 = not found
			else if (buffer[i] != 8)
			{
				ihs_detected = buffer[i]; // 16 = IHS disabled
				break;                    // everything else = unknown error
			}
		}
	}
	else ihs_detected = 99; // unknown error

	if (KeepOn == 0)
	{
		// turn SC+ IHS off
		send_mnib_cmd(fd, FL_IHS_OFF, NULL, 0);
		burst_read(fd);
	}

	return (ihs_detected);
}


// Check for 1541/1571 (SC+ compatible) IHS presence
//  0 = found
//  8 = hole not detected, or IHS not working, or IHS not present
// 16 = IHS disabled (must enable first!)
BYTE Check_SCPlus_IHS_2(CBM_FILE fd, BYTE KeepOn)
{
	BYTE ihs_detected = 1;

	motor_on(fd);

	// turn SCPlus IHS on
	send_mnib_cmd(fd, FL_IHS_ON, NULL, 0);
	burst_read(fd);

	// check SCPlus IHS presence
	send_mnib_cmd(fd, FL_IHS_PRESENT2, NULL, 0);
	ihs_detected = burst_read(fd);

	if (KeepOn == 0)
	{
		// turn SCPlus IHS off
		send_mnib_cmd(fd, FL_IHS_OFF, NULL, 0);
		burst_read(fd);
	}

	return (ihs_detected);
}


void OutputIHSResult(int ToConsole, int ToLogfile, BYTE IHSresult, FILE *fplog)
{
	if (ToLogfile) fprintf(fplog, "\nTesting 1541/1571 Index Hole Sensor (SC+ compatible).\n");
	switch (IHSresult)
	{
		case 0:
			if (ToConsole) printf("\nIndex Hole Sensor detected.\n");
			if (ToLogfile) fprintf(fplog, "Index Hole Sensor detected.\n");
			break;
		case 0x10:
			if (ToConsole) printf("\nIHS Error #%d: IHS not enabled.\n",IHSresult);
			if (ToLogfile) fprintf(fplog,"IHS Error #%d: IHS not enabled.\n",IHSresult);
			break;
		case 0x08:
			if (ToConsole) printf("\nIHS Error #%d: Index Hole not detected, sensor not working or not present.\nWrong disk side?\n",IHSresult);
			if (ToLogfile) fprintf(fplog,"IHS Error #%d: Index Hole not detected, Sensor not working or not present.\nWrong disk side?\n",IHSresult);
			break;
		default:
			if (ToConsole) printf("\nIHS Error #%d: Unknown error.\n",IHSresult);
			if (ToLogfile) fprintf(fplog,"IHS Error #%d: Unknown error.\n",IHSresult);
	}
}


// Track Alignment Report
// First version with timeouts in drive code
// requires 1541/1571 SC+ compatible IHS
int TrackAlignmentReport2(CBM_FILE fd, BYTE *buffer)
{
	int i, m, track, ht, res, NumSync;
	BYTE density;
	int EvenTrack;
	int dump_retry = 10;

	if (Check_SCPlus_IHS(fd,1) != 0)
	{
		fprintf(fplog, "Error: Index Hole Sensor *NOT* detected!\n");
		printf("\nError: Index Hole Sensor *NOT* detected!\n");
		goto finish1;
	}

	fprintf(fplog, "\nIndex Hole Sensor detected. Starting Track Alignment Analysis.\n\n");
	printf("\nIndex Hole Sensor detected. Starting Track Alignment Analysis.\n\n");

	if (track_inc == 1)
	{
		printf("    |           Full Track           |       Half Track (+0.5)       \n");
		printf(" #T +--------------------------------+-------------------------------\n");
		printf(" RA |      pre  #sync   data BYTEs   |      pre  #sync   data BYTEs  \n");
		printf(" CK | BR  lo hi lo hi A1 A2 A3 A4 A5 | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
		printf("----+--------------------------------+-------------------------------");
		fprintf(fplog, "    |           Full Track           |       Half Track (+0.5)       \n");
		fprintf(fplog, " #T +--------------------------------+-------------------------------\n");
		fprintf(fplog, " RA |      pre  #sync   data BYTEs   |      pre   #sync  data BYTEs  \n");
		fprintf(fplog, " CK | BR  lo hi lo hi A1 A2 A3 A4 A5 | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
		fprintf(fplog, "----+--------------------------------+-------------------------------");
	}
	else
	{
		printf("    |             Full Track        \n");
		printf("    |      pre  #sync   data BYTEs  \n");
		printf("    | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
		printf("----+-------------------------------");
		fprintf(fplog, "    |             Full Track\n");
		fprintf(fplog, "    |      pre  #sync   data BYTEs  \n");
		fprintf(fplog, "    | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
		fprintf(fplog, "----+-------------------------------");
	}

	// Don't forget to analyze final half track if half tracks are enabled
	ht = (track_inc == 1) ? 1 : 0;

	for (track = start_track; track <= end_track+ht; track += track_inc)
	{
		step_to_halftrack(fd, track);

		// deep scan track density (1541/1571 SC+ compatible IHS)
		density = Scan_Track_SCPlus_IHS(fd, track, buffer);
		set_density(fd, density&3);

		if ((EvenTrack = (track == (track/2)*2)) )
		{
			printf("\n %2.2d ", track/2);
			fprintf(fplog, "\n %2.2d ", track/2);
		}

		if (density & BM_FF_TRACK)
		{
			printf("| <*> KILLER                     ");
			fprintf(fplog, "| <*> KILLER                     ");
			continue;
		}
		else if (density & BM_NO_SYNC)
		{
			printf("| <*> NOSYNC                     ");
			fprintf(fplog, "| <*> NOSYNC                     ");
			continue;
		}
		else
		{
			printf("| <%d> ", density);
			fprintf(fplog, "| <%d> ", density);
		}

		// Try to get track dump "dump_retry" times
		for (i = 0; i < dump_retry; i++)
		{
			memset(buffer, 0x00, NIB_TRACK_LENGTH);

			send_mnib_cmd(fd, FL_IHS_READ_SCP, NULL, 0);
			burst_read(fd);

			res = burst_read_track(fd, buffer, NIB_TRACK_LENGTH);

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

			// for (i = 0; i < NIB_TRACK_LENGTH; i++) // a single 0xFF
			// if (buffer[i] == 0xFF) break;          //    may be a data BYTE
			i = 0;
			if (buffer[i] != 0xFF)
			{
				for (i = 1; i < NIB_TRACK_LENGTH-1; i++)
					if ((buffer[i] == 0xFF) && (buffer[i+1] == 0xFF)) break;
			}
			if (i == NIB_TRACK_LENGTH-1) i++; // final single 0xFF is handled as data BYTE here

			// Print number of data BYTEs before first sync
			printf("%2.2X %2.2X ", i%256, i/256); // lo/hi
			fprintf(fplog, "%2.2X %2.2X ", i%256, i/256); // lo/hi

			// Find end of sync, count 0xFFs
			NumSync = 0;
			for (m = i; m < NIB_TRACK_LENGTH; m++)
				if (buffer[m] == 0xFF) NumSync++;
				else break;

			// Print number of 0xFF sync BYTEs
			printf("%2.2X %2.2X ", NumSync%256, NumSync/256); // lo/hi
			fprintf(fplog, "%2.2X %2.2X ", NumSync%256, NumSync/256); // lo/hi

			// Dump first 5 data BYTEs after sync
			for (i = 0; i < 5; i++)
			{
				if (m+i >= NIB_TRACK_LENGTH)
				{
					printf("   ");
					fprintf(fplog, "   ");
				}
				else // (0xFF possible)
				{
					printf("%2.2X ", buffer[m+i]);
					fprintf(fplog, "%2.2X ", buffer[m+i]);
				}
			}
		}
		else
		{
			// Call to "burst_read_track" was 10x unsuccessful.
			printf("\nToo many errors.");
			fprintf(fplog, "\nToo many errors.");
			exit(2);
		}
	}
	printf("\n");
	fprintf(fplog, "\n");

finish1:

	// turn SCPlus IHS off
	send_mnib_cmd(fd, FL_IHS_OFF, NULL, 0);
	burst_read(fd);

	return 0;
}


// Deep Bitrate Analysis
// requires 1541/1571 SC+ compatible IHS
int DeepBitrateAnalysis(CBM_FILE fd, char *filename, BYTE *buffer, char *logline)
{
	int i, m, track, res;
	BYTE density, killer_info, t1;
	DWORD dw;
	unsigned short NumSync;
	BYTE header[0x200], thdr[7];
	char fname[256];
	FILE * fpout[4];
	int dump_retry = 10;

	if (Check_SCPlus_IHS(fd,1) != 0)
	{
		fprintf(fplog, "Error: Index Hole Sensor *NOT* detected!\n");
		printf("\nError: Index Hole Sensor *NOT* detected!\n");
		goto finish2;
	}

	fprintf(fplog, "\nIndex Hole Sensor detected. Starting Deep Bitrate Scan.\n\n");
	printf("\nIndex Hole Sensor detected. Starting Deep Bitrate Scan.\n\n");

	// Create and init raw bitrate dump files
	for (density=0; density < 4; density++)
	{
		sprintf(fname, "%s.br%d", filename, (int)density);
		if ((fpout[density] = fopen(fname, "wb")) == NULL)
		{
			printf( "Error creating deep scan dump file %s.\n", fname);
			exit(2);
		}

		// Write initial BRX-header
		memset(header, 0x00, sizeof(header));
		sprintf(header, "BR%d-1541",density);
		header[8] = 0;  // BRX file version number
		header[9] = (BYTE) 84;  // max number of halftracks
		header[10] = (BYTE) (NIB_TRACK_LENGTH % 256);   // Size of each stored track = 8KByte
		header[11] = (BYTE) (NIB_TRACK_LENGTH / 256);
		// header[12..15] = 0;

		if (fwrite(header, sizeof(header), 1, fpout[density]) != 1)
		{
			fprintf(stderr, "Cannot write BR%d header.\n", density);
			exit(2);
		}

		// Update file offset
		dw = 0x0200;
	}

	for (track = start_track; track <= end_track; track += track_inc)
	{
		step_to_halftrack(fd, track);

		for (density = 0; density <= 3; density++)
		{
			set_density(fd, density);

			printf("T%2.1f: ", (float) track/2);
			fprintf(fplog, "T%2.1f: ", (float) track/2);
			if (track < 10*2)
			{
				printf(" ");
				fprintf(fplog, " ");
			}
			printf("<%d> ", density);
			fprintf(fplog, "<%d> ", density);

			// Scan for killer track
			send_mnib_cmd(fd, FL_SCANKILLER, NULL, 0);
			killer_info = burst_read(fd);

			if (killer_info & BM_FF_TRACK)
			{
				printf("KILLER\n");
				fprintf(fplog, "KILLER\n");
				// Write killer track to BRX file
				memset(buffer, 0xFF, NIB_TRACK_LENGTH);
				// Set end-of-data marker 0x55
				buffer[NIB_TRACK_LENGTH-7] = (BYTE) 0x55;
			}
			else
			{
				// Try to get bitrate dump "dump_retry" times
				for (i = 0; i < dump_retry; i++)
				{
					memset(buffer, 0x00, NIB_TRACK_LENGTH);

					send_mnib_cmd(fd, FL_DBR_ANALYSIS, NULL, 0);
					burst_read(fd);

					res = burst_read_n(fd, buffer, NIB_TRACK_LENGTH-6);

					if (!res)
					{
						printf("(deep scan timeout #%d: T%d D%d)", i+1, track, density);
						fprintf(fplog, "(deep scan timeout #%d: T%d D%d)", i+1, track, density);
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
					// Read number of sync marks
					NumSync = ( ((unsigned short) read_mem(fd, 0xcd)) << 8 ) | read_mem(fd, 0xcc);

					// Evaluate density dump
					BitrateStats( buffer, logline, NumSync );
					printf("\n");
					fprintf(fplog, "\n");
				}
				else
				{
					// Call to "burst_read_n" was 10x unsuccessful.
					// BRX files may be asynchronous.
					printf("Too many errors.\n");
					fprintf(fplog, "Too many errors.\n");
					exit(2);
				}
			}

			// Write track to BRX file. Regardless if bitrate data, killer or no sync.

			// Write bitrate track header
			sprintf(thdr, "[%4.1f]", (float)track/2 );
			if (fwrite(thdr, 6, 1, fpout[density]) != 1)
			{
				fprintf(stderr, "Cannot write track [%d] header (BR%d).\n", track, density);
				exit(2);
			}

			// Save raw bitrate track to BRX file
			if (fwrite(buffer, NIB_TRACK_LENGTH-6, 1, fpout[density]) != 1)
			{
				fprintf(stderr, "Cannot write track [%d] data (BR%d).\n", track, density);
				exit(2);
			}

		} // for (density = 0; density <= 3; density++)

		if (DBG_IHS)
		{
			// Print "best" density for reading,
			t1 = Scan_Track_SCPlus_IHS(fd, track, buffer);
			printf(">>%d\n", t1);
			fprintf(fplog, ">>%d\n", t1);
		}

		// Register track in BRX header index
		for (m=0; m < 4; m++)
			header[ 16 + ((track-2) * 4) + m ] = (dw >> m*8) & 0xff;

		// Update file offset
		dw += NIB_TRACK_LENGTH; // = 0x2000

		printf("\n");
		fprintf(fplog, "\n");

	} // for (track = start_track; track <= end_track; track += track_inc)

	// Update BRX headers and close
	for (density=0; density < 4; density++)
	{
		rewind(fpout[density]);
		sprintf(header, "BR%d-1541", density);
		if (fwrite(header, sizeof(header), 1, fpout[density]) != 1)
		{
			printf("Error updating BRX header (BR%d).\n", density);
			exit(2);
		}
		fclose(fpout[density]);
	}

finish2:

	// turn SCPlus IHS off
	send_mnib_cmd(fd, FL_IHS_OFF, NULL, 0);
	burst_read(fd);

	return 0;
}


// Bitrate Statistics
// buffer = {{0-5}FF}55
int BitrateStats(BYTE *buffer, char *logline, unsigned short NumSync)
{
	char workstr[50];
	int j, k, m, total, subtotal, avr, SyncNum;
	int SectorStats[NIB_TRACK_LENGTH], TrackStats[6], SectorAverages[NIB_TRACK_LENGTH], work[6];

	logline[0]        = '\0';
	SectorStats[0]    = '\0';
	SectorAverages[0] = '\0';
	k=0;
	total=0;
	SyncNum=0;
	for (j=0; j < 6; j++)
	{
		TrackStats[j]=0;
		work[j]=0;
	}

	for (j=0; j < NIB_TRACK_LENGTH-6; j++)
	{
		if (buffer[j] == 0x55)
		{
			SectorStats[k++] = 0x55;
			strcat(logline, "<55>");
			// Buffer needs cleaning before dumping to disk
			for (m=j+1; m < NIB_TRACK_LENGTH; m++) buffer[m] = 0;
				break;
		}

		if (buffer[j] != 0xFF) work[buffer[j]]++;
		else
		{
			SyncNum += 1;
			subtotal=0; avr=0;
			strcat(logline, "[");
			for (m=0; m < 6; m++)
			{
				SectorStats[k++] = work[m];
				TrackStats[m]+= work[m];
				sprintf(workstr, "%.4d",work[m]);
				if (m < 6) strcat(workstr, ",");
				strcat(logline, workstr);
				subtotal+= work[m];
				avr+= work[m]*(m+1);
				work[m] = 0;
			}
			total+= subtotal;
			SectorStats[k++] = 0xFF;
			strcat(logline, "FF]");
			if (subtotal == 0) sprintf(workstr, "<=0.0> ");
			else sprintf(workstr, "<=%1.1f> ", (float) avr/subtotal-1);
			strcat(logline, workstr);
		}
	}

	if (SyncNum != 0)
	{
		// Print Track "length"
		printf("[%4.1d] ",total);
		fprintf(fplog, "[%4.1d] ",total);

		if (NumSync > 256)
		{
			printf("(%4.1d!) <",NumSync);
			fprintf(fplog, "(%4.1d!) <",NumSync);
		}
		else
		{
			// Print number of detected Syncs
			printf("(%4.1dS) <",SyncNum);
			fprintf(fplog, "(%4.1dS) <",SyncNum);
		}

		// Print TrackStat
		subtotal=0;
		for (m=0; m < 6; m++)
		{
			printf("%4.1d",TrackStats[m]);
			fprintf(fplog, "%4.1d",TrackStats[m]);
			subtotal+= TrackStats[m]*(m+1);
			if (m < 5)
			{
				printf(",");
				fprintf(fplog, ",");
			}
		}
		if (total == 0) total = subtotal = 1;
		printf("> <=%1.1f>", (float) subtotal/total-1);
		fprintf(fplog, "><=%1.1f> ", (float) subtotal/total-1);

		// Print SectorStats to logfile
		fprintf(fplog, logline);
	}
	else
	{
		printf("NOSYNC");
		fprintf(fplog, "NOSYNC");
	}

	return(0);
}


// Deep Density Scan Track
// requires 1541/1571 SC+ compatible IHS
BYTE
Scan_Track_SCPlus_IHS(CBM_FILE fd, int track, BYTE *buffer)
{
	BYTE density, killer_info;
	int j, m, MaxDenso, MaxNum, res;
	int TrackStats[4][6], Killer[4];
	int dump_retry = 10;

	for (density = 0; density <= 3; density++)
	{
		Killer[density] = 0;
		for (j = 0; j <= 5; j++)
			TrackStats[density][j] = 0;
	}

	if (DBG_IHS)
		printf("Scan_Track_SCPlus_IHS:\n");

	for (density = 0; density <= 3; density++)
	{
		set_density(fd, density);

		/* Scan for killer track! */
		send_mnib_cmd(fd, FL_SCANKILLER, NULL, 0);
		killer_info = burst_read(fd);

		if (killer_info & BM_FF_TRACK)
		{
			Killer[density] = 1;
			if (DBG_IHS)
			{
				printf("D%d:Killer!\n",density);
				fprintf(fplog, "D%d:Killer!\n",density);
			}
		}
		else
		{
			// Try to get bitrate dump "dump_retry" times
			for (m = 0; m < dump_retry; m++)
			{
				memset(buffer, 0x00, NIB_TRACK_LENGTH);

				send_mnib_cmd(fd, FL_DBR_ANALYSIS, NULL, 0);
				burst_read(fd);

				res = burst_read_n(fd, buffer, NIB_TRACK_LENGTH);

				if (!res)
				{
					printf("(deep scan timeout #%d: T%.1f D%d)", m+1, (float) track/2, density);
					fprintf(fplog, "(deep scan timeout #%d: T%.1f D%d)", m+1, (float) track/2, density);
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
				for (j=0; j < NIB_TRACK_LENGTH; j++)
				{
					if (buffer[j] == 0x55) break;
					if (buffer[j] != 0xFF) TrackStats[density][buffer[j]]++;
				}
			}
			else
			{
				// Call to "burst_read_n" was 10x unsuccessful.
				printf("Too many errors.\n");
				fprintf(fplog, "Too many errors.\n");
				exit(2);
			}
		}
	}

	if (DBG_IHS)
	{
		for (density = 0; density <= 3; density++)
		{
			printf(":D%d-K%d: <", density,Killer[density]);
			for (j = 0; j <= 5; j++) {
				printf("%d", TrackStats[density][j]);
				if (j <5) printf(",");
			}
			printf(">\n");
		}
	}

	// Return BM_FF_TRACK if Killer track detected for all bitrates.
	if (Killer[0] == 1 || Killer[1] == 1 || Killer[2] == 1 || Killer[3] == 1)
		return(get_default_bitrate(track) | BM_FF_TRACK);

	// We may have found something, determine max density.
	MaxDenso = MaxNum = 0;
	for (density = 0; density <= 3; density++)
		for (j = 0; j <= 5; j++)
			if (TrackStats[density][j] >= MaxNum)
			{
				MaxDenso = j;
				MaxNum = TrackStats[density][j];
			}

	if (DBG_IHS)
		printf(">MaxDenso=%d, MaxNum=%d - ",MaxDenso,MaxNum);

	// Return BM_NO_SYNC if no Syncs detected for all bitrates.
	if (MaxNum == 0)
		return(get_default_bitrate(track) | BM_NO_SYNC);

	// Normalize MaxDenso
	if (MaxDenso == 5) MaxDenso = 4;
	if (MaxDenso > 0) MaxDenso -= 1;

	return((BYTE)MaxDenso);
}


// read drive memory location, returns 8bit value from 16bit address
static BYTE
read_mem(CBM_FILE fd, unsigned short addr)
{
        send_mnib_cmd(fd, FL_READ_MEM, NULL, 0);
        burst_write(fd, (BYTE)(addr & 0xff) );
        burst_write(fd, (BYTE)((addr >> 8) & 0xff) );
        return burst_read(fd);
}


// return default bitrate for track
static BYTE
get_default_bitrate(int track)
{
	return (speed_map[track/2]);
}
