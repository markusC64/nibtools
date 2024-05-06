/*
 * NIBTOOLS
 * Copyright (c) Pete Rittwage <peter(at)rittwage(dot)com>
 * based on MNIB by Markus Brenner <markus(at)brenner(dot)de>
 */


#include "nibtools_version.h"
#include "nibtools_defs.h"

/* global variables */
#include "nibtools_global.h"


#include "ihs.h"

/* common */
void usage(void);

/* nibread.c */
int disk2file(CBM_FILE fd, char * filename);
void parallel_test(int interations);

/* nibwrite.c */
int loadimage(char * filename);
int writeimage(CBM_FILE fd);

/* fileio.c */
#include "fileio.h"

/* read.c */
BYTE read_halftrack(CBM_FILE fd, int halftrack, BYTE * buffer);
BYTE paranoia_read_halftrack(CBM_FILE fd, int halftrack, BYTE * buffer);
int read_floppy(CBM_FILE fd, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int write_nb2(CBM_FILE fd, char * filename);
void get_disk_id(CBM_FILE fd);
BYTE scan_density(CBM_FILE fd);
int TrackAlignmentReport(CBM_FILE fd);

/* write.c */
void master_disk(CBM_FILE fd, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
void master_disk_raw(CBM_FILE fd, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
void prep_track(CBM_FILE fd, BYTE *track_buffer, BYTE *track_density, int track, size_t tracklen);
void write_raw(CBM_FILE fd, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
void unformat_disk(CBM_FILE fd);
void zero_track(CBM_FILE fd, int track);
unsigned int track_capacity(CBM_FILE fd);
void init_aligned_disk(CBM_FILE fd);
void adjust_target(CBM_FILE fd);
void speed_adjust(CBM_FILE fd);
void kill_track(CBM_FILE fd, int track);

/* drive.c  */
#include "drive.h"
