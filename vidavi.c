
/* Copyright (C) 2004 by Amish S. Dave (adave3@uic.edu) */

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

#include "jpeglib.h"

#include "vidio.h"

typedef struct
{
	int cjpeg_initted;
	struct jpeg_decompress_struct cjpeg;
	struct jpeg_error_mgr jerr;
	char *jpeg_line;
	unsigned char *jpeg_inbuffer;
	int jpeg_inbuffer_size;
} VIDEXTRA;

static void *alloc_vidextra(int width, int height, int n_components);
static void free_vidextra(void *extra);

static unsigned long get_lsb32(int fd)
{
	unsigned long lval = 0;
	read(fd, &lval, 4);
	return lval;
}

static unsigned short get_lsb16(int fd)
{
	unsigned short sval = 0;
	read(fd, &sval, 2);
	return sval;
}

/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int vidavi_recognizer(VIDFILE *fp)
{
	if (strncasecmp(fp->name + strlen(fp->name) - 4, ".avi", 4))
		return -1;
	return 0;
}

int vidavi_open(VIDFILE *fp)
{
	struct stat statbuf;
	char cbuf[5];
	int i;

	cbuf[4] = '\0';
	fp->fd = -1;
	fp->close = vidavi_close;
	fp->read = vidavi_read;
	fp->write = vidavi_write;
	fp->seek = vidavi_seek;
	fp->ctl = vidavi_ctl;
	fp->stat = vidavi_stat;

	fp->framentimes = NULL;
	fp->frameindexes = NULL;
	fp->framebytes = NULL;
	fp->maxframebytes = 0;

	fp->len = 0;
	fp->addr = NULL;
	fp->entry = 1;
	fp->nentries = 1;
	fp->timestamp = 0;
	fp->microtimestamp = 0;


	if ((fp->fd = open(fp->name, O_RDONLY)) == -1)
	{
		vid_errno = errno;
		return -1;
	}
	if (fstat(fp->fd, &statbuf) == -1)
	{
		vid_errno = errno;
		return -1;
	}
	fp->len = statbuf.st_size;

	/*
	** Parse header
	*/
	lseek(fp->fd, 32, SEEK_SET);
	fp->microsec_per_frame = get_lsb32(fp->fd);
	lseek(fp->fd, 48, SEEK_SET);
	fp->tot_frames = get_lsb32(fp->fd);
	lseek(fp->fd, 64, SEEK_SET);
	fp->video_width = get_lsb32(fp->fd);
	fp->video_height = get_lsb32(fp->fd);
	lseek(fp->fd, 186, SEEK_SET);
	fp->n_components = get_lsb16(fp->fd);
	fp->n_components /= 8;

	fprintf(stderr, "microsec_per_frame = %ld\n", fp->microsec_per_frame);
	fprintf(stderr, "tot_frames = %ld\n", fp->tot_frames);
	fprintf(stderr, "video_width = %ld\n", fp->video_width);
	fprintf(stderr, "video_height = %ld\n", fp->video_height);
	fprintf(stderr, "n_components = %ld\n", fp->n_components);

	/*
	** Read the timemarks
	*/
	fp->framentimes = (unsigned long long *)calloc(fp->tot_frames, sizeof(unsigned long long));
	lseek(fp->fd, fp->len - (fp->tot_frames * 16) - 8 - (fp->tot_frames * 8) - 8, SEEK_SET);
	read(fp->fd, cbuf, 4);
	if (!strcmp(cbuf, "JUNK"))
	{
		unsigned long long temp1, temp2;
		temp1 = get_lsb32(fp->fd);
		for (i=0; i < fp->tot_frames; i++)
		{
			temp1 = get_lsb32(fp->fd);
			temp2 = get_lsb32(fp->fd);
			fp->framentimes[i] = temp1 | (temp2 << 32);
		}
	}
	else
	{
		fprintf(stderr, "vidavi_open(): unable to read timemarks from '%s'\n", fp->name);
	}

	/*
	** Read the index
	*/
	fp->frameindexes = (unsigned long *)calloc(fp->tot_frames, sizeof(unsigned long));
	fp->framebytes = (unsigned long *)calloc(fp->tot_frames, sizeof(unsigned long));
	lseek(fp->fd, fp->len - (fp->tot_frames * 16) - 8, SEEK_SET);
	read(fp->fd, cbuf, 4);
	if (!strcmp(cbuf, "idx1"))
	{
		unsigned long junk;
		junk = get_lsb32(fp->fd);
		for (i=0; i < fp->tot_frames; i++)
		{
			junk = get_lsb32(fp->fd);
			junk = get_lsb32(fp->fd);
			fp->frameindexes[i] = get_lsb32(fp->fd);
			fp->framebytes[i] = get_lsb32(fp->fd);
		}
	}
	else
	{
		fprintf(stderr, "vidavi_open(): unable to read index from '%s'\n", fp->name);
	}

	fp->extra = alloc_vidextra(fp->video_width, fp->video_height, fp->n_components);

	fp->maxframebytes = 0;
	for (i=0; i < fp->tot_frames; i++)
	{
		if (fp->framebytes[i] > fp->maxframebytes)
			fp->maxframebytes = fp->framebytes[i];
	}
#ifdef notdef
	for (i=0; i < fp->tot_frames; i++)
	{
		fprintf(stderr, "%4d\t%qd\t%ld\t%ld\n", i, fp->framentimes[i], fp->frameindexes[i], fp->framebytes[i]);
	}
#endif
	return 0;
}

void vidavi_close(VIDFILE *fp)
{
	if (fp->flags == O_RDONLY)
	{
		if (fp->fd != -1) close(fp->fd);
		if (fp->framentimes) free(fp->framentimes);
		if (fp->frameindexes) free(fp->frameindexes);
		if (fp->framebytes) free(fp->framebytes);
		if (fp->extra) free_vidextra(fp->extra);
	}
	return;
}

/*
** Writes the frame data for frame number 'framenum' into the memory pointed at by 'framedata',
** which must be allocated to have been at least equal to height * width * n_components bytes.
** The ntime for this frame is returned in 'framentime'.
*/
int vidavi_read(VIDFILE *fp, int framenum, char *framedata, unsigned long long *framentime)
{
	VIDEXTRA *vidextra = (VIDEXTRA *)fp->extra;
	int row_stride;
	JSAMPROW row_pointer;

	if (fp->flags == O_RDONLY)
	{
		if ((framenum < 0) || (framenum >= fp->tot_frames))
		{
			vid_errno = ERANGE;
			return -1;
		}
		lseek(fp->fd, fp->frameindexes[framenum], SEEK_SET);
		read(fp->fd, vidextra->jpeg_inbuffer, fp->framebytes[framenum]);

		jpeg_read_header(&vidextra->cjpeg, TRUE);
		jpeg_start_decompress(&vidextra->cjpeg);
		row_stride = vidextra->cjpeg.output_width * vidextra->cjpeg.output_components;
		while (vidextra->cjpeg.output_scanline < vidextra->cjpeg.output_height)
		{
			row_pointer = &framedata[vidextra->cjpeg.output_scanline * row_stride];
			jpeg_read_scanlines(&vidextra->cjpeg, &row_pointer, 1);
		}
		jpeg_finish_decompress(&vidextra->cjpeg);
		*framentime = fp->framentimes[framenum];
		return 0;
	}
	vid_errno = EOPNOTSUPP;
	return -1;
}

int vidavi_seek(VIDFILE *fp, int entry)
{
	vid_errno = EOPNOTSUPP;
	return -1;
}

int vidavi_ctl(VIDFILE *fp, int request, void *arg)
{
	if (fp->flags == O_RDONLY)
	{
		if ((request == VIDIOGETNENTRIES) && (arg != NULL))
		{
			*(int *)arg = fp->nentries;
			return 0;
		}
		else if ((request == VIDIOGETNFRAMES) && (arg != NULL))
		{
			*(int *)arg = fp->tot_frames;
			return 0;
		}
		else if ((request == VIDIOGETMICROSECPERFRAME) && (arg != NULL))
		{
			*(int *)arg = fp->microsec_per_frame;
			return 0;
		}
		else if ((request == VIDIOGETWIDTH) && (arg != NULL))
		{
			*(int *)arg = fp->video_width;
			return 0;
		}
		else if ((request == VIDIOGETHEIGHT) && (arg != NULL))
		{
			*(int *)arg = fp->video_height;
			return 0;
		}
		else if ((request == VIDIOGETNCOMPONENTS) && (arg != NULL))
		{
			*(int *)arg = fp->n_components;
			return 0;
		}
		else if ((request == VIDIOGETMAXRAWFRAMEBYTES) && (arg != NULL))
		{
			*(int *)arg = fp->maxframebytes;
			return 0;
		}
		else if ((request == VIDIOGETSTARTNTIME) && (arg != NULL))
		{
			if ((fp->framentimes == NULL) || (fp->tot_frames == 0))
			{
				vid_errno = EINVAL;
				return -1;
			}
			*(unsigned long long *)arg = fp->framentimes[0];
			return 0;
		}
		else if ((request == VIDIOGETSTOPNTIME) && (arg != NULL))
		{
			if ((fp->framentimes == NULL) || (fp->tot_frames == 0))
			{
				vid_errno = EINVAL;
				return -1;
			}
			*(unsigned long long *)arg = fp->framentimes[fp->tot_frames - 1];
			return 0;
		}
		else if ((request == VIDIOGETCLOSESTFRAME) && (arg != NULL))
		{
			VIDCLOSESTFRAME *vcf = (VIDCLOSESTFRAME *)arg;
			int i, a, b, mid;

			/* If the frametime we seek is before the first frame, use the first frame */
			if (vcf->ntime < fp->framentimes[0])
			{
				vcf->framenum = 0;
				vcf->ntime = fp->framentimes[0];
				return 0;
			}

			/* If the frametime we seek is psat the last frame, use the last frame */
			if (fp->framentimes[fp->tot_frames - 1] < vcf->ntime)
			{
				vcf->framenum = fp->tot_frames - 1;
				vcf->ntime = fp->framentimes[fp->tot_frames - 1];
				return 0;
			}

			/*
			** If we're given a hint in the vcf->framenum, then use it.  Assume that
			** the frame with the desired time (vcf->ntime) is equal to or a little
			** bit greater than the hint framenum.  This covers the majority of playing
			** situations, where we're not going to skip about randomly, but will
			** advance monotonically.  Thus, we optimize for this case.
			*/
			if ((vcf->framenum >= 0) && (vcf->framenum < fp->tot_frames))
			{
				for (i=vcf->framenum; (i < fp->tot_frames) && (i < vcf->framenum + 10); i++)
				{
					if (fp->framentimes[i] <= vcf->ntime)
					{
						if ((i < fp->tot_frames - 1) && (fp->framentimes[i+1] > vcf->ntime))
						{
							vcf->framenum = i;
							vcf->ntime = fp->framentimes[i];
							return 0;
						}
					}
					else break;
				}
			}

			/*
			** No hint, or hint was not helpful ...  Now use brute force.
			** But use a binary search for speed.
			*/
			a = 0;
			b = fp->tot_frames - 1;
			do {
				mid = (a + b) / 2;
				if ((fp->framentimes[a] <= vcf->ntime) && (vcf->ntime < fp->framentimes[mid]))
				{
					b = mid;
				}
				if ((fp->framentimes[mid] <= vcf->ntime) && (vcf->ntime < fp->framentimes[b]))
				{
					a = mid;
				}
			} while (b - a > 1);
			vcf->framenum = a;
			vcf->ntime = fp->framentimes[a];
			return 0;
		}
		else if ((request == VIDIOGETCAPS) && (arg != NULL))
		{
			*(int *)arg = 0;
			return 0;
		}
	}
	vid_errno = EINVAL;
	return -1;
}

int vidavi_stat(VIDFILE *fp, struct vidstat *buf)
{
	struct stat statbuf;

	if (fp->flags == O_RDONLY)
	{
		if (stat(fp->name, &statbuf) == -1)
		{
			vid_errno = errno;
			return -1;
		}
		memset(buf, 0, sizeof(struct vidstat));
		buf->entry = fp->entry;
		buf->nentries = fp->nentries;
		buf->tot_frames = fp->tot_frames;
		buf->microsec_per_frame = fp->microsec_per_frame;
		buf->video_width = fp->video_width;
		buf->video_height = fp->video_height;
		buf->n_components = fp->n_components;
		buf->maxframebytes = fp->maxframebytes;
		buf->startntime = fp->framentimes[0];
		buf->stopntime = fp->framentimes[fp->tot_frames - 1];
		buf->capabilities = 0;
		return 0;
	}
	vid_errno = EINVAL;
	return -1;
}

int vidavi_write(VIDFILE *fp, short *buf, int nsamples)
{
	vid_errno = EINVAL;
	return -1;
}


/*
** LOCAL FUNCTIONS
*/
static void *alloc_vidextra(int width, int height, int n_components)
{
	VIDEXTRA *vidextra = NULL;

	vidextra = (VIDEXTRA *)calloc(1, sizeof(VIDEXTRA));
	vidextra->jpeg_line = (char *)malloc(3 * width);
	vidextra->cjpeg.err = jpeg_std_error(&vidextra->jerr);
	jpeg_create_decompress(&vidextra->cjpeg);
	vidextra->cjpeg.image_width = width;
	vidextra->cjpeg.image_height = height;

	vidextra->cjpeg.dct_method = JDCT_FASTEST;
	vidextra->jpeg_inbuffer_size = width * height * 3;
	vidextra->jpeg_inbuffer = (unsigned char *)malloc(vidextra->jpeg_inbuffer_size);
	jpeg_mem_src(&vidextra->cjpeg, vidextra->jpeg_inbuffer, vidextra->jpeg_inbuffer_size);

	return (void *)vidextra;
}

static void free_vidextra(void *extra)
{
	VIDEXTRA *vidextra = (VIDEXTRA *)extra;

	if (vidextra)
	{
		jpeg_destroy_decompress(&vidextra->cjpeg);
		free(vidextra->jpeg_inbuffer);
		free(vidextra->jpeg_line);
		free(vidextra);
	}
	return;
}

