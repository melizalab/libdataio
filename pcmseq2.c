
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
#include <sys/types.h>
#include <errno.h>
#include "pcmio.h"
#include "pcmseq2_read.h"
#include "pcmseq2_write.h"

/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int pcmseq_recognizer(PCMFILE *fp)
{
	if ((strncasecmp(fp->name + strlen(fp->name) - 9, ".pcm_seq2", 9)) &&
		(strncasecmp(fp->name + strlen(fp->name) - 8, ".pcm_seq", 8)) &&
		(strncasecmp(fp->name + strlen(fp->name) - 8, ".pcmseq2", 8)) &&
		(strncasecmp(fp->name + strlen(fp->name) - 7, ".pcmseq", 7)))
		return -1;
	return 0;
}

int pcmseq_open(PCMFILE *fp)
{
	fp->memalloctype = PCMIOMMAP;
	fp->tempnam = NULL;
	fp->close = pcmseq_close;
	fp->read = pcmseq_read;
	fp->write = pcmseq_write;
	fp->seek = pcmseq_seek;
	fp->ctl = pcmseq_ctl;
	fp->stat = pcmseq_stat;
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
	fp->entry = 1;
	if (fp->flags == O_RDONLY)
	{
		if ((fp->p2file = pcmseq2_open(fp->name)) == NULL)
		{
			pcm_errno = EIO;
			return -1;
		}
		pcmseq2_getinfo(fp->p2file, 1, NULL, &(fp->samplerate), NULL, &(fp->nentries));
	}
	if (fp->flags == O_WRONLY)
	{
		fp->entrystarted = 0;
		if ((fp->outfp = fopen(fp->name, "w")) == NULL)
		{
			pcm_errno = errno;
			return -1;
		}
	}
	return 0;
}

void pcmseq_close(PCMFILE *fp)
{
	if (fp->flags == O_RDONLY)
	{
		/* Unmap the mapped file, OR free the malloc()ed memory */
		if (fp->addr != NULL)
		{
			if (fp->memalloctype == PCMIOMMAP)
				munmap(fp->addr, fp->len);
			else if (fp->memalloctype == PCMIOMALLOC)
				free(fp->addr);
		}
		/* Clean up after the temporary file */
		if (fp->tempnam != NULL)
		{
			close(fp->fd);
			free(fp->tempnam);
			fp->tempnam = NULL;
		}
		/* Close and free the pcmseq2-reading file structures */
		if (fp->p2file != NULL)
			pcmseq2_close(fp->p2file);
	}
	if (fp->flags == O_WRONLY)
	{
		/* Mark current entry finished. */
		if (fp->entrystarted != 0) pcmseq2_write_data(fp, NULL, 0, 1);
		if (fp->outfp != NULL) fclose(fp->outfp);
	}
	return;
}

int pcmseq_read(PCMFILE *fp, short **buf_p, int *nsamples_p)
{
	long numwritten, entrysize;
	void *addr;
	int len;
	int samplerate;

	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->addr != NULL)
		{
			if (fp->memalloctype == PCMIOMMAP)
				munmap(fp->addr, fp->len);
			else if (fp->memalloctype == PCMIOMALLOC)
				free(fp->addr);
		}
		fp->addr = NULL;
		fp->bufptr = NULL;
		fp->len = fp->buflen = 0;

		/*
		** Get the size
		*/
		if (pcmseq2_getinfo(fp->p2file, fp->entry, &entrysize, &samplerate, NULL, NULL) != 0)
		{
			pcm_errno = EIO;
			return -1;
		}
		len = entrysize * 2;
		fp->samplerate = samplerate;

		/*
		** Either mmap the tmp file (extending it to the proper size)
		** or malloc the needed space.
		*/
		if (fp->memalloctype == PCMIOMMAP)
		{
			/*
			** Open a temporary file; each call to read() reads an entry into this file;
			** the file is then mmap'ed (in read()). seek() determines which entry will be read next.
			** NOTE: by unlinking this file immediately after it opens, I ensure that it gets
			** deleted after this program quits (even if it crashes).  It doesn't affect the usage
			** of the file while it is kept open, however.
			** The location of the temporary file can be modified by the environment variable TMPDIR.
			** NOTE: it can be used multiple times, if pcmseq_read() is called again...
			*/
			if (fp->tempnam == NULL)
			{
				fp->tempnam = strdup("/tmp/pcmXXXXXX");
				if ((fp->fd = mkstemp(fp->tempnam)) == -1)
				{
					free(fp->tempnam);
					fp->tempnam = NULL;
					pcm_errno = errno;
					return -1;
				}
				unlink(fp->tempnam);
			}
			ftruncate(fp->fd, len);
			lseek(fp->fd, 0, SEEK_SET);
			addr = (void *)mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fp->fd, 0);
			if (addr == (void *)-1)
			{
				pcm_errno = errno;
				return -1;
			}
			madvise(addr, len, MADV_SEQUENTIAL);
		}
		else if (fp->memalloctype == PCMIOMALLOC)
		{
			if ((addr = (void *)malloc(len)) == NULL)
			{
				pcm_errno = EINVAL;
				return -1;
			}
		}
		else return -1;

		/*
		** read into the tmp file
		*/
		if ((pcmseq2_read(fp->p2file, fp->entry, 0, len/2 - 1, addr, &numwritten, &entrysize) != 0) ||
			((numwritten != entrysize) && (numwritten != (len/2))))
		{
			pcm_errno = EIO;
			if (fp->memalloctype == PCMIOMMAP)
				munmap(addr, len);
			else if (fp->memalloctype == PCMIOMALLOC)
				free(addr);
			return -1;
		}
		if (fp->memalloctype == PCMIOMMAP)
			mprotect(addr, len, PROT_READ);

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

int pcmseq_seek(PCMFILE *fp, int entry)
{
	if (fp->flags == O_WRONLY)
	{
		if (fp->outfp != NULL)
		{
			if (fp->entrystarted != 0)
			{
				/*
				** Mark current entry finished; clear entrystarted so pcmseq_write() can write a new header.
				*/
				pcmseq2_write_data(fp, NULL, 0, 1);
				fp->entrystarted = 0;
			}
			fp->entry = entry;
			return 0;
		}
	}
	if (fp->flags == O_RDONLY)
	{
		if ((entry > 0) && (entry <= fp->nentries))
		{
			fp->entry = entry;
			return 0;
		}
	}
	pcm_errno = EINVAL;
	return -1;
}

int pcmseq_ctl(PCMFILE *fp, int request, void *arg)
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
			pcmseq_stat(fp, &buf);
			*(int *)arg = buf.nsamples;
			return 0;
		}
		if ((request == PCMIOGETSR) && (arg != NULL))
		{
			pcmseq_stat(fp, &buf);
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
			pcmseq_stat(fp, &buf);
			*(int *)arg = buf.timestamp;
			return 0;
		}
		if ((request == PCMIOGETTIMEFRACTION) && (arg != NULL))
		{
			pcmseq_stat(fp, &buf);
			*(long *)arg = buf.microtimestamp;
			return 0;
		}
	}
	if (fp->flags == O_WRONLY)
	{
		if ((request == PCMIOGETSIZE) && (arg != NULL))
		{
			pcmseq_stat(fp, &buf);
			*(int *)arg = buf.nsamples;
			return 0;
		}
		if ((request == PCMIOGETCAPS) && (arg != NULL))
		{
			pcmseq_stat(fp, &buf);
			*(int *)arg = buf.capabilities;
			return 0;
		}
		if ((request == PCMIOGETSR) && (arg != NULL))
		{
			pcmseq_stat(fp, &buf);
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
			fp->microtimestamp = *(long *)arg;
			return 0;
		}
	}
	pcm_errno = EINVAL;
	return -1;
}

int pcmseq_stat(PCMFILE *fp, struct pcmstat *buf)
{
	long entrysize;
	int samplerate, nentries;
	unsigned long long datetime;

	if (fp->flags == O_RDONLY)
	{
		if (fp->p2file != NULL)
		{
			if (pcmseq2_getinfo(fp->p2file, fp->entry, &entrysize, &samplerate, &datetime, &nentries) != 0)
			{
				pcm_errno = EIO;
				return -1;
			}
			buf->entry = fp->entry;
			buf->nentries = nentries;
			buf->nsamples = entrysize;
			buf->samplerate = samplerate;
			buf->timestamp = ((datetime - 0x007c95674beb4000LL) / 10000000 + 18000);
			buf->microtimestamp = ((datetime - 0x007c95674beb4000LL) % 10000000 / 10);
			buf->capabilities = PCMIOCAP_MULTENTRY | PCMIOCAP_SAMPRATE;
			return 0;
		}
	}
	if (fp->flags == O_WRONLY)
	{
		buf->entry = fp->entry;
		buf->nsamples = fp->pcmseq3_entrysize;
		buf->samplerate = fp->samplerate;
		buf->capabilities = PCMIOCAP_MULTENTRY | PCMIOCAP_SAMPRATE;
		return 0;
	}
	pcm_errno = EINVAL;
	return -1;
}


/*
** This can be called repetitively; it will keep appending to the current entry.
** To finish the entry, call pcmseq_seek() with the next entry #.
** To finish the entry AND file, call pcmseq_close()
*/
int pcmseq_write(PCMFILE *fp, short *buf, int nsamples)
{
	int rc;

	rc = -1;
	if (fp->flags == O_WRONLY)
	{
		rc = 0;
		if (fp->outfp != NULL)
		{
			if (fp->entrystarted == 0)
			{
				rc = pcmseq2_write_hdr(fp);
				fp->entrystarted = 1;
			}
			if (rc == 0) rc = pcmseq2_write_data(fp, buf, nsamples, 0);
			return rc;
		}
	}
	pcm_errno = EINVAL;
	return rc;
}


