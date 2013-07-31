/*
** toeio.h
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

#ifndef TOEIOHEADER
#define TOEIOHEADER

extern int toe_errno;

#ifdef linux
#ifdef MADV_SEQUENTIAL
#undef MADV_SEQUENTIAL
#endif
#define MADV_SEQUENTIAL
#define madvise(addr,len,flags)
#endif

#define TOEIOGETUNIT 10				/* arg = int* */
#define TOEIOGETNUNITS 11			/* arg = int* */
#define TOEIOGETNREPS 12			/* arg = int* */

typedef struct toefilestruct
{
	char *name;
	int flags;
	int entry;
	int fd;
	int memalloctype;
	void *addr;
	int len;

	float *toedata;
	int toe_numunits;
	int toe_numreps;
	int toe_unit;
	float **toe_repptrs;
	int *toe_repcounts;

	int (*open)();
	void (*close)();
	int (*read)();
	int (*write)();
	int (*seek)();
	int (*ctl)();
	int (*stat)();

	FILE *outfp;
} TOEFILE;

struct toestat
{
	int nunits;
	int nreps;
};

TOEFILE *toe_open(char *, char *);
void toe_close(TOEFILE *);
int toe_read(TOEFILE *fp, float ***toe_repptrs_p, int **toe_repcounts_p, int *toe_numunits_p, int *toe_numreps_p);
int toe_seek(TOEFILE *fp, int entry);
int toe_ctl(TOEFILE *fp, int request, void *arg);
int toe_stat(TOEFILE *fp, struct toestat *buf);
int toe_write(TOEFILE *fp, float **toe_repptrs, int *toe_repcounts, int toe_numunits, int toe_numreps);

int toelis_recognizer(TOEFILE *);
int toelis_open(TOEFILE *);
void toelis_close(TOEFILE *fp);
int toelis_read(TOEFILE *fp, float ***toe_repptrs_p, int **toe_repcounts_p, int *toe_numunits_p, int *toe_numreps_p);
int toelis_seek(TOEFILE *fp, int entry);
int toelis_ctl(TOEFILE *fp, int request, void *arg);
int toelis_stat(TOEFILE *fp, struct toestat *buf);
int toelis_write(TOEFILE *fp, float **toe_repptrs, int *toe_repcounts, int toe_numunits, int toe_numreps);

#endif

