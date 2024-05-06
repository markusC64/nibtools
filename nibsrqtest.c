/*
    NIBSRQTEST
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

char bitrate_value[4] = { 0x00, 0x20, 0x40, 0x60 };
char density_branch[4] = { 0xb1, 0xb5, 0xb7, 0xb9 };

BYTE speed_map[42 + 1] = {
	0,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/*  1 - 10 */
	3, 3, 3, 3, 3, 3, 3, 2, 2, 2,	/* 11 - 20 */
	2, 2, 2, 2, 1, 1, 1, 1, 1, 1,	/* 21 - 30 */
	0, 0, 0, 0, 0,					/* 31 - 35 */
	2, 2, 2, 2, 2, 2, 2				/* 36 - 42 (non-standard) */
};

BYTE drive = 8;
char * cbm_adapter = "";
int use_floppycode_srq=2;
int use_floppycode_ihs=0;
int override_srq=0;
int drivetype=1571;
CBM_FILE fd;

FILE *fplog;

int ARCH_MAINDECL
main(int argc, char *argv[])
{
	BYTE buffer1[0xff+1];
	BYTE buffer2[0xff+1];
	char cmd[80];
	size_t l,m;

	fprintf(stdout,"\nnibsrqtest - tests SRQ communication code (with 1571 drive)\n"
	AUTHOR "Revision %d - " VERSION "\n\n", SVN);

	if(cbm_driver_open_ex(&fd, cbm_adapter) != 0)
	{
		printf("Is your cable properly configured?\n");
		exit(0);
	}

	printf("Initializing\n");

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

	/* Once the drive is accessed, we need to close out state when exiting */
	atexit(handle_exit);
	signal(SIGINT, handle_signals);

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

	// set dens to 3
	BYTE cmdArgs[] = {
		0x9f,
		bitrate_value[3],
	};
	send_mnib_cmd(fd, FL_MOTOR, cmdArgs, sizeof(cmdArgs));
	burst_read(fd);

	for(l=1;l<=100;l++)
	{
		printf("\nSending data interation #%d\n",l);

		//memset(buffer1, rand() % 256, sizeof(buffer1));
		for (m = 0; m < sizeof(buffer1); m++)
			buffer1[m] = rand() % 256;

		send_mnib_cmd(fd, FL_WRITE, NULL, 0);
		if(!burst_write_track(fd, buffer1, sizeof(buffer1)))
		{
			printf("timeout error writing");
			exit(0);
		}

		printf("\nReading data interation #%d\n",l);
		send_mnib_cmd(fd, FL_READNORMAL, NULL, 0);
		burst_read(fd);
		if(!burst_read_track(fd, buffer2, sizeof(buffer2)))
		{
			printf("timeout error reading");
			exit(0);
		}

		printf("\nComparing data interation #%d\n",l);
		for(m=0;m<=0xff;m++)
		{
			if(buffer1[m]!=buffer2[m])
			{
				printf("POS %d: error %x!=%x\n",m,buffer1[m],buffer2[m]);
				exit(0);
			}
			else
				printf("%x=%x ",buffer1[m], buffer2[m]);
		}
		printf("\nFinished Iteration #%d\n",l);
	}

	printf("\nPassed comm tests");
	exit(1);

}



