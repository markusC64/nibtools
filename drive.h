
#ifndef DRIVE_H
#define DRIVE_H

int compare_extension(unsigned char * filename, unsigned char * extension);
unsigned char  burst_read(CBM_FILE f);
void burst_write(CBM_FILE f, unsigned char c);
int burst_read_n(CBM_FILE f, unsigned char *Buffer, unsigned int Length);
int burst_write_n(CBM_FILE f, unsigned char *Buffer, unsigned int Length);
int burst_read_track(CBM_FILE f, unsigned char *Buffer, unsigned int Length);
int burst_write_track(CBM_FILE f, unsigned char *Buffer, unsigned int Length);
void ARCH_SIGNALDECL handle_signals(int sig);
void ARCH_SIGNALDECL handle_exit(void);
int upload_code(CBM_FILE fd, BYTE drive);
int reset_floppy(CBM_FILE fd, BYTE drive);
int init_floppy(CBM_FILE fd, BYTE drive, int bump);
int set_density(CBM_FILE fd, BYTE density);
BYTE set_bitrate(CBM_FILE fd, BYTE density);
BYTE set_default_bitrate(CBM_FILE fd, int track);
BYTE scan_track(CBM_FILE fd, int track);
void perform_bump(CBM_FILE fd, BYTE drive);
int test_par_port(CBM_FILE fd);
void send_mnib_cmd(CBM_FILE fd, BYTE cmd, BYTE *args, int num_args);
void set_full_track(CBM_FILE fd);
void motor_on(CBM_FILE fd);
void motor_off(CBM_FILE fd);
void step_to_halftrack(CBM_FILE fd, int halftrack);
int verify_floppy(CBM_FILE fd);
#ifdef DJGPP
#include <unistd.h>
int find_par_port(CBM_FILE fd);
#endif

#endif
