
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

#define put_int16(val) \
	{\
		short tmpshort = sexconv16(val);\
		write(fp->fd, &tmpshort, 2);\
	}

#define put_int32(val) \
	{\
		int tmplong = sexconv32(val);\
		write(fp->fd, &tmplong, 4);\
	}

#define put_fourcc(val) \
	{\
		char hdr[5]; strcpy(hdr, val);\
		write(fp->fd, hdr, 4);\
	}

#define WAVE_FORMAT_UNKNOWN             (0x0000)
#define WAVE_FORMAT_PCM                 (0x0001) 
#define WAVE_FORMAT_ADPCM               (0x0002)
#define WAVE_FORMAT_ALAW                (0x0006)
#define WAVE_FORMAT_MULAW               (0x0007)
#define WAVE_FORMAT_OKI_ADPCM           (0x0010)
#define WAVE_FORMAT_DIGISTD             (0x0015)
#define WAVE_FORMAT_DIGIFIX             (0x0016)
#define IBM_FORMAT_MULAW                (0x0101)
#define IBM_FORMAT_ALAW                 (0x0102)
#define IBM_FORMAT_ADPCM                (0x0103)


static int wav_readhdr(int fd, int *nsamples_p, int *samplerate_p, short *nchans_p, short *resolution_p, long *pcmstartpos_p);


/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int pcmwav_recognizer(PCMFILE *fp)
{
	if (strncasecmp(fp->name + strlen(fp->name) - 4, ".wav", 4))
		return -1;
	return 0;
}

int pcmwav_open(PCMFILE *fp)
{
	fp->memalloctype = PCMIOMMAP;
	fp->tempnam = NULL;
	fp->close = pcmwav_close;
	fp->read = pcmwav_read;
	fp->write = pcmwav_write;
	fp->seek = pcmwav_seek;
	fp->ctl = pcmwav_ctl;
	fp->stat = pcmwav_stat;
	fp->addr = NULL;
	fp->len = 0;
	fp->bufptr = NULL;
	fp->buflen = 0;
	fp->samplerate = 20000;
	fp->fd = -1;
	fp->timestamp = 0;
	fp->microtimestamp = 0;
	fp->pcmseq3_entrysize = 0;
	fp->nentries = 1;
	fp->entry = 1;
	if (fp->flags == O_WRONLY)
	{
		if ((fp->fd = open(fp->name, O_CREAT|O_RDWR, 0666)) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		fp->pcmseq3_entrysize = 0;
		fp->wav_nbytes_addr1 = 0;
		fp->wav_nbytes_addr2 = 0;
	}
	else
	{
		int fd, rc;
		int nsamples, samplerate;
		short nchans, resolution;
		long pcmstartpos;

		/*
		** Open the file and get its samplerate
		*/
		rc = -1;
		if ((fd = open(fp->name, O_RDONLY)) != -1)
		{
			rc = wav_readhdr(fd, &nsamples, &samplerate, &nchans, &resolution, &pcmstartpos);
			close(fd);
		} else pcm_errno = errno;
		if (rc == -1) return -1;
		fp->samplerate = samplerate;
		fp->nentries = nchans;
		fprintf(stderr, "wav samplerate = %d\n", fp->samplerate);
		fprintf(stderr, "wav nchans = %d\n", fp->nentries);
		fprintf(stderr, "wav resolution = %d\n", resolution);
	}
	return 0;
}

void pcmwav_close(PCMFILE *fp)
{
	struct utimbuf timbuf;

	if (fp->flags == O_RDONLY)
	{
		if (fp->tempnam != NULL)
			free(fp->tempnam);
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
			int tmplong;

			lseek(fp->fd, fp->wav_nbytes_addr1, SEEK_SET);
			tmplong = sexconv32((fp->pcmseq3_entrysize * sizeof(short)) + 36);
			write(fp->fd, &tmplong, 4);

			lseek(fp->fd, fp->wav_nbytes_addr2, SEEK_SET);
			tmplong = sexconv32(fp->pcmseq3_entrysize * sizeof(short));
			write(fp->fd, &tmplong, 4);

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


int pcmwav_read(PCMFILE *fp, short **buf_p, int *nsamples_p)
{
	int i, fd, len;
	struct stat statbuf;
	void *addr;
	short nchans, resolution;
	int samplerate, nsamples;
	long pcmstartpos;
	char *databuf, *hdr;

	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->tempnam != NULL)
			free(fp->tempnam);
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
		** Read the wav file header, and leave the file positioned at the beginning of the
		** samples.  (pcmstartpos)
		*/
		if (wav_readhdr(fd, &nsamples, &samplerate, &nchans, &resolution, &pcmstartpos) == -1)
		{
			close(fd);
			return -1;
		}

		/*
		** Okay.  If PCMIOMMAP, then we open a temporary file.
		** Either mmap() this file directly, or malloc() some memory and read the orig file into it
		** NOTE: For mmap(), we don't close() the file descriptor.  For malloc(), we DO close() it.
		*/
		if (fp->memalloctype == PCMIOMMAP)
		{
			/*
			** Open a temporary file, mmap() it, read the orig into it, sexconvert it, then close
			** the original file.
			** NOTE: by unlinking this file immediately after it opens, I ensure that it gets
			** deleted after this program quits (even if it crashes).  It doesn't affect the usage
			** of the file while it is kept open, however.
			** The location of the temporary file can be modified by the environment variable TMPDIR.
			*/
			len = nsamples * sizeof(short);

			fp->tempnam = strdup("/tmp/pcmXXXXXX");
			if ((fp->fd = mkstemp(fp->tempnam)) == -1)
			{
				free(fp->tempnam);
				fp->tempnam = NULL;
				pcm_errno = errno;
				close(fd);
				return -1;
			}
			unlink(fp->tempnam);
			ftruncate(fp->fd, len);
			lseek(fp->fd, 0, SEEK_SET);

			/*
			** NOTE: right now, fd is the original file,
			** fp->fd is the temp file
			*/
			if ((addr = (void *)mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fp->fd, 0)) == (void *)-1)
			{
				close(fp->fd);
				close(fd);
				pcm_errno = errno;
				return -1;
			}
			madvise(addr, len, MADV_SEQUENTIAL);

			/*
			** Read into the mmap'ed region.
			** Remember that fd is the handle to the original .wav file, while fp->fd is a handle into
			** the memory-mapped region which will hold raw sample data for a specific channel (entry).
			*/
			lseek(fd, pcmstartpos, SEEK_SET);
			databuf = (char *)malloc(nchans * resolution / 8);
			for (i=0; i < nsamples; i++)
			{
				if (read(fd, databuf, nchans * resolution / 8) == -1)
				{
					free(databuf); databuf = NULL;
					pcm_errno = errno;
					munmap(addr, len);
					close(fp->fd);
					close(fd);
					return -1;
				}
				hdr = databuf + ((fp->entry - 1) * resolution / 8);
				((short *)addr)[i] = (resolution == 16) ? (*(short *)(hdr)) : ((short)((*(unsigned char *)(hdr)) - 128) << 8);
			}
			free(databuf); databuf = NULL;

			close(fd);
			fd = fp->fd;
			sexconv16_array(((short *)addr), (nsamples * nchans));
			mprotect(addr, len, PROT_READ);
			/* NOTE: now, fd and fp->fd are the temp file */
		}
		else if (fp->memalloctype == PCMIOMALLOC)
		{
			/*
			** Allocate memory
			*/
			len = nsamples * sizeof(short);
			if ((addr = (void *)malloc(len)) == NULL)
			{
				close(fd);
				pcm_errno = EINVAL;
				return -1;
			}

			/*
			** Read into the malloc'ed region.
			*/
			lseek(fd, pcmstartpos, SEEK_SET);
			databuf = (char *)malloc(nchans * resolution / 8);
			for (i=0; i < nsamples; i++)
			{
				if (read(fd, databuf, nchans * resolution / 8) == -1)
				{
					free(databuf); databuf = NULL;
					free(addr); addr = NULL;
					pcm_errno = errno;
					close(fd);
					return -1;
				}
				hdr = databuf + ((fp->entry - 1) * resolution / 8);
				((short *)addr)[i] = (resolution == 16) ? (*(short *)(hdr)) : ((short)((*(unsigned char *)(hdr)) - 128) << 8);
			}
			free(databuf); databuf = NULL;

			close(fd);
			fd = -1;
			sexconv16_array(((short *)addr), len);
		}
		else return -1;

		/*
		** Now it is read
		*/
		fp->fd = fd;
		fp->addr = addr;
		fp->len = len;
		fp->samplerate = samplerate;
		fprintf(stderr, "wav samplerate = %d\n", fp->samplerate);

		fp->bufptr = (short *)addr;
		fp->buflen = nsamples * 2;
		*buf_p = (short *)addr;
		*nsamples_p = nsamples;
		return 0;
	}
	pcm_errno = EOPNOTSUPP;
	return -1;
}

int pcmwav_seek(PCMFILE *fp, int entry)
{
	if (fp->flags == O_RDONLY)
	{
		if ((entry > 0) && (entry <= fp->nentries))
		{
			fp->entry = entry;
			return 0;
		}
	}
	pcm_errno = EOPNOTSUPP;
	return -1;
}

int pcmwav_ctl(PCMFILE *fp, int request, void *arg)
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
			pcmwav_stat(fp, &buf);
			*(int *)arg = buf.nsamples;
			return 0;
		}
		if ((request == PCMIOGETSR) && (arg != NULL))
		{
			pcmwav_stat(fp, &buf);
			*(int *)arg = buf.samplerate;
			return 0;
		}
		if ((request == PCMIOGETCAPS) && (arg != NULL))
		{
			pcmwav_stat(fp, &buf);
			*(int *)arg = buf.capabilities;
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
			pcmwav_stat(fp, &buf);
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
		if ((request == PCMIOGETCAPS) && (arg != NULL))
		{
			pcmwav_stat(fp, &buf);
			*(int *)arg = buf.capabilities;
			return 0;
		}
		if ((request == PCMIOGETENTRY) && (arg != NULL))
		{
			*(int *)arg = fp->entry;
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

int pcmwav_stat(PCMFILE *fp, struct pcmstat *buf)
{
	struct stat statbuf;
	int fd;
	int nsamples, samplerate;
	short nchans, resolution;

	if (fp->flags == O_RDONLY)
	{
		if ((fd = open(fp->name, O_RDONLY)) == -1)
		{
			pcm_errno = errno;
			return -1;
		}
		if (wav_readhdr(fd, &nsamples, &samplerate, &nchans, &resolution, NULL) == -1)
		{
			close(fd);
			return -1;
		}
		statbuf.st_mtime = 0;
		fstat(fd, &statbuf);
		close(fd);

		fp->samplerate = samplerate;
		buf->entry = fp->entry;
		buf->nentries = fp->nentries;
		buf->nsamples = nsamples;
		buf->samplerate = samplerate;
		buf->timestamp = statbuf.st_mtime;
		buf->capabilities = PCMIOCAP_SAMPRATE;
		return 0;
	}
	if (fp->flags == O_WRONLY)
	{
		buf->entry = fp->entry;
		buf->nsamples = fp->pcmseq3_entrysize;
		buf->samplerate = fp->samplerate;
		buf->timestamp = fp->timestamp;
		buf->capabilities = PCMIOCAP_SAMPRATE;
		return 0;
	}
	pcm_errno = EINVAL;
	return -1;
}


int pcmwav_write(PCMFILE *fp, short *buf, int nsamples)
{
	if (fp->flags == O_WRONLY)
	{
		if (fp->fd != -1)
		{
			/*
			** If we're just starting out, then write the .wav header
			*/
			if (fp->pcmseq3_entrysize == 0)
			{
				put_fourcc("RIFF");							/* 'RIFF' chunk */
				fp->wav_nbytes_addr1 = lseek(fp->fd, 0, SEEK_CUR);
				put_int32(0);								/*   len */
				put_fourcc("WAVE");							/*   'WAVE' chunk */
				put_fourcc("fmt ");							/*     'fmt' chunk */
				put_int32(16);								/*       len */
				put_int16(WAVE_FORMAT_PCM);					/*       format */
				put_int16(1);								/*       nchans */
				put_int32(fp->samplerate);					/*       samplerate */
				put_int32(fp->samplerate * 2);				/*       avg bytes/sec */
				put_int16(2);								/*       block alignment (?) */
				put_int16(16);								/*       bits per sample */
				put_fourcc("data");							/*     'data' chunk */
				fp->wav_nbytes_addr2 = lseek(fp->fd, 0, SEEK_CUR);
				put_int32(0);								/*       len */
			}

#ifdef NEED_TO_SEXCONV
			sexconv16_array(buf, nsamples);
			write(fp->fd, buf, nsamples * 2);					/*       (...) samples... */
			sexconv16_array(buf, nsamples);
#else
			write(fp->fd, buf, nsamples * 2);					/*       (...) samples... */
#endif
			fp->pcmseq3_entrysize += nsamples;
			return 0;
		}
	}
	pcm_errno = EINVAL;
	return -1;
}

static int wav_readhdr(int fd, int *nsamples_p, int *samplerate_p, short *nchans_p, short *resolution_p, long *pcmstartpos_p)
{
	int16_t nchans, resolution, fmt;
	int32_t samplerate, nsamples, pcmstartpos;
	char header[4];
	long pos;

	/*
	** Read the WAV file header information
	** Out of it, we are interested in:
	** 		short nchans == 1 or 2
	**		long samplerate
	**		short resolution == 8 or 16
	**		long nsamples
	**		int pcmstartpos == byte offset into the file where the raw pcm data occurs
	*/
	pos = lseek(fd, 0l, SEEK_SET);
	if ((read(fd, &header, 4) == -1) ||		/* Read riff chunk */
		(strncmp("RIFF", (char *)&header, 4) != 0) ||
		(read(fd, &header, 4) == -1) ||
		(read(fd, &header, 4) == -1) ||
		(strncmp("WAVE", (char *)&header, 4) != 0) ||
		(read(fd, &header, 4) == -1) ||		/* Read format sub-chunk */
		(strncmp("fmt ", (char *)&header, 4) != 0) ||
		(read(fd, &header, 4) == -1) ||
		(read(fd, &fmt, 2) == -1) ||		/* Read field: PCM format */
		(sexconv16(fmt) != WAVE_FORMAT_PCM) ||
		(read(fd, &nchans, 2) == -1) ||		/* Read field: 1 or 2 channels */
		(read(fd, &samplerate, 4) == -1) ||	/* Read field: samplerate */
		(read(fd, &header, 4) == -1) ||		/* Read field: avg bytes/sec */
		(read(fd, &header, 2) == -1) ||		/* Read field: block alignment */
		(read(fd, &resolution, 2) == -1) ||	/* Read field: bits per sample */
		(read(fd, &header, 4) == -1))	 	/* Read field: */
	{
		pcm_errno = errno;
		return -1;
	}
	pos = lseek(fd, 0l, SEEK_CUR);
	nchans = sexconv16(nchans);
	samplerate = sexconv32(samplerate);
	resolution = sexconv16(resolution);
	if ((header[0] == 'f') && (header[1] == 'a') && (header[2] == 'c') && (header[3] == 't'))
	{
		read(fd, &header, 4);
		read(fd, &nsamples, 4);
		read(fd, &header, 4);
	}
	else
		read(fd, &nsamples, 4);
	nsamples = sexconv32(nsamples);
	nsamples = nsamples / (nchans * resolution / 8);
	fprintf(stderr, "%d chans, %ld samples per chan, %d bits per sample\n", nchans, nsamples, resolution);

#ifdef notdef
	/*
	** Some programs (like the Sox version 12.15) write incorrect wav files where the nsamples
	** field is actually nsamples * nchans * resolution / 8
	*/
	if (fstat(fd, &statbuf) == 0)
	{
		int len;

		len = statbuf.st_size;
		if (nsamples * nchans * resolution / 8 > len)
		{
			fprintf(stderr, "Stupidly written file - correcting nsamples field from %ld to ", nsamples);
			nsamples = nsamples / (nchans * resolution / 8);
			fprintf(stderr, "%ld\n", nsamples);
		}
	}
#endif

	pcmstartpos = lseek(fd, 0, SEEK_CUR);
	if (((resolution != 8) && (resolution != 16)) || ((nchans != 1) && (nchans != 2)))
	{
		pcm_errno = EINVAL;
		return -1;
	}

	if (samplerate_p) *samplerate_p = samplerate;
	if (nsamples_p) *nsamples_p = nsamples;
	if (pcmstartpos_p) *pcmstartpos_p = pcmstartpos;
	if (nchans_p) *nchans_p = nchans;
	if (resolution_p) *resolution_p = resolution;
	return 0;
}

