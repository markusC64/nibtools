/* NIBTOOLS IHS routines */

// IHS drive code ==========
#define FL_IHS_ON       0x10
#define FL_IHS_OFF      0x11
#define FL_IHS_PRESENT2 0x12 // version with long timeouts
#define FL_IHS_PRESENT  0x13 // version with cbm_parallel_burst_read_track
#define FL_DBR_ANALYSIS 0x14
#define FL_READ_MEM     0x15
#define FL_IHS_READ_SCP 0x16
// =========================

extern int Use_SCPlus_IHS;          // "-j"
extern int track_align_report;      // "-x"
extern int Deep_Bitrate_SCPlus_IHS; // "-y"
extern int Test_SCPlus_IHS;         // "-z"
extern BYTE IHSresult;
extern int use_floppycode_ihs;
extern char *logline;

BYTE Check_SCPlus_IHS(CBM_FILE fd, BYTE KeepOn);
BYTE Check_SCPlus_IHS_2(CBM_FILE fd, BYTE KeepOn);
void OutputIHSResult(int ToConsole, int ToLogfile, BYTE IHSresult, FILE *fplog);
int TrackAlignmentReport2(CBM_FILE fd, BYTE *buffer);
int DeepBitrateAnalysis(CBM_FILE fd, char *filename, BYTE *track_buffer, char *logline);
int BitrateStats(BYTE *buffer, char *logline, unsigned short NumSync);
BYTE Scan_Track_SCPlus_IHS(CBM_FILE fd, int track, BYTE *buffer);
