
#ifndef NIBTOOLS_DEFS_H
#define NIBTOOLS_DEFS_H

#define FL_STEPTO       0x00
#define FL_MOTOR        0x01
#define FL_RESET        0x02
#define FL_READWOSYNC   0x03
#define FL_READNORMAL   0x04
#define FL_READIHS      0x05
#define FL_DENSITY      0x06
#define FL_SCANKILLER  	0x07
#define FL_SCANDENSITY 	0x08
#define FL_READMOTOR   	0x09
#define FL_TEST        	0x0a
#define FL_WRITE        0x0b
#define FL_CAPACITY    	0x0c
#define FL_ALIGNDISK    0x0d
#define FL_VERIFY_CODE  0x0e
#define FL_FILLTRACK    0x0f
#define FL_READMARKER   0x10

#define DISK_NORMAL     0

#define IMAGE_NIB      	0	/* destination image format */
#define IMAGE_D64      	1
#define IMAGE_G64      	2
#define IMAGE_NB2       3

#define BM_MATCH       	0x10 /* not used but exists in very old images */
#define BM_NO_CYCLE     0x20
#define BM_NO_SYNC      0x40
#define BM_FF_TRACK     0x80

#define DENSITY_SAMPLES 2

/* custom density maps for reading */
#define DENSITY_STANDARD 0
#define DENSITY_RAPIDLOK 1

#define MODE_READ_DISK      0
#define MODE_WRITE_DISK     1
#define MODE_UNFORMAT_DISK  2
#define MODE_WRITE_RAW	    3
#define MODE_TEST_ALIGNMENT 4
#define MODE_SPEED_ADJUST   5

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0)

#endif
