/*
    NIBWRITE - part of the NIBTOOLS package for 1541/1571 disk image nibbling
	by Peter Rittwage <peter(at)rittwage(dot)com>
    based on code from MNIB, by Dr. Markus Brenner
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#ifndef DJGPP
#include <opencbm.h>
#endif

#include "mnibarch.h"
#include "gcr.h"
#include "nibtools.h"
#include "lz.h"

int _dowildcard = 1;

char bitrate_range[4] = { 43 * 2, 31 * 2, 25 * 2, 18 * 2 };
char bitrate_value[4] = { 0x00, 0x20, 0x40, 0x60 };
char density_branch[4] = { 0xb1, 0xb5, 0xb7, 0xb9 };

BYTE compressed_buffer[(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH];
BYTE file_buffer[(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH];
BYTE track_buffer[(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH];
BYTE track_density[MAX_HALFTRACKS_1541 + 2];
BYTE track_alignment[MAX_HALFTRACKS_1541 + 2];
size_t track_length[MAX_HALFTRACKS_1541 + 2];

int file_buffer_size;
int start_track, end_track, track_inc;
int reduce_sync;
int fix_gcr, aggressive_gcr;
int align;
extern unsigned int lpt[4];
extern int lpt_num;
int drivetype;
extern unsigned int floppybytes;
int imagetype;
int mode;
int verify;
int auto_capacity_adjust;
int align_disk;
int gap_match_length;
int cap_min_ignore;
int verbose = 0;
float motor_speed;
int skew = 0;
int ihs = 0;
int rpm_real;
int unformat_passes;
int align_delay;
int increase_sync = 0;
int presync = 0;
BYTE fillbyte = 0x55;
BYTE drive = 8;
char * cbm_adapter = "";
int use_floppycode_srq = 0;
int override_srq = 0;
int extra_capacity_margin=5;
int sync_align_buffer=0;
int fattrack=0;
int track_match=0;
int old_g64=0;

CBM_FILE fd;
FILE *fplog;

int ARCH_MAINDECL
main(int argc, char *argv[])
{
	int bump, reset, i;
	char filename[256];
	char argcache[256];

	fprintf(stdout,
		"\nnibwrite - Commodore 1541/1571 disk image 'remastering' tool\n"
		AUTHOR "Revision %d - " VERSION "\n\n", SVN);

	/* we can do nothing with no switches */
	if (argc < 2)
		usage();

#ifdef DJGPP
	fd = 1;
#endif

	bump = 1;  /* failing to bump sometimes give us wrong tracks on heavily protected disks */
	reset = 1;

	start_track =  2;
	end_track = 82;
	track_inc = 2;

	reduce_sync = 4;
	fix_gcr = 1;
	align_disk = 0;
	auto_capacity_adjust = 1;
	verbose = 1;
	gap_match_length = 7;
	cap_min_ignore = 0;
	motor_speed = 300;
	rpm_real = 296;
	unformat_passes = 1;

	mode = MODE_WRITE_DISK;
	align = ALIGN_NONE;

	/* clear heap buffers */
	memset(compressed_buffer, 0x00, sizeof(compressed_buffer));
	memset(file_buffer, 0x00, sizeof(file_buffer));
	memset(track_buffer, 0x00, sizeof(track_buffer));

	/* default is to reduce sync */
	memset(reduce_map, REDUCE_SYNC, MAX_TRACKS_1541+2);

	/* cache our arguments for logfile generation */
	strcpy(argcache, "");
	for (i = 0; i < argc; i++)
	{
		strcat(argcache, argv[i]);
		strcat(argcache," ");
	}

	while (--argc && (*(++argv)[0] == '-'))
		parseargs(argv);

	printf("\n");
	if (argc > 0)	strcpy(filename, argv[0]);

	if(mode == MODE_WRITE_DISK)
	{
		if(!(loadimage(filename)))
		{
			printf("\nImage loading failed\n");
			exit(0);
		}
	}

#ifdef DJGPP
	calibrate();
	if (!detect_ports(reset))
		return 0;
#elif defined(OPENCBM_42)
	/* remain compatible with OpenCBM < 0.4.99 */
	if (cbm_driver_open(&fd, 0) != 0)
	{
		printf("Is your X-cable properly configured?\n");
		exit(0);
	}
#else /* assume > 0.4.99 */
	if (cbm_driver_open_ex(&fd, cbm_adapter) != 0)
	{
		printf("Is your X-cable properly configured?\n");
		exit(0);
	}
#endif

	/* Once the drive is accessed, we need to close out state when exiting */
	atexit(handle_exit);
	signal(SIGINT, handle_signals);

	printf("Using device #%d\n",drive);
	if(!(init_floppy(fd, drive, bump)))
	{
		printf("\nFloppy drive initialization failed\n");
		exit(0);
	}

	switch (mode)
	{
		case MODE_WRITE_DISK:
		case MODE_WRITE_RAW:
			/*printf("Current disk WILL be OVERWRITTEN!\n"
			   "Press ENTER to continue or CTRL-C to quit.\n");
			   getchar();
			*/
			writeimage(fd);
			break;

		case MODE_UNFORMAT_DISK:
			/*printf("Ready to unformat disk.\n");
			   printf("Current disk WILL be DESTROYED!\n"
			  "Press ENTER to continue or CTRL-C to quit.\n");
			getchar();
			*/
			unformat_disk(fd);
			break;

		case MODE_SPEED_ADJUST:
			speed_adjust(fd);
			break;
	}

	motor_on(fd);
	step_to_halftrack(fd, 18 * 2);

	exit(0);
}

int loadimage(char *filename)
{
	/* read and remaster disk */
	if (compare_extension(filename, "D64"))
	{
		if(!(read_d64(filename, track_buffer, track_density, track_length))) return 0;
	}
	else if (compare_extension(filename, "G64"))
	{
		if(!(read_g64(filename, track_buffer, track_density, track_length))) return 0;
		if(sync_align_buffer)	sync_tracks(track_buffer, track_density, track_length, track_alignment);
	}
	else if (compare_extension(filename, "NBZ"))
	{
		printf("Uncompressing NBZ...\n");
		if(!(file_buffer_size = load_file(filename, compressed_buffer))) return 0;
		if(!(file_buffer_size = LZ_Uncompress(compressed_buffer, file_buffer, file_buffer_size))) return 0;
		if(!(read_nib(file_buffer, file_buffer_size, track_buffer, track_density, track_length))) return 0;
		align_tracks(track_buffer, track_density, track_length, track_alignment);
	}
	else if (compare_extension(filename, "NIB"))
	{
		if(!(file_buffer_size = load_file(filename, file_buffer))) return 0;
		if(!(read_nib(file_buffer, file_buffer_size, track_buffer, track_density, track_length))) return 0;
		align_tracks(track_buffer, track_density, track_length, track_alignment);
	}
	else if (compare_extension(filename, "NB2"))
	{
		if(!(read_nb2(filename, track_buffer, track_density, track_length))) return 0;
		align_tracks(track_buffer, track_density, track_length, track_alignment);
	}
	else
	{
		printf("\nUnknown image type");
		return 0;
	}

	return 1;
}

int writeimage(CBM_FILE fd)
{
	/* turn on motor and measure speed */
	motor_on(fd);

	if(auto_capacity_adjust)
		adjust_target(fd);

	if(align_disk)
		init_aligned_disk(fd);

	if(mode == MODE_WRITE_RAW)
		master_disk_raw(fd, track_buffer, track_density, track_length);
	else
		master_disk(fd, track_buffer, track_density, track_length);

	step_to_halftrack(fd, 18 * 2);
	printf("\n");

	return 1;
}

void
usage(void)
{
	fprintf(stderr, "usage: nibwrite [options] <filename>\n\n"
		 " -@x: Use OpenCBM device 'x' (xa1541, xum1541:0, xum1541:1, etc.)\n"
	     " -D[n]: Use drive #[n]\n"
	     " -S[n]: Override starting track\n"
	     " -E[n]: Override ending track\n"
//	     " -s[n]: Manual track skew (in ms)\n"
		 " -m[n]: Change extra capacity margin to [n] (default: 0)\n"
		 " -s: Use SRQ transfer code instead of parallel (1571 only)\n"
	     " -t: Enable timer-based track alignment\n"
	     " -c: Disable automatic capacity adjustment\n"
	     " -u: Unformat disk. (writes all 0 bits to surface)\n"
	     );

	switchusage();
	exit(1);
}


