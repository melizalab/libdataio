
/* Derived from pcmraw.c, Copyright (C) 2001 Amish Dave */
/* Changes Copyright (C) 2010 by Mike Lusignan (mlusigna@uchicago.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <errno.h>
#include "pcmio.h"

#include "arf_support.h"

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
#define NEED_TO_SEXCONV
#else
#define sexconv16(in) (in)
#define sexconv32(in) (in)
#define sexconv16_array(buf, cnt)
#endif


/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int arfhdf5_recognizer(PCMFILE *fp)
{
	const char *ARF_EXT = ".arf";

	return (strncasecmp(fp->name + strlen(fp->name) - strlen(ARF_EXT), ARF_EXT, strlen(ARF_EXT))) ? -1 : 0;
}

int arfhdf5_open(PCMFILE *fp)
{
        // attempt to open
	if ((fp->fd = H5Fopen(fp->name, H5F_ACC_RDONLY, H5P_DEFAULT)) == -1)
	{
		pcm_errno = errno;
		return -1;
	}
        /* set some fields of the struct. only ones known to be used by aplot */
        fp->flags = O_RDONLY;
	fp->close = arfhdf5_close;
	fp->read = arfhdf5_read;
	fp->write = NULL;
	fp->seek = arfhdf5_seek;
	fp->ctl = arfhdf5_ctl;
	fp->stat = arfhdf5_stat;
        fp->memalloctype = PCMIOMALLOC;
        // start counting entries
        hdf5_scan_entries(fp->fd, &(fp->nentries));
        // seek to the first entry
        arfhdf5_seek(fp, 1);

	return 0;
}

void arfhdf5_close(PCMFILE *fp)
{
	//H5Fclose(fp->fd);
}

int arfhdf5_read(PCMFILE *fp, short **buf_p, int *nsamples_p)
{
	int fd, len, sample_rate, timestamp, microtimestamp;
	void *addr;

	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->addr != NULL) {
                        free(fp->addr);
		}
		fp->addr = NULL;
		fp->bufptr = NULL;
		fp->len = fp->buflen = 0;

		/*
		** Open the file and get its size, for mmap() and malloc()
		*/
		fd = H5Fopen(fp->name, H5F_ACC_RDONLY, H5P_DEFAULT);
		if (fd == -1)
		{
			pcm_errno = errno;
			return -1;
		}

		/*
		** Either mmap() the file directly, or malloc() some memory and read the file into it.
		** NOTE: For mmap(), we don't close() the file descriptor. For malloc(), we DO close() it.
		*/
                if (hdf5_read_entry(fd, fp->entry-1, &len, &sample_rate, &timestamp, &microtimestamp,
                                    &addr) == -1)
                {
                        free(addr);
                        H5Fclose(fd);
                        pcm_errno = errno;
                        return -1;
                }
                H5Fclose(fd);
                fd = -1;
                sexconv16_array(((short *)addr), len);

		/*
		** Now it is read
		*/
		fp->fd = fd;
		fp->addr = addr;
		fp->len = len * sizeof(short);
		fp->bufptr = (short *)addr;
		fp->buflen = fp->len;
		fp->samplerate = sample_rate;
		fp->timestamp = timestamp;
		fp->microtimestamp = microtimestamp;
		*buf_p = fp->bufptr;
		*nsamples_p = len;
		return 0;
	}
	pcm_errno = EOPNOTSUPP;
	return -1;
}

int arfhdf5_seek(PCMFILE *fp, int entry)
{
	if (entry > fp->nentries)
	{
		pcm_errno = EOPNOTSUPP;
		return -1;
	}
	fp->entry = entry;
	return 0;
}

int arfhdf5_ctl(PCMFILE *fp, int request, void *arg)
{
	if (request == PCMIOMMAP)
	{
		pcm_errno = EINVAL;
		return -1;
	}
	if ((request == PCMIOGETTIME) && (arg != NULL))
	{
		*(int *)arg = fp->timestamp;
		return 0;
	}
	if ((request == PCMIOGETTIMEFRACTION) && (arg != NULL))
	{
		*(long *)arg = fp->microtimestamp;
		return 0;
	}
	if ((request == PCMIOGETSIZE) && (arg != NULL))
	{
		*(int *)arg = fp->buflen;
		return 0;
	}

	return pcmraw_ctl(fp, request, arg);
}

int arfhdf5_stat(PCMFILE *fp, struct pcmstat *buf)
{
        buf->entry = fp->entry;
        buf->nentries = fp->nentries;
        buf->samplerate = fp->samplerate;
        buf->nsamples = fp->len / sizeof(short);
        buf->timestamp = fp->timestamp;
        buf->capabilities = 0;
        return 0;
}
