/*
	drive.c - (C) Pete Rittwage, Markus Brenner, and friends.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#ifndef DJGPP
#include <opencbm.h>
#endif

#include "mnibarch.h"
#include "gcr.h"
#include "nibtools.h"

unsigned char *floppy_code = NULL;
unsigned int lpt[4];
int lpt_num;
extern int drivetype;
unsigned int floppybytes;
extern CBM_FILE fd;
extern int use_floppycode_srq;

#ifdef OPENCBM_42
int
cbm_parallel_burst_read_n(CBM_FILE f, unsigned char *Buffer, unsigned int Length)
{
	unsigned int count;

	for(count = 0; count < Length; count ++)
		Buffer[count] = cbm_parallel_burst_read(f);

	return 1;
}

int
cbm_parallel_burst_write_n(CBM_FILE f, unsigned char *Buffer, unsigned int Length)
{
	unsigned int count;

	for(count = 0; count < Length; count ++)
		cbm_parallel_burst_write(f, Buffer[count]);

	return 1;
}
#endif


/* Change the burst r/w routines to callbacks to avoid code mess */
unsigned char
burst_read(CBM_FILE f)
{
#if !defined (DJGPP) && !defined (OPENCBM_42)
	if(use_floppycode_srq)
		return cbm_srq_burst_read(f);
	else
#endif
		return cbm_parallel_burst_read(f);
}
void
burst_write(CBM_FILE f, unsigned char c)
{
#if !defined (DJGPP) && !defined (OPENCBM_42)
	if(use_floppycode_srq)
		cbm_srq_burst_write(f, c);
	else
#endif
		cbm_parallel_burst_write(f, c);
}
int
burst_read_n(CBM_FILE f, unsigned char *Buffer, unsigned int Length)
{
#if !defined (DJGPP) && !defined (OPENCBM_42)
	if(use_floppycode_srq)
		return cbm_srq_burst_read_n(f, Buffer, Length);
	else
#endif
		return cbm_parallel_burst_read_n(f, Buffer, Length);
}
int
burst_write_n(CBM_FILE f, unsigned char *Buffer, unsigned int Length)
{
#if !defined (DJGPP) && !defined (OPENCBM_42)
	if(use_floppycode_srq)
		return cbm_srq_burst_write_n(f, Buffer, Length);
	else
#endif
		return cbm_parallel_burst_write_n(f, Buffer, Length);
}
int
burst_read_track(CBM_FILE f, unsigned char *Buffer, unsigned int Length)
{
#if !defined (DJGPP) && !defined (OPENCBM_42)
	if(use_floppycode_srq)
		return cbm_srq_burst_read_track(f, Buffer, Length);
	else
#endif
		return cbm_parallel_burst_read_track(f, Buffer, Length);
}
int
burst_write_track(CBM_FILE f, unsigned char *Buffer, unsigned int Length)
{
#if !defined (DJGPP) && !defined (OPENCBM_42)
	if(use_floppycode_srq)
		return cbm_srq_burst_write_track(f, Buffer, Length);
	else
#endif
		return cbm_parallel_burst_write_track(f, Buffer, Length);
}

void ARCH_SIGNALDECL
handle_signals(int sig)
{
	/* Ignore multiple presses of ^C */
	//signal(SIGINT, SIG_IGN);
	//printf("\nExit requested by user. ");
	//exit(1);
}

void ARCH_SIGNALDECL
handle_exit(void)
{
	// Perform UI and wait a short while before the hard reset.
	send_mnib_cmd(fd, FL_RESET, NULL, 0);
	delay(50);
	printf("\nResetting drive...\n");
	cbm_reset(fd);
#ifndef DJGPP
	cbm_driver_close(fd);
#endif
	printf("Cleaning up...\n");
}

int
upload_code(CBM_FILE fd, BYTE drive)
{
	unsigned int databytes;
	int ret;

    static BYTE floppycode1541[] = {
		#include "nibtools_1541.inc"
		};
    static BYTE floppycode1571[] = {
		#include "nibtools_1571.inc"
		};
    static BYTE floppycode1541ihs[] = {
		#include "nibtools_1541_ihs.inc"
		};
    static BYTE floppycode1571ihs[] = {
		#include "nibtools_1571_ihs.inc"
		};
    static BYTE floppycode1571srq[] = {
		#include "nibtools_1571_srq.inc"
		};
    static BYTE floppycode1571srqtest[] = {
		#include "nibtools_1571_srq_test.inc"
		};

    switch (drivetype)
    {
    	case 1571:
			if(use_floppycode_srq==1)
    	    {
				// srq floppy code
				floppy_code = floppycode1571srq;
				databytes = sizeof(floppycode1571srq);
				printf("Sending 1571 SRQ support code...\n");
			}
			else if (use_floppycode_ihs)
			{
				// IHS floppy code
				floppy_code = floppycode1571ihs;
				databytes = sizeof(floppycode1571ihs);
				printf("Sending 1571 parallel/IHS support code...\n");
			}
			else if (use_floppycode_srq==2)
			{
				// SRQ test floppy code
				floppy_code = floppycode1571srqtest;
				databytes = sizeof(floppycode1571srqtest);
				printf("Sending 1571 SRQ test code...\n");
			}
			else
			{
				// non IHS floppy code
    	    	floppy_code = floppycode1571;
    	    	databytes = sizeof(floppycode1571);
    	    	printf("Sending 1571 parallel support code...\n");
    	    }
    	    break;

    	case 1541:
			if (!use_floppycode_ihs)
			{
				// non IHS floppy code
				floppy_code = floppycode1541;
				databytes = sizeof(floppycode1541);
				printf("Sending 1541 parallel support code...\n");
			}
			else
			{
				// IHS floppy code
				floppy_code = floppycode1541ihs;
				databytes = sizeof(floppycode1541ihs);
				printf("Sending 1541 parallel support code...\n");
			}
    	    break;

	default:
		printf("Unsupported drive type\n");
		return -1;
    }

	printf("Uploading floppy-side code ($%.4x bytes, $300-$%.3x)...", databytes, databytes+0x300);
	ret = cbm_upload(fd, drive, 0x300, floppy_code, databytes);
	if (ret < 0) return ret;
	floppybytes = databytes;
	printf("done.\n");

	return 0;
}

int
test_par_port(CBM_FILE fd)
{
	unsigned int i, rv;
	BYTE testBuf[0x100 + 1];

	rv = 1;

	printf("Testing communication...");
	send_mnib_cmd(fd, FL_TEST, NULL, 0);
	burst_read_n(fd, testBuf, sizeof(testBuf));

	// Check first 256 bytes for values 0 ... 255
	for (i = 0; i < sizeof(testBuf) - 1; i++) {
		if (testBuf[i] != i) {
			rv = 0;
			break;
		}
	}

	// Check last byte for 0
	if (testBuf[sizeof(testBuf) - 1] != 0)
		rv = 0;

	printf("done.\n");
	return (rv);
}

int
verify_floppy(CBM_FILE fd)
{
	unsigned int i, rv;
	BYTE testBuf[(0x800 - 0x300) + 1];

	printf("Testing code upload...");
	rv = 1;
	send_mnib_cmd(fd, FL_VERIFY_CODE, NULL, 0);
	burst_read_n(fd, testBuf, sizeof(testBuf));

	// Check for exact match with code we uploaded
	for (i = 0; i < floppybytes; i++)
	{
		if (testBuf[i] != floppy_code[i])
		{
			rv = 0;
			printf("diff: %d\n", i);
		}
	}

	// Check last byte for 0
	if (testBuf[sizeof(testBuf) - 1] != 0)
		rv = 0;

	printf("done.\n");
	return (rv);
}

#ifdef DJGPP
int
find_par_port(CBM_FILE fd)
{
	int i;
	for (i = 0; set_par_port(i); i++)
	{
		if (test_par_port(fd))
		{
			printf(" Found!\n");
			return (1);
		}
		printf(" no\n");
	}
	return (0);		/* no parallel port found */
}
#endif // DJGPP

int
reset_floppy(CBM_FILE fd, BYTE drive)
{
	char cmd[80];
	int ret;

	/* Turn on motor, go to track 18, send reset cmd to drive code */
	motor_on(fd);
	step_to_halftrack(fd, 18*2);
	send_mnib_cmd(fd, FL_RESET, NULL, 0);
	printf("drive reset...\n");
	delay(5000);
	cbm_listen(fd, drive, 15);

	/* Send Initialize command */
	ret = cbm_raw_write(fd, "I", 1);
	if (ret < 0) {
		printf("reset_floppy: error %d initializing\n", ret);
		return (ret);
	}
	cbm_unlisten(fd);
	delay(5000);

	/* Begin executing drive code at the start again */
	sprintf(cmd, "M-E%c%c", 0x00, 0x03);
	cbm_listen(fd, drive, 15);
	ret = cbm_raw_write(fd, cmd, 5);
	if (ret < 0) {
		printf("reset_floppy: error %d sending cmd\n", ret);
		return (ret);
	}
	cbm_unlisten(fd);
	//burst_read(fd);
	return (0);
}

int
init_floppy(CBM_FILE fd, BYTE drive, int bump)
{
	char cmd[80];
	char error[500];

	/* prepare error string $73: CBM DOS V2.6 1541 */
	sprintf(cmd, "M-W%c%c%c%c%c%c%c%c", 0, 3, 5, 0xa9, 0x73, 0x4c, 0xc1, 0xe6);
	cbm_exec_command(fd, drive, cmd, 11);
	sprintf(cmd, "M-E%c%c", 0x00, 0x03);
	cbm_exec_command(fd, drive, cmd, 5);
	cbm_device_status(fd, drive, error, sizeof(error));
	printf("Drive Version: %s\n", error);
	if(fplog) fprintf(fplog,"Drive Version: %s\n", error);

	/* OpenCBM returns 99, DRIVER ERROR on problems */
	if(error[0] == '9')
	{
		printf("Driver error.  Non-existent device or other problem...");
		return 0;
	}

	if (error[18] == '7')
	{
		drivetype = 1571;
		if(!override_srq)
			use_floppycode_srq = 1;
	}
	else
		drivetype = 1541;	/* if unknown drive, use 1541 code */

	printf("Drive type: %d\n", drivetype);

	if(fplog)
		fprintf(fplog,"Drive type: %d\n", drivetype);

	delay(1000);

	if (bump) perform_bump(fd,drive);

	/*
	 * Initialize media and switch drive to 1541 mode.
	 * We initialize first to do the head seek to read the BAM after the bump above.
	 * Changed to perform the 1541 mode select first, or else it breaks on a 1571 (PR)
	 */

	printf("Initializing\n");

	if(drivetype == 1571)
		cbm_exec_command(fd, drive, "U0>M0", 0);

	cbm_exec_command(fd, drive, "I0", 0);  /* test - this hangs on a completely non-CBM disk */

	if (upload_code(fd, drive) < 0)
	{
		printf("code upload failed, exiting\n");
		return 0;
	}

	/* Begin executing drive code at $300 */
	printf("Starting custom drive code...");
	sprintf(cmd, "M-E%c%c", 0x00, 0x03);
	cbm_exec_command(fd, drive, cmd, 5);
	burst_read(fd);
	printf("Started!\n");

#ifdef DJGPP
	if (!find_par_port(fd))
	{
		return 0;
	}
#endif

	if(!test_par_port(fd))
	{
		printf("Failed port transfer test. Check cabling.\n");
		return 0;
	}
	printf("Passed initial communication test.\n");

	if(!verify_floppy(fd))
	{
		printf("Failed code verification test. Check cabling.\n");
		return 0;
	}
	printf("Passed code verification test.\n");

	printf("Passed all basic port checks.\n");
	return 1;
}

void perform_bump(CBM_FILE fd, BYTE drive)
{
	char cmd[80];
	char byte;
	int rv, count;

	printf("Bumping...\n");

	/* Set job to run on track 1, sector 0 */
	sprintf(cmd, "M-W%c%c%c%c%c", 6, 0, 2, 1, 0);
	if (cbm_exec_command(fd, drive, cmd, 8) != 0) {
		printf("seek track 1 failed, exiting\n");
		exit(4);
	}

	/* Send bump command */
	byte = 0xc0;
	sprintf(cmd, "M-W%c%c%c%c", 0, 0, 1, byte);
	if (cbm_exec_command(fd, drive, cmd, 7) != 0) {
		printf("bump command failed, exiting\n");
		exit(4);
	}
	delay(2000);

	/* Wait until command has been completed (high bit=0) */
	count = 10;
	sprintf(cmd, "M-R%c%c", 0, 0);
	while ((byte & 0x80) != 0 && count-- != 0) {
		delay(500);
		if (cbm_exec_command(fd, drive, cmd, 5) != 0) {
			printf("bump m-r failed, exiting\n");
			exit(4);
		}
		rv = cbm_talk(fd, drive, 15);
		if (rv != 0) {
			printf("bump talk failed: %d\n", rv);
			exit(4);
		}
		rv = cbm_raw_read(fd, &byte, sizeof(byte));
		cbm_untalk(fd);
		if (rv != sizeof(byte)) {
			printf("bump raw read failed: %d\n", rv);
			exit(4);
		}
	}

	/* Check if status was 1 (OK) */
	if (byte != 1) {
		printf("bump status was error: %#x\n", byte);
		exit(4);
	}
}


int
set_density(CBM_FILE fd, BYTE density)
{
	BYTE cmdArgs[] = {
		density_branch[density],
		0x9f,
		bitrate_value[density],
	};

	if(use_floppycode_srq)
		cmdArgs[0] = density; // SRQ code doesn't use branching like original routines

	send_mnib_cmd(fd, FL_DENSITY, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);

	return (density);
}

/* $13d6 */
BYTE
set_bitrate(CBM_FILE fd, BYTE density)
{
	BYTE cmdArgs[] = {
		0x9f,			/* $1c00 CLEAR mask */
		bitrate_value[density],	/* $1c00 SET mask */
	};
	send_mnib_cmd(fd, FL_MOTOR, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);
	return (density);
}

/* $13bc */
BYTE
set_default_bitrate(CBM_FILE fd, int track)
{
	BYTE density;

	density = speed_map[track/2];
	return set_bitrate(fd, density);
}

void
send_mnib_cmd(CBM_FILE fd, BYTE cmd, BYTE *args, int num_args)
{
	BYTE cmdBuf[32] = { 0x00, 0x55, 0xaa, 0xff, cmd, };

	if (num_args > (int) sizeof(cmdBuf) - 5) {
		printf("send_mnib_cmd: too many args %d\n", num_args);
		return;
	}

	if (num_args != 0)
		memcpy(&cmdBuf[5], args, num_args);
	burst_write_n(fd, cmdBuf, 5 + num_args);
}

void
set_full_track(CBM_FILE fd)
{
	BYTE cmdArgs[] = {
		0xfc,	/* $1c00 CLEAR mask (clear stepper bits) */
		0x02,	/* $1c00 SET mask (stepper bits = %10) */
	};
	send_mnib_cmd(fd, FL_MOTOR, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);
	delay(500);			/* wait for motor to step */
}

void
motor_on(CBM_FILE fd)
{
	BYTE cmdArgs[] = {
		0xf3,	/* $1c00 CLEAR mask */
		0x0c,	/* $1c00 SET mask (LED + motor ON) */
	};
	send_mnib_cmd(fd, FL_MOTOR, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);
	delay(500);			/* wait for motor to turn on */
}

void
motor_off(CBM_FILE fd)
{
	BYTE cmdArgs[] = {
		0xf3,	/* $1c00 CLEAR mask */
		0x00,	/* $1c00 SET mask (LED + motor OFF) */
	};
	send_mnib_cmd(fd, FL_MOTOR, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);
	delay(500);			/* wait for motor to turn off */
}

void
step_to_halftrack(CBM_FILE fd, int halftrack)
{
	BYTE cmdArgs[] = {
		(BYTE) (halftrack != 0 ? halftrack : 1),
	};
	send_mnib_cmd(fd, FL_STEPTO, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);
}

unsigned int
track_capacity(CBM_FILE fd)
{
	unsigned int capacity;
	BYTE capacity_data[2];

	send_mnib_cmd(fd, FL_CAPACITY, NULL, 0);
	burst_read_n(fd, capacity_data, sizeof(capacity_data));

	capacity = (capacity_data[1] << 8) | capacity_data[0];
	return (capacity);
}


