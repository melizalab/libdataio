/*
** pcmio.h
**
** header file for libdataio, a library for reading certain types of data
** such as sampled sound/electrical data, labels (named events occuring at
** specific times), or spike times of events for neuronal action potential
** firing.   This library is primarily used by my programs aplot and by the
** a-ware set of programs for scientific data collection and analysis.
**
** Copyright (C) by Amish S. Dave 2003
**
** This code is distributed under the GNU GENERAL PUBLIC LICENSE (GPL)
** Version 2 (June 1991). See the "COPYING" file distributed with this software
** for more info.
*/

#ifndef PCMIOHEADER
#define PCMIOHEADER

extern int pcm_errno;

#ifdef linux
#ifdef MADV_SEQUENTIAL
#undef MADV_SEQUENTIAL
#endif
#define MADV_SEQUENTIAL
#define madvise(addr,len,flags)
#endif

#define PCMIOMMAP 1
#define PCMIOMALLOC 2
#define PCMIOINCENTRY 3
#define PCMIODECENTRY 4
#define PCMIOSETTIME 5
#define PCMIOSETSR 6
#define PCMIOGETSIZE 7				/* Get the size in samples - shortcut to using pcm_stat() - arg = int* */
#define PCMIOGETSR 8				/* Get the samplerate in Hz - shortcut to using pcm_stat() - arg = int* */
#define PCMIOGETENTRY 9				/* Get the entry - shortcut to using pcm_stat() - arg = int* */
#define PCMIOGETTIME 10				/* Get the timestamp - shortcut to using pcm_stat() - arg = (long *) */
#define PCMIOGETCAPS 11				/* Get the capabilities of this file format... Includes MULTENTRY */
#define PCMIOSETTIMEFRACTION 12
#define PCMIOGETTIMEFRACTION 13		/* Get the timestamp's microseconds fraction, arg = (long *) */
#define PCMIOGETNENTRIES 14			/* Get the number of entries in this file */

#define PCMIOCAP_MULTENTRY 1		/* This file format can hold >1 entry... */
#define PCMIOCAP_SAMPRATE 2			/* This file format stores samplerate info... */

typedef struct pcmfilestruct
{
	char *name;
	int flags;
	int entry;
	int fd;
	void *addr;
	int len;
	short *bufptr;
	int buflen;
	int memalloctype;
	int (*open)();
	void (*close)();
	int (*read)();
	int (*write)();
	int (*seek)();
	int (*ctl)();
	int (*stat)();
	char *tempnam;
#ifdef USING_ESPS
	void *header;				/* For ESPS */
	FILE *espsfp;				/* For ESPS */
#endif
	int samplerate;
	int timestamp;
	long microtimestamp;
	int nentries;
	void *p2file;				/* For PCMSEQ2 */
	int pcmseq3_entrysize;		/* For PCMSEQ2 */
	char pcmseq3_key[29];		/* For PCMSEQ2 */
	long pcmseq3_cursamp;		/* For PCMSEQ2 */
	long pcmseq3_poscache;		/* For PCMSEQ2 */
	int entrystarted;			/* For PCMSEQ2 */
	FILE *outfp;
	int wav_nbytes_addr1;
	int wav_nbytes_addr2;
} PCMFILE;

struct pcmstat
{
	int	entry;
	int	nsamples;
	int	samplerate;
	int timestamp;
	long microtimestamp;
	int capabilities;
	int nentries;
};

PCMFILE *pcm_open(char *, char *);
void pcm_close(PCMFILE *);
int pcm_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcm_seek(PCMFILE *fp, int entry);
int pcm_ctl(PCMFILE *fp, int request, void *arg);
int pcm_stat(PCMFILE *fp, struct pcmstat *buf);
int pcm_write(PCMFILE *fp, short *buf, int nsamples);

int pcmraw_recognizer(PCMFILE *);
int pcmraw_open(PCMFILE *);
void pcmraw_close(PCMFILE *fp);
int pcmraw_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcmraw_seek(PCMFILE *fp, int entry);
int pcmraw_ctl(PCMFILE *fp, int request, void *arg);
int pcmraw_stat(PCMFILE *fp, struct pcmstat *buf);
int pcmraw_write(PCMFILE *fp, short *buf, int nsamples);

int pcmfix_recognizer(PCMFILE *);
int pcmfix_open(PCMFILE *);
void pcmfix_close(PCMFILE *fp);
int pcmfix_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcmfix_seek(PCMFILE *fp, int entry);
int pcmfix_ctl(PCMFILE *fp, int request, void *arg);
int pcmfix_stat(PCMFILE *fp, struct pcmstat *buf);
int pcmfix_write(PCMFILE *fp, short *buf, int nsamples);

int pcmwav_recognizer(PCMFILE *);
int pcmwav_open(PCMFILE *);
void pcmwav_close(PCMFILE *fp);
int pcmwav_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcmwav_seek(PCMFILE *fp, int entry);
int pcmwav_ctl(PCMFILE *fp, int request, void *arg);
int pcmwav_stat(PCMFILE *fp, struct pcmstat *buf);
int pcmwav_write(PCMFILE *fp, short *buf, int nsamples);

int pcmseq_recognizer(PCMFILE *);
int pcmseq_open(PCMFILE *);
void pcmseq_close(PCMFILE *fp);
int pcmseq_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcmseq_seek(PCMFILE *fp, int entry);
int pcmseq_ctl(PCMFILE *fp, int request, void *arg);
int pcmseq_stat(PCMFILE *fp, struct pcmstat *buf);
int pcmseq_write(PCMFILE *fp, short *buf, int nsamples);

#ifdef USING_ESPS
int pcmfeasd_recognizer(PCMFILE *);
int pcmfeasd_open(PCMFILE *);
void pcmfeasd_close(PCMFILE *fp);
int pcmfeasd_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcmfeasd_seek(PCMFILE *fp, int entry);
int pcmfeasd_ctl(PCMFILE *fp, int request, void *arg);
int pcmfeasd_stat(PCMFILE *fp, struct pcmstat *buf);
#endif

int expidx_recognizer(PCMFILE *);
int expidx_open(PCMFILE *);
void expidx_close(PCMFILE *fp);
int expidx_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int expidx_seek(PCMFILE *fp, int entry);
int expidx_ctl(PCMFILE *fp, int request, void *arg);
int expidx_stat(PCMFILE *fp, struct pcmstat *buf);
int expidx_write(PCMFILE *fp, short *buf, int nsamples);

#ifdef HAVE_MP3
int pcmmp3_recognizer(PCMFILE *);
int pcmmp3_open(PCMFILE *);
void pcmmp3_close(PCMFILE *fp);
int pcmmp3_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int pcmmp3_seek(PCMFILE *fp, int entry);
int pcmmp3_ctl(PCMFILE *fp, int request, void *arg);
int pcmmp3_stat(PCMFILE *fp, struct pcmstat *buf);
#endif

int arfhdf5_recognizer(PCMFILE *fp);
int arfhdf5_open(PCMFILE *fp);
void arfhdf5_close(PCMFILE *fp);
int arfhdf5_read(PCMFILE *fp, short **buf_p, int *nsamples_p);
int arfhdf5_seek(PCMFILE *fp, int entry);
int arfhdf5_ctl(PCMFILE *fp, int request, void *arg);
int arfhdf5_stat(PCMFILE *fp, struct pcmstat *buf);

#endif

