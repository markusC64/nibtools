
#ifndef NIBTOOLS_GLOBAL_H
#define NIBTOOLS_GLOBAL_H

extern char bitrate_range[4];
extern char bitrate_value[4];
extern char density_branch[4];
extern char *cbm_adapter;
extern BYTE density_map;
extern BYTE skew_map[];
extern BYTE fillbyte;
extern BYTE copyprot;
extern BYTE drive;
extern FILE * fplog;
extern float motor_speed;
extern size_t error_retries;
extern int skew;
extern int mode;
extern int read_killer;
extern int align_disk;
extern int force_density;
extern int track_match;
extern int interactive_mode;
extern int gap_match_length;
extern int cap_min_ignore;
extern int verbose;
extern int ihs;
extern int start_track, end_track, track_inc;
extern int fix_gcr, reduce_sync, increase_sync;
extern int imagetype, auto_capacity_adjust;
extern int extended_parallel_test;
extern int force_nosync;
extern int rawmode;
extern int rpm_real;
extern int unformat_passes;
extern int align_delay;
extern int presync;
extern int use_floppycode_srq;
extern int override_srq;
extern int extra_capacity_margin;
extern int sync_align_buffer;
extern int fattrack;
extern int old_g64;

#endif
