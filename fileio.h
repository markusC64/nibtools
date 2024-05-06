
#ifndef FILEIO_H
#define FILEIO_H

void parseargs(char *argv[]);
void switchusage(void);
int load_file(char *filename, BYTE *file_buffer);
int save_file(char *filename, BYTE *file_buffer, int length);
int read_nib(BYTE *file_buffer, int file_buffer_size, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int read_nb2(char *filename, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int read_g64(char *filename, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int read_d64(char *filename, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int write_nib(BYTE*file_buffer, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int write_g64(char *filename, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
int write_d64(char *filename, BYTE *track_buffer, BYTE *track_density, size_t *track_length);
size_t compress_halftrack(int halftrack, BYTE *track_buffer, BYTE track_density, size_t track_length);
int align_tracks(BYTE *track_buffer, BYTE *track_density, size_t *track_length, BYTE *track_alignment);
int sync_tracks(BYTE *track_buffer, BYTE *track_density, size_t *track_length, BYTE *track_alignment);
int write_dword(FILE * fd, DWORD * buf, int num);
unsigned int crc_dir_track(BYTE *track_buffer, size_t *track_length);
unsigned int crc_all_tracks(BYTE *track_buffer, size_t *track_length);
unsigned int md5_dir_track(BYTE *track_buffer, size_t *track_length, unsigned char *result);
unsigned int md5_all_tracks(BYTE *track_buffer, size_t *track_length, unsigned char *result);

#endif
