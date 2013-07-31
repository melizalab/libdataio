
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef VMS
#include <memory.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <errno.h>
#include "pcmio.h"

#if __BIG_ENDIAN__
#define sexconv16(in) ((((in) & 0x00ff) << 8 ) | (( (in) & 0xff00) >> 8))
#define sexconv16_array(buf, cnt) \
	{\
		int n;\
		short *bufptr = (buf);\
		for (n=0; n < (cnt); n++)\
			bufptr[n] = sexconv16(bufptr[n]);\
	}
#define NEED_TO_SEXCONV
#else
#define sexconv16_array(buf, cnt)
#endif

static int expidx_validate(int fd);
static int expidx_getsize(int fd, int entry, int *entrysize);
static int expidx_readentry(int fd, int entry, short *addr, int addrsize, int *numwritten);

/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int expidx_recognizer(PCMFILE *fp)
{
	if ((strncasecmp(fp->name + strlen(fp->name) - 8, ".exp_idx", 8)) &&
		(strncasecmp(fp->name + strlen(fp->name) - 7, ".expidx", 7)))
		return -1;
	return 0;
}

int expidx_open(PCMFILE *fp)
{
	int fd;

	fp->memalloctype = PCMIOMALLOC;
	fp->tempnam = NULL;
	fp->close = expidx_close;
	fp->read = expidx_read;
	fp->write = expidx_write;
	fp->seek = expidx_seek;
	fp->ctl = expidx_ctl;
	fp->stat = expidx_stat;
	fp->addr = NULL;
	fp->len = 0;
	fp->bufptr = NULL;
	fp->buflen = 0;
	fp->samplerate = 20000;
	fp->fd = -1;
	fp->outfp = NULL;
	fp->timestamp = 0;
	fp->microtimestamp = 0;
	fp->nentries = 0;
	if (fp->flags == O_RDONLY)
	{
		if ((fd = open(fp->name, O_RDONLY)) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		if (expidx_validate(fd) == -1)
		{
			pcm_errno = EIO;
			close(fd);
			return -1;
		}
		fp->nentries = 1;
		fp->fd = fd;
	}
	if (fp->flags == O_WRONLY)
		return -1;
	return 0;
}

void expidx_close(PCMFILE *fp)
{
	struct utimbuf timbuf;

	if (fp->flags == O_RDONLY)
	{
		/* free the malloc()ed memory */
		if (fp->addr != NULL)
			free(fp->addr);

		/* close the file */
		if (fp->fd != -1)
		{
			close(fp->fd);

			/* Set the filesystem time-stamp, now that the file is closed */
			if (fp->timestamp != 0)
			{
				timbuf.actime = fp->timestamp;
				timbuf.modtime = fp->timestamp;
				utime(fp->name, &timbuf);
			}
		}
	}
	return;
}

int expidx_read(PCMFILE *fp, short **buf_p, int *nsamples_p)
{
	int numwritten, entrysize;
	void *addr;
	int len;

	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->addr != NULL)
			free(fp->addr);
		fp->addr = NULL;
		fp->bufptr = NULL;
		fp->len = fp->buflen = 0;

		/*
		** Get the size
		*/
		if (expidx_getsize(fp->fd, fp->entry, &entrysize) != 0)
		{
			pcm_errno = EIO;
			return -1;
		}
		len = entrysize * 2;

		/*
		** malloc the needed space.
		*/
		if ((addr = (void *)malloc(len)) == NULL)
		{
			pcm_errno = EINVAL;
			return -1;
		}

		/*
		** read into the tmp file
		*/
		if ((expidx_readentry(fp->fd, fp->entry, (short *)addr, len/2, &numwritten) != 0) ||
			((numwritten != entrysize) && (numwritten != (len/2))))
		{
			pcm_errno = EIO;
			free(addr);
			return -1;
		}

		/*
		** We were successful
		*/
		fp->addr = addr;
		fp->len = len;
		fp->bufptr = addr;
		fp->buflen = fp->len;
		*buf_p = fp->bufptr;
		*nsamples_p = fp->buflen / 2;
		return 0;
	}
	pcm_errno = EOPNOTSUPP;
	return -1;
}

int expidx_seek(PCMFILE *fp, int entry)
{
	if (fp->flags == O_RDONLY)
	{
		fp->entry = entry;
		return 0;
	}
	pcm_errno = EINVAL;
	return -1;
}

int expidx_ctl(PCMFILE *fp, int request, void *arg)
{
	struct pcmstat buf;

	if (fp->flags == O_RDONLY)
	{
		/* Only allow a change if we haven't already read the file. */
		if ((request == PCMIOGETSIZE) && (arg != NULL))
		{
			expidx_stat(fp, &buf);
			*(int *)arg = buf.nsamples;
			return 0;
		}
		if ((request == PCMIOGETSR) && (arg != NULL))
		{
			expidx_stat(fp, &buf);
			*(int *)arg = buf.samplerate;
			return 0;
		}
		if ((request == PCMIOGETENTRY) && (arg != NULL))
		{
			*(int *)arg = fp->entry;
			return 0;
		}
		if ((request == PCMIOGETNENTRIES) && (arg != NULL))
		{
			*(int *)arg = fp->nentries;
			return 0;
		}
		if ((request == PCMIOGETTIME) && (arg != NULL))
		{
			expidx_stat(fp, &buf);
			*(int *)arg = buf.timestamp;
			return 0;
		}
		if ((request == PCMIOGETTIMEFRACTION) && (arg != NULL))
		{
			*(long *)arg = 0;
			return 0;
		}
	}
	if (fp->flags == O_WRONLY)
	{
		if ((request == PCMIOGETSR) && (arg != NULL))
		{
			*(int *)arg = fp->samplerate;
			return 0;
		}
		if ((request == PCMIOGETENTRY) && (arg != NULL))
		{
			*(int *)arg = fp->entry;
			return 0;
		}
		if ((request == PCMIOGETNENTRIES) && (arg != NULL))
		{
			*(int *)arg = fp->nentries;
			return 0;
		}
		if ((request == PCMIOGETTIME) && (arg != NULL))
		{
			*(int *)arg = fp->timestamp;
			return 0;
		}
		if ((request == PCMIOGETTIMEFRACTION) && (arg != NULL))
		{
			*(long *)arg = 0;
			return 0;
		}
		if ((request == PCMIOSETSR) && (arg != NULL))
		{
			fp->samplerate = *(int *)arg;
			return 0;
		}
		if ((request == PCMIOSETTIME) && (arg != NULL))
		{
			fp->timestamp = *(int *)arg;
			return 0;
		}
		if ((request == PCMIOSETTIMEFRACTION) && (arg != NULL))
		{
			return 0;
		}
	}
	pcm_errno = EINVAL;
	return -1;
}

int expidx_stat(PCMFILE *fp, struct pcmstat *buf)
{
	struct stat statbuf;
	int entrysize;

	if (fp->flags == O_RDONLY)
	{
		if (fp->fd != -1)
		{
			if (expidx_getsize(fp->fd, fp->entry, &entrysize) != 0)
			{
				pcm_errno = EIO;
				return -1;
			}
			statbuf.st_mtime = 0;
			fstat(fp->fd, &statbuf);

			buf->entry = fp->entry;
			buf->nentries = fp->nentries;
			buf->nsamples = entrysize;
			buf->samplerate = fp->samplerate;
			buf->timestamp = statbuf.st_mtime;
			return 0;
		}
	}
	pcm_errno = EINVAL;
	return -1;
}


/*
** This can be called repetitively; it will keep appending to the current entry.
** To finish the entry, call expidx_seek() with the next entry #.
** To finish the entry AND file, call expidx_close()
*/
int expidx_write(PCMFILE *fp, short *buf, int nsamples)
{
	pcm_errno = EINVAL;
	return -1;
}

static int expidx_validate(int fd)
{
	char addr[56];
	int pos;

	/*
	** I also don't support reading .exp_idx files over NFS
	** This is because I get all sorts of RMS junk this way.
	** However, ftp and rcp clean things up.  So, here, check
	** if things will work.
	*/
	pos = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);
	read(fd, addr, 56);
	if ((addr[0] != ' ') && (addr[1] != '1'))
		return -1;
	lseek(fd, pos, SEEK_SET);
	return 0;
}

static int expidx_getsize(int fd, int entry, int *entrysize)
{
	unsigned char addr[4];
	int nsamples, pos;

	/*
	** I don't support multiple entry .exp_idx files yet
	*/
	if (entry != 1)
		return -1;

	/*
	** Go read the entry length longword (it is in Intel little-endian format)
	*/
	pos = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0x44 + 56 + 4 + 4, SEEK_SET);
	read(fd, addr, 4);
	nsamples = (((unsigned long)(((unsigned char *)addr)[0])) | (((unsigned long)(((unsigned char *)addr)[1])) << 8) |
		(((unsigned long)(((unsigned char *)addr)[2])) << 16) | (((unsigned long)(((unsigned char *)addr)[3])) << 24));
	*entrysize = nsamples;
	lseek(fd, pos, SEEK_SET);
	return 0;
}

static char readbuf[4056];
static int expidx_readentry(int fd, int entry, short *addr, int addrsize, int *numwritten)
{
	int toread, pos, written = 0;
	short *ptr;

	pos = 152;
	ptr = addr;
	for (;;)
	{
		lseek(fd, pos, SEEK_SET);
		read(fd, readbuf, 4056);
		if ((readbuf[0] != ' ') && (readbuf[1] != '3'))
			break;
		toread = 2000;
		if (addrsize - written < 2000)
			toread = addrsize - written;
		memcpy(ptr, readbuf + 56, 2 * toread);
		written += toread;
		ptr += toread;
		readbuf[0] = '\0';
		if (written == addrsize)
			break;
		pos += 4056;
	}
	sexconv16_array(addr, written);
	*numwritten = written;
	return 0;
}

