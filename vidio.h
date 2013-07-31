/*
** vidio.h
**
** header file for libdataio, a library for reading certain types of data
** such as sampled sound/electrical data, labels (named events occuring at
** specific times), or spike times of events for neuronal action potential
** firing.   This library is primarily used by my programs aplot and by the
** a-ware set of programs for scientific data collection and analysis.
**
** Copyright (C) by Amish S. Dave 2004
**
** This code is distributed under the GNU GENERAL PUBLIC LICENSE (GPL)
** Version 2 (June 1991). See the "COPYING" file distributed with this software
** for more info.
*/

#ifndef VIDIOHEADER
#define VIDIOHEADER

extern int vid_errno;

#define VIDIOGETNENTRIES 1
#define VIDIOGETNFRAMES 2
#define VIDIOGETMICROSECPERFRAME 3
#define VIDIOGETWIDTH 4
#define VIDIOGETHEIGHT 5
#define VIDIOGETNCOMPONENTS 6
#define VIDIOGETMAXRAWFRAMEBYTES 7
#define VIDIOGETSTARTNTIME 8
#define VIDIOGETSTOPNTIME 9
#define VIDIOGETCLOSESTFRAME 10
#define VIDIOGETCAPS 11				/* Get the capabilities of this file format... Includes MULTENTRY */

typedef struct vidclosestframestruct
{
	unsigned long long ntime;
	int framenum;
} VIDCLOSESTFRAME;

typedef struct vidfilestruct
{
	char *name;
	int flags;
	int fd;
	short *bufptr;
	int buflen;
	int len;
	void *addr;
	int entry;

	int (*open)();
	void (*close)();
	int (*read)();
	int (*write)();
	int (*seek)();
	int (*ctl)();
	int (*stat)();

	unsigned long microsec_per_frame;
	unsigned long tot_frames;
	int cur_frame;
	unsigned long video_width;
	unsigned long video_height;
	unsigned long n_components;

	int samplerate;
	int timestamp;
	long microtimestamp;
	int nentries;

	unsigned long *frameindexes;
	unsigned long *framebytes;
	unsigned long long *framentimes;
	int maxframebytes;

	void *extra;
} VIDFILE;

struct vidstat
{
	int entry;
	int nentries;
	int tot_frames;
	int microsec_per_frame;
	int video_width;
	int video_height;
	int n_components;
	int maxframebytes;
	unsigned long long startntime;
	unsigned long long stopntime;
	int capabilities;
};

VIDFILE *vid_open(char *, char *);
void vid_close(VIDFILE *);
int vid_read(VIDFILE *fp, int framenum, char *framedata, unsigned long long *framentime);
int vid_seek(VIDFILE *fp, int entry);
int vid_ctl(VIDFILE *fp, int request, void *arg);
int vid_stat(VIDFILE *fp, struct vidstat *buf);
int vid_write(VIDFILE *fp, short *buf, int nsamples);

int vidavi_recognizer(VIDFILE *);
int vidavi_open(VIDFILE *);
void vidavi_close(VIDFILE *fp);
int vidavi_read(VIDFILE *fp, int framenum, char *framedata, unsigned long long *framentime);
int vidavi_seek(VIDFILE *fp, int entry);
int vidavi_ctl(VIDFILE *fp, int request, void *arg);
int vidavi_stat(VIDFILE *fp, struct vidstat *buf);
int vidavi_write(VIDFILE *fp, short *buf, int nsamples);

#endif

