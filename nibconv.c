/*
    NIBCONV - part of the NIBTOOLS package for 1541/1571 disk image nibbling
	by Peter Rittwage <peter(at)rittwage(dot)com>
    based on code from MNIB, by Dr. Markus Brenner
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include "mnibarch.h"
#include "gcr.h"
#include "drive.h"
#include "fileio.h"
#include "lz.h"
#include "nibtools_version.h"
#include "prot.h"

void usage(void);

int _dowildcard = 1;

BYTE compressed_buffer[(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH];
BYTE file_buffer[(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH];
BYTE track_buffer[(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH];
BYTE track_density[MAX_HALFTRACKS_1541 + 2];
BYTE track_alignment[MAX_HALFTRACKS_1541 + 2];
size_t track_length[MAX_HALFTRACKS_1541 + 2];
int file_buffer_size;
int start_track, end_track, track_inc;
int reduce_sync, reduce_badgcr, reduce_gap;
int fix_gcr, align, force_align;
int gap_match_length;
int cap_min_ignore;
int skip_halftracks;
int verbose;
int rpm_real;
int auto_capacity_adjust;
int skew;
int align_disk;
int ihs;
int mode;
int unformat_passes;
int capacity_margin;
int align_delay;
int increase_sync = 0;
int presync = 0;
BYTE fillbyte = 0x55;
BYTE drive = 8;
char * cbm_adapter = "";
int use_floppycode_srq = 0;
int extra_capacity_margin=5;
int sync_align_buffer=0;
int fattrack=0;
int track_match=0;
int old_g64=0;

int ARCH_MAINDECL
main(int argc, char **argv)
{
	char inname[256], outname[256];
	char *dotpos;
	FILE *fp;
	int t;

	start_track = 1 * 2;
	end_track = 42 * 2;
	track_inc = 1;
	fix_gcr = 1;
	reduce_sync = 4;
	skip_halftracks = 0;
	align = ALIGN_NONE;
	force_align = ALIGN_NONE;
	gap_match_length = 7;
	cap_min_ignore = 0;
	verbose = 1;
	rpm_real = 296;

	/* default is to reduce sync */
	memset(reduce_map, REDUCE_SYNC, MAX_TRACKS_1541 + 2);
	//memset(track_length, 0, MAX_TRACKS_1541 + 2);
	for(t=0; t<MAX_TRACKS_1541 + 2; t++)
		track_length[t] = NIB_TRACK_LENGTH; // I do not recall why this was done, but left at MAX

	fprintf(stdout,
		"\nnibconv - converts a CBM disk image from one format to another.\n"
		AUTHOR "Revision %d - " VERSION "\n\n", SVN);

	/* clear heap buffers */
	memset(compressed_buffer, 0x00, sizeof(compressed_buffer));
	memset(file_buffer, 0x00, sizeof(file_buffer));
	memset(track_buffer, 0x00, sizeof(track_buffer));

	while (--argc && (*(++argv)[0] == '-'))
		parseargs(argv);

	if(argc < 1)	usage();

	strcpy(inname, argv[0]);

	if(argc < 2)
	{
		strcpy(outname, inname);
		dotpos = strrchr(outname, '.');
		if (dotpos != NULL) *dotpos = '\0';

		 if(compare_extension(inname, "G64"))
			strcat(outname, ".d64");
		else
			strcat(outname, ".g64");
	}
	else
		strcpy(outname, argv[1]);

	printf("Converting %s -> %s\n\n",inname, outname);

	if( (fp=fopen(outname,"r")) )
	{
		fclose(fp);
		printf("File exists - Overwrite? (y/N)");
		if(getchar() != 'y') exit(0);
	}

	/* convert */
	if (compare_extension(inname, "D64"))
	{
		if(!(read_d64(inname, track_buffer, track_density, track_length))) exit(0);
		skip_halftracks=1;
	}
	else if (compare_extension(inname, "G64"))
	{
		if(!(read_g64(inname, track_buffer, track_density, track_length))) exit(0);
		if(sync_align_buffer)	sync_tracks(track_buffer, track_density, track_length, track_alignment);
	}
	else if (compare_extension(inname, "NBZ"))
	{
		printf("Uncompressing NBZ...\n");
		if(!(file_buffer_size = load_file(inname, compressed_buffer))) exit(0);
		if(!(file_buffer_size = LZ_Uncompress(compressed_buffer, file_buffer, file_buffer_size))) exit(0);
		if(!(read_nib(file_buffer, file_buffer_size, track_buffer, track_density, track_length))) exit(0);
		if( (compare_extension(outname, "G64")) || (compare_extension(outname, "D64")) )
			align_tracks(track_buffer, track_density, track_length, track_alignment);
		search_fat_tracks(track_buffer, track_density, track_length);
	}
	else if (compare_extension(inname, "NIB"))
	{
		if(!(file_buffer_size = load_file(inname, file_buffer))) exit(0);
		if(!(read_nib(file_buffer, file_buffer_size, track_buffer, track_density, track_length))) exit(0);
		if( (compare_extension(outname, "G64")) || (compare_extension(outname, "D64")) )
			align_tracks(track_buffer, track_density, track_length, track_alignment);
		search_fat_tracks(track_buffer, track_density, track_length);
	}
	else if (compare_extension(inname, "NB2"))
	{
		if(!(read_nb2(inname, track_buffer, track_density, track_length))) exit(0);
		if( (compare_extension(outname, "G64")) || (compare_extension(outname, "D64")) )
			align_tracks(track_buffer, track_density, track_length, track_alignment);
		search_fat_tracks(track_buffer, track_density, track_length);
	}
	else
	{
		printf("Unknown input file type\n");
		exit(0);
	}

	if (compare_extension(outname, "D64"))
	{
		if(!(write_d64(outname, track_buffer, track_density, track_length))) exit(0);
		printf("\nWARNING!\nConverting to D64 is a lossy conversion.\n");
		printf("All individual sector header and gap information is lost.\n");
		printf("It is suggested you use the G64 format for most disks.\n");
	}
	else if (compare_extension(outname, "G64"))
	{
		if(skip_halftracks) track_inc = 2;
		if(!(write_g64(outname, track_buffer, track_density, track_length))) exit(0);

		if (compare_extension(inname, "D64"))
		{
			printf("\nWARNING!\nConverting from D64/G64 to G64 is not normally useful.\n");
			printf("No individual sector header or gap information is stored in a D64 image,\n");
			printf("so it has to be recontructed to make this conversion.  If the program you are\n");
			printf("trying to use needs this information (such as for protection),\nit may still fail.\n");
		}
	}
	else if (compare_extension(outname, "NBZ"))
	{
		/*
		if( (compare_extension(inname, "D64")) ||
			(compare_extension(inname, "G64")) ||
			(compare_extension(inname, "NBZ")))
		{
			printf("Output format makes no sense from this input file.\n");
			exit(0);
		}
		*/
		if(skip_halftracks) track_inc = 2;
		if(!(file_buffer_size = write_nib(file_buffer, track_buffer, track_density, track_length))) exit(0);
		if(!(file_buffer_size = LZ_CompressFast(file_buffer, compressed_buffer, file_buffer_size))) exit(0);
		if(!(save_file(outname, compressed_buffer, file_buffer_size))) exit(0);
	}
	else if (compare_extension(outname, "NIB"))
	{
		/*
		if( (compare_extension(inname, "D64")) ||
			(compare_extension(inname, "G64")))
		{
			printf("Output format makes no sense from this input file.\n");
			exit(0);
		}
		*/
		if(skip_halftracks) track_inc = 2;
		if(!(file_buffer_size = write_nib(file_buffer, track_buffer, track_density, track_length))) exit(0);
		if(!(save_file(outname, file_buffer, file_buffer_size))) exit(0);
	}
	else if (compare_extension(outname, "NB2"))
	{
		printf("Output to NB2 format makes no sense from this input file.\n");
		exit(0);
	}
	else
	{
		printf("Unknown output file type\n");
		exit(0);
	}

	return 0;
}

void
usage(void)
{
	fprintf(stderr,
	"usage: nibconv [options] <infile>.ext1 <outfile>.ext2\n"
	"\nsupported file extensions for ext1:\n"
	"NIB, NB2, D64, G64\n"
	"\nsupported file extensions for ext2:\n"
	"D64, G64\n"
	"\noptions:\n");

	switchusage();
	exit(1);
}
