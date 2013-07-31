/*
** lblio.h
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

#ifndef LBLIOHEADER
#define LBLIOHEADER

extern int lbl_errno;

#ifdef linux
#ifdef MADV_SEQUENTIAL
#undef MADV_SEQUENTIAL
#endif
#define MADV_SEQUENTIAL
#define madvise(addr,len,flags)
#endif

#define LBLIOGETNLBLS 10			/* arg = int* */

typedef struct lblfilestruct
{
	char *name;
	int flags;
	int entry;
	int fd;
	int memalloctype;
	void *addr;
	int len;

	float *lbltimes;
	char **lblnames;
	int lblcount;

	int (*open)();
	void (*close)();
	int (*read)();
	int (*write)();
	int (*seek)();
	int (*ctl)();
	int (*stat)();

	FILE *outfp;
} LBLFILE;

struct lblstat
{
	int	lblcount;
};

LBLFILE *lbl_open(char *, char *);
void lbl_close(LBLFILE *);
int lbl_read(LBLFILE *fp, float **lbltimes_p, char ***lblnames_p, int *lblcount_p);
int lbl_seek(LBLFILE *fp, int entry);
int lbl_ctl(LBLFILE *fp, int request, void *arg);
int lbl_stat(LBLFILE *fp, struct lblstat *buf);
int lbl_write(LBLFILE *fp, float *lbltimes, char **lblnames, int lblcount);

int lblesps_recognizer(LBLFILE *);
int lblesps_open(LBLFILE *);
void lblesps_close(LBLFILE *fp);
int lblesps_read(LBLFILE *fp, float **lbltimes_p, char ***lblnames_p, int *lblcount_p);
int lblesps_seek(LBLFILE *fp, int entry);
int lblesps_ctl(LBLFILE *fp, int request, void *arg);
int lblesps_stat(LBLFILE *fp, struct lblstat *buf);
int lblesps_write(LBLFILE *fp, float *lbltimes, char **lblnames, int lblcount);

int lbltoe_recognizer(LBLFILE *);
int lbltoe_open(LBLFILE *);
void lbltoe_close(LBLFILE *fp);
int lbltoe_read(LBLFILE *fp, float **lbltimes_p, char ***lblnames_p, int *lblcount_p);
int lbltoe_seek(LBLFILE *fp, int entry);
int lbltoe_ctl(LBLFILE *fp, int request, void *arg);
int lbltoe_stat(LBLFILE *fp, struct lblstat *buf);
int lbltoe_write(LBLFILE *fp, float *lbltimes, char **lblnames, int lblcount);

#endif

