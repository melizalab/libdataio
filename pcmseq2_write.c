
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */


/*
** Note that the pcm_seq2 file format was originally written for use on
** VAX/VMS machines.  The time-stamps in the files, therefore follow
** the system time of the VAX/VMS OS, which count the number of 100 ns
** intervals since 00:00 hours, Nov. 17, 1858 (Smithsonian Institute
** astronomical time).
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifndef VMS
#include <unistd.h>
#endif
#include <sys/time.h>
#include "pcmio.h"
#include "pcmseq2_write.h"

/*
** We define these to convert from native bytesex to little-endian bytesex
*/
#if __BIG_ENDIAN__
#define sexconv16(in) ((((in) & 0x00ff) << 8 ) | (( (in) & 0xff00) >> 8))
#define sexconv32(in) ( (((in) & 0x000000ff) << 24 ) | (( (in) & 0x0000ff00) << 8) | (( (in) & 0x00ff0000) >> 8) | (( (in) & 0xff000000) >> 24) )
#define sexconv16_array(buf, cnt) \
	{\
		int n;\
		short *bufptr = (buf);\
		for (n=0; n < (cnt); n++)\
			bufptr[n] = sexconv16(bufptr[n]);\
	}
#define sexconv_needed
#else
#define sexconv16(in) (in)
#define sexconv32(in) (in)
#define sexconv16_array(buf, cnt)
#endif

#define putshort(fd, val) { short temp = sexconv16(val); rc |= (fwrite(&temp, 2, 1, (fd)) != 1); }
#define putlong(fd, val) { long temp = sexconv32(val); rc |= (fwrite(&temp, 4, 1, (fd)) != 1); }
#define putchars(fd, ptr, len) { rc |= (fwrite(ptr, len, 1, (fd)) != 1); }
#ifdef sexconv_needed
#define putdata(fd, data, nsamples) \
	{\
		int n;\
		for (n=0; n < (nsamples); n++)\
			putshort(fd, data[n]);\
	}
#else
#define putdata(fd, data, nsamples) { rc |= (fwrite(ptr, (nsamples)*2, 1, (fd)) != 1); }
#endif

static short zeropad[2048];
static int zeropadzeroed = 0;

int pcmseq2_write_hdr(PCMFILE *fp)
{
	int rc = 0;
	char basename[8];
	FILE *fd = fp->outfp;
	unsigned long long lltime;
	unsigned char jj;

	/*
	** This is for pcmseq2_write_data()
	*/
	if (zeropadzeroed == 0)
	{
		memset(zeropad, '\0', 2048 * sizeof(short));
		zeropadzeroed = 1;
	}

	/*
	** Prepare the PCM_SEQ2 init_key field
	*/
	strncpy(basename, fp->name, 7);
	basename[7] = '\0';
	strcpy(fp->pcmseq3_key, "                            ");			/* IMPORTANT: 28 spaces, then null */
	strcpy(fp->pcmseq3_key, " 2");
	strcat(fp->pcmseq3_key, basename);
	fp->pcmseq3_key[strlen(basename) + 2] = ' ';
	sprintf(fp->pcmseq3_key + 21, "%3d   1", fp->entry);

	/*
	** Write the entry header
	*/
#ifdef VMS
	putshort(fd, 2 + 28 + 8 + 4 + 4 + 4 + 4);							/* record size, in bytes */
#endif
	putshort(fd, 0x03);													/* control word */
	putchars(fd, fp->pcmseq3_key, 28);									/* init_key */

	if (fp->timestamp == 0)
	{
		struct timeval curtime;

		gettimeofday(&curtime, NULL);
		lltime = ((unsigned long long)(curtime.tv_sec) - 18000LL) * 10000000LL;
		lltime += (unsigned long long)(curtime.tv_usec) * 10LL;
		lltime += 0x007c95674beb4000LL;	/* 64-bit date time */
	}
	else
	{
		lltime = ((unsigned long long)(fp->timestamp) - 18000LL) * 10000000LL;
		lltime += fp->microtimestamp * 10LL;
		lltime += 0x007c95674beb4000LL;	/* 64-bit date time */
	}
	jj = lltime & 0xffLL; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff00LL) >> 8; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff0000LL) >> 16; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff000000LL) >> 24; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff00000000LL) >> 32; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff0000000000LL) >> 40; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff000000000000LL) >> 48; fwrite(&jj, 1, 1, fd);
	jj = (lltime & 0xff00000000000000LL) >> 56; fwrite(&jj, 1, 1, fd);

	putlong(fd, 2048);													/* segment_size */
	putlong(fd, 0x1);													/* pcm_start */
	putlong(fd, 0x20f01);												/* gain?? */
	putlong(fd, fp->samplerate);										/* samplerate */

	/*
	** Convert the init_key field into a match_key field
	*/
	fp->pcmseq3_key[1] = '3';											/* Turn init_key into a match_key */
	fp->pcmseq3_cursamp = 2048;
	fp->pcmseq3_entrysize = 0;

	if ((rc != 0) || feof(fd) || ferror(fd))
		return -1;
	return 0;
}

/*
** This routine MUST get called with a block of 2048 samples!
*/
int pcmseq2_write_2048(PCMFILE *fp, short *data, int lastsegment)
{
	int rc = 0;
	int recordwords, towritesamples;
	short *ptr = data;
	FILE *fd = fp->outfp;

	fp->pcmseq3_entrysize += 2048;
	recordwords = (lastsegment == 0) ? 0 : (fp->pcmseq3_entrysize);
	if ((lastsegment != 0) && (data == NULL)) data = zeropad;

	/* First seg */
	ptr = data;
	towritesamples = 1005;
#ifdef VMS
	putshort(fd, 0x7fc);												/* record size (1 short) */
#endif
	putshort(fd, 0x01);													/* signifies first segment (1 short) */
	putchars(fd, fp->pcmseq3_key, 28);									/* match_key (28 bytes) */
	putlong(fd, recordwords);											/* total record words (1 long) */
	putdata(fd, ptr, towritesamples);									/* the first 1005 samples */
	ptr += towritesamples;

	/* Second seg */
	towritesamples = 1021;
#ifdef VMS
	putshort(fd, 0x7fc);												/* record size (1 short) */
#endif
	putshort(fd, 0x00);													/* signifies middle segment (1 short) */
	putdata(fd, ptr, towritesamples);									/* the next 1021 samples */
	ptr += towritesamples;

	/* Third, and last seg */
	towritesamples = 22;
#ifdef VMS
	putshort(fd, 0x2e);													/* record size (1 short) */
#endif
	putshort(fd, 0x02);													/* signifies last segment (1 short) */
	putdata(fd, ptr, towritesamples);									/* the last 22 samples */

	if ((rc != 0) || feof(fd) || ferror(fd))
		return -1;
	return 0;
}


/*
** This routine can be called with variable numbers of samples.
** To do this, we maintain our state between calls.
** The state variable consists of 'cursamp', which is the
** current sample from 0 to 2047.  It's initial value must be 2048.
**
** After calling pcmseq2_write_hdr(), this routine can be called 
** several times in a row with variable numbers of samples.  To
** complete the current entry, set the lastsegment parameter to 1
** on the last call.  If this is not desirable, this routine can
** also be called with nsamples == 0 and lastsegment == 1 to close
** the entry.
**
** The entry may be padded with zero samples at the end to reach
** a 2048-tuple sample count.  The correct size will be written
** in the last record's recordwords field.
*/
#define FIRSTSEG 1
#define MIDDLESEG 2
#define LASTSEG 3

int pcmseq2_write_data(PCMFILE *fp, short *data, int nsamples, int lastsegment)
{
	int rc = 0;
	long recordwords, cursamp, togo;
	short state, *ptr = data;
	FILE *fd = fp->outfp;

	fp->pcmseq3_entrysize += nsamples;
	recordwords = 0;

	cursamp = fp->pcmseq3_cursamp;
	if (cursamp < 1005) state = FIRSTSEG;
	else if (cursamp >= 2026) state = LASTSEG;
	else state = MIDDLESEG;

	if ((lastsegment != 0) && (nsamples == 0))
	{
		nsamples = 2048 - cursamp;
		ptr = zeropad;
	}
	while (nsamples > 0)
	{
		switch(state)
		{
			case FIRSTSEG:
				togo = 1005 - cursamp;									/* how many 'til we can finish FIRSTSEG */
				if (nsamples >= togo)									/* we have enough to finish FIRSTSEG */
				{
					putdata(fd, ptr, togo);								/* write 'togo' samples */
					ptr += togo;
					nsamples -= togo;
					cursamp = 1005;
					state = MIDDLESEG;									/* transition to MIDDLESEG state */
#ifdef VMS
					putshort(fd, 0x7fc);								/* write middleseg header */
#endif
					putshort(fd, 0x00);
				}
				else
				{
					putdata(fd, ptr, nsamples);							/* can't end state, so write what we can */
					ptr += nsamples;
					cursamp += nsamples;
					nsamples = 0;
				}
				break;
			case MIDDLESEG:
				togo = 2026 - cursamp;									/* how many 'til we can finish MIDDLESEG */
				if (nsamples >= togo)									/* we have enough to finish MIDDLESEG */
				{
					putdata(fd, ptr, togo);
					ptr += togo;
					nsamples -= togo;
					cursamp = 2026;
					state = LASTSEG;									/* transition to LASTSEG state */
#ifdef VMS
					putshort(fd, 0x2e);									/* write lastseg header */
#endif
					putshort(fd, 0x02);
				}
				else
				{
					putdata(fd, ptr, nsamples);							/* can't end state, so write what we can */
					ptr += nsamples;
					cursamp += nsamples;
					nsamples = 0;
				}
				break;
			case LASTSEG:
				togo = 2048 - cursamp;									/* how many 'til we can finish LASTSEG */
				if (nsamples >= togo)									/* we have enough to finish LASTSEG */
				{
					if (togo > 0)										/* initially, it will be 0 */
					{
						putdata(fd, ptr, togo);
						ptr += togo;
						nsamples -= togo;
					}
					cursamp = 0;
					state = FIRSTSEG;									/* transition to FIRSTSEG state */
					if ((lastsegment == 0) || (nsamples > 0))			/* if there is going to be more data... */
					{
						if ((lastsegment != 0) && (nsamples <= 2048))	/* we are almost finished */
							recordwords = fp->pcmseq3_entrysize;
#ifdef VMS
						putshort(fd, 0x7fc);							/* write firstseg header */
#endif
						putshort(fd, 0x01);
						putchars(fd, fp->pcmseq3_key, 28);
						fp->pcmseq3_poscache = ftell(fd);
						putlong(fd, recordwords);
					}
				}
				else
				{
					putdata(fd, ptr, nsamples);							/* can't end state, so write what we can */
					ptr += nsamples;
					cursamp += nsamples;
					nsamples = 0;
				}
				break;
		}
		if ((lastsegment != 0) && (nsamples == 0) && (cursamp > 0))
		{
			nsamples = 2048 - cursamp;
			ptr = zeropad;
			continue;
		}
	}
	if (lastsegment != 0)
	{
		fseek(fd, fp->pcmseq3_poscache, 0);
		putlong(fd, fp->pcmseq3_entrysize);
		fseek(fd, 0, 2);
	}

	fp->pcmseq3_cursamp = cursamp;

	if ((rc != 0) || feof(fd) || ferror(fd))
		return -1;
	return 0;
}
#undef FIRSTSEG
#undef MIDDLESEG
#undef LASTSEG

