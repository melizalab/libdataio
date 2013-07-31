
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

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
int pcmfix_recognizer(PCMFILE *fp)
{
	if ((strncasecmp(fp->name + strlen(fp->name) - 8, ".pcm_fix", 8)) &&
		(strncasecmp(fp->name + strlen(fp->name) - 7, ".pcmfix", 7)))
		return -1;
	return 0;
}

int pcmfix_open(PCMFILE *fp)
{
	int temp = 0;

	fp->memalloctype = PCMIOMMAP;
	fp->tempnam = NULL;
	fp->close = pcmfix_close;
	fp->read = pcmfix_read;
	fp->write = pcmfix_write;
	fp->seek = pcmfix_seek;
	fp->ctl = pcmfix_ctl;
	fp->stat = pcmfix_stat;
	fp->addr = NULL;
	fp->len = 0;
	fp->bufptr = NULL;
	fp->buflen = 0;
	fp->samplerate = 20000;
	fp->fd = -1;
	fp->timestamp = 0;
	fp->microtimestamp = 0;
	fp->entry = 1;
	fp->nentries = 0;
	if (fp->flags == O_WRONLY)
	{
		fp->pcmseq3_entrysize = 0;
		if ((fp->fd = open(fp->name, O_CREAT|O_WRONLY, 0666)) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		fp->nentries = 1;
		write(fp->fd, &temp, 4);
	}
	fp->nentries = 1;
	return 0;
}

void pcmfix_close(PCMFILE *fp)
{
	struct utimbuf timbuf;
	unsigned char temp;

	if (fp->flags == O_RDONLY)
	{
		if (fp->tempnam != NULL)
		{
			free(fp->tempnam);
			fp->tempnam = NULL;
		}
		/* Unmap the mapped file, OR free the malloc()ed memory */
		if (fp->addr != NULL)
		{
			if (fp->memalloctype == PCMIOMMAP)
			{
				munmap(fp->addr, fp->len);
				if (fp->fd != -1) close(fp->fd);			/* This might be a temp file or the orig file */
			}
			else if (fp->memalloctype == PCMIOMALLOC)
				free(fp->addr);
		}
	}
	if (fp->flags == O_WRONLY)
	{
		if (fp->fd != -1)
		{
			/*
			** Now that the file is written, we need to rewind to the
			** beginning and write the correct # of samples into the
			** first 4 bytes.  I do it this way to insure it writes
			** little-endian format, regardless of the local endian-ness.
			*/
			lseek(fp->fd, 0, SEEK_SET);
			temp = (fp->pcmseq3_entrysize & 0x000000ff);
			write(fp->fd, &temp, 1);
			temp = ((fp->pcmseq3_entrysize & 0x0000ff00) >> 8);
			write(fp->fd, &temp, 1);
			temp = ((fp->pcmseq3_entrysize & 0x00ff0000) >> 16);
			write(fp->fd, &temp, 1);
			temp = ((fp->pcmseq3_entrysize & 0xff000000) >> 24);
			write(fp->fd, &temp, 1);
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


int pcmfix_read(PCMFILE *fp, short **buf_p, int *nsamples_p)
{
	int fd, len;
	struct stat statbuf;
	void *addr;

	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->tempnam != NULL)
		{
			free(fp->tempnam);
			fp->tempnam = NULL;
		}
		if (fp->addr != NULL)
		{
			if (fp->memalloctype == PCMIOMMAP)
			{
				munmap(fp->addr, fp->len);
				close(fp->fd);
			}
			else if (fp->memalloctype == PCMIOMALLOC)
				free(fp->addr);
		}

		/*
		** Open the file and get its size, for mmap() and malloc()
		*/
		if ((fd = open(fp->name, O_RDONLY)) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		if (fstat(fd, &statbuf) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		len = statbuf.st_size;

		/*
		** Okay.  If (PCMIOMMAP) AND (we need to sex convert), then we open a temporary file.
		** Either mmap() the file directly, or malloc() some memory and read the file into it
		** NOTE: For mmap(), we don't close() the file descriptor.  For malloc(), we DO close() it.
		*/
		if (fp->memalloctype == PCMIOMMAP)
		{
#ifdef NEED_TO_SEXCONV
			/*
			** Open a temporary file, mmap() it, read the orig into it, sexconvert it, then close
			** the original file.
			** NOTE: by unlinking this file immediately after it opens, I ensure that it gets
			** deleted after this program quits (even if it crashes).  It doesn't affect the usage
			** of the file while it is kept open, however.
			** The location of the temporary file can be modified by the environment variable TMPDIR.
			*/
			fp->tempnam = strdup("/tmp/pcmXXXXXX");
			if ((fp->fd = mkstemp(fp->tempnam)) == -1)
			{
				free(fp->tempnam);
				fp->tempnam = NULL;
				pcm_errno = errno;
				return -1;
			}
			unlink(fp->tempnam);
			ftruncate(fp->fd, len);
			lseek(fp->fd, 0, SEEK_SET);
			/* NOTE: right now, fd is the original file, fp->fd is the temp file */
			if ((addr = (void *)mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fp->fd, 0)) == (void *)-1)
			{
				pcm_errno = errno;
				return -1;
			}
			madvise(addr, len, MADV_SEQUENTIAL);
			if (read(fd, addr, len) == -1)
			{
				pcm_errno = errno;
				munmap(addr, len);
				close(fd);
				return -1;
			}
			close(fd);
			fd = fp->fd;
			sexconv16_array(((short *)addr)+2, (len/2)-2);
			mprotect(addr, len, PROT_READ);
			/* NOTE: now, fd and fp->fd are the temp file */
#else
			if ((addr = (void *)mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0)) == (void *)-1)
			{
				close(fd);
				pcm_errno = errno;
				return -1;
			}
			madvise(addr, len, MADV_SEQUENTIAL);
			/* NOTE: now, fd is the original file */
#endif
		}
		else if (fp->memalloctype == PCMIOMALLOC)
		{
			if ((addr = (void *)malloc(len)) == NULL)
			{
				close(fd);
				pcm_errno = EINVAL;
				return -1;
			}
			if (read(fd, addr, len) == -1)
			{
				free(addr);
				close(fd);
				pcm_errno = errno;
				return -1;
			}
			close(fd);
			fd = -1;
			sexconv16_array(((short *)addr)+2, (len/2)-2);
		}
		else return -1;

		/*
		** Now it is read
		*/
		fp->fd = fd;
		fp->addr = addr;
		fp->len = len;
		/*
		** Interpret the first four bytes of the file as a little-endian long word
		** containing the number of samples
		*/
		fp->bufptr = (short *)(((char *)addr) + 4);
		fp->buflen = 2 * (((unsigned long)(((unsigned char *)addr)[0])) | (((unsigned long)(((unsigned char *)addr)[1])) << 8) |
			(((unsigned long)(((unsigned char *)addr)[2])) << 16) | (((unsigned long)(((unsigned char *)addr)[3])) << 24));
		*buf_p = fp->bufptr;
		*nsamples_p = fp->buflen / 2;
		return 0;
	}
	pcm_errno = EOPNOTSUPP;
	return -1;
}

int pcmfix_seek(PCMFILE *fp, int entry)
{
	if (entry != 1)
	{
		pcm_errno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

int pcmfix_ctl(PCMFILE *fp, int request, void *arg)
{
	struct pcmstat buf;

	if (fp->flags == O_RDONLY)
	{
		/* Only allow a change if we haven't already read the file. */
		if ((fp->addr == NULL) && ((request == PCMIOMALLOC) || (request == PCMIOMMAP)))
		{
			fp->memalloctype = request;
			return 0;
		}
		if ((request == PCMIOGETSIZE) && (arg != NULL))
		{
			pcmfix_stat(fp, &buf);
			*(int *)arg = buf.nsamples;
			return 0;
		}
		if ((request == PCMIOGETSR) && (arg != NULL))
		{
			pcmfix_stat(fp, &buf);
			*(int *)arg = buf.samplerate;
			return 0;
		}
		if ((request == PCMIOGETCAPS) && (arg != NULL))
		{
			pcmfix_stat(fp, &buf);
			*(int *)arg = buf.capabilities;
			return 0;
		}
		if ((request == PCMIOGETNENTRIES) && (arg != NULL))
		{
			*(int *)arg = fp->nentries;
			return 0;
		}
		if ((request == PCMIOGETENTRY) && (arg != NULL))
		{
			*(int *)arg = fp->entry;
			return 0;
		}
		if ((request == PCMIOGETTIME) && (arg != NULL))
		{
			pcmfix_stat(fp, &buf);
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
		if ((request == PCMIOGETCAPS) && (arg != NULL))
		{
			pcmfix_stat(fp, &buf);
			*(int *)arg = buf.capabilities;
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

int pcmfix_stat(PCMFILE *fp, struct pcmstat *buf)
{
	struct stat statbuf;
	int fd;
	char addr[4];

	if (fp->flags == O_RDONLY)
	{
		if ((fd = open(fp->name, O_RDONLY)) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		if (read(fd, addr, 4) == -1)
		{
			close(fd);
			pcm_errno = errno;
			return -1;
		}
		statbuf.st_mtime = 0;
		fstat(fd, &statbuf);
		close(fd);
		/*
		** Interpret the first four bytes of the file as a little-endian long word
		** containing the number of samples
		*/
		buf->entry = fp->entry;
		buf->nentries = fp->nentries;
		buf->nsamples = (((long)(((unsigned char *)addr)[0])) | (((long)(((unsigned char *)addr)[1])) << 8) |
			(((long)(((unsigned char *)addr)[2])) << 16) | (((long)(((unsigned char *)addr)[3])) << 24));
		buf->samplerate = fp->samplerate;
		buf->timestamp = statbuf.st_mtime;
		buf->capabilities = 0;
		return 0;
	}
	if (fp->flags == O_WRONLY)
	{
		buf->entry = fp->entry;
		buf->nentries = fp->nentries;
		buf->nsamples = fp->pcmseq3_entrysize;
		buf->samplerate = fp->samplerate;
		buf->timestamp = fp->timestamp;
		buf->capabilities = 0;
		return 0;
	}
	pcm_errno = EINVAL;
	return -1;
}

int pcmfix_write(PCMFILE *fp, short *buf, int nsamples)
{
	if (fp->flags == O_WRONLY)
	{
		if (fp->fd != -1)
		{
			sexconv16_array(buf, nsamples);
			write(fp->fd, buf, nsamples * 2);
			sexconv16_array(buf, nsamples);
			fp->pcmseq3_entrysize += nsamples;
			return 0;
		}
	}
	pcm_errno = EINVAL;
	return -1;
}

