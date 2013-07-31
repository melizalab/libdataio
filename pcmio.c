
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pcmio.h"

int pcm_errno;

typedef struct
{
	int (*recognizer)(PCMFILE *);
	int (*open)(PCMFILE *);
} PCMTYPE;

PCMTYPE pcm_types[] =
{
	{pcmfix_recognizer, pcmfix_open},
	{pcmseq_recognizer, pcmseq_open},
#ifdef USING_ESPS
	{pcmfeasd_recognizer, pcmfeasd_open},
#endif
	{expidx_recognizer, expidx_open},
	{pcmwav_recognizer, pcmwav_open},
	{pcmraw_recognizer, pcmraw_open},
#ifdef HAVE_MP3
	{pcmmp3_recognizer, pcmmp3_open},
#endif
	{arfhdf5_recognizer, arfhdf5_open},
	{NULL, NULL}
};

PCMFILE *pcm_open(char *filename, char *type)
{
	PCMFILE *newfp;
	int typefound, i;

	/*
	** Validate the arguments
	*/
	if ((type == NULL) || ((type[0] != 'r') && (type[0] != 'w')))
	{
		errno = EINVAL;
		return NULL;
	}
	if ((type[0] == 'r') && (access(filename, R_OK|F_OK) == -1))
		return NULL;

	pcm_errno = 0;
	if ((newfp = (PCMFILE *)malloc(sizeof(PCMFILE))) != NULL)
	{
		if ((newfp->name = (char *)malloc(strlen(filename) + 1)) != NULL)
		{
			strcpy(newfp->name, filename);
			newfp->flags = (type[0] == 'w') ? (O_WRONLY) : (O_RDONLY);
			newfp->close = NULL;
			newfp->read = NULL;
			newfp->write = NULL;
			newfp->seek = NULL;
			newfp->ctl = NULL;
			newfp->stat = NULL;
			newfp->p2file = NULL;
			newfp->tempnam = NULL;
			newfp->entry = 1;
#ifdef USING_ESPS
			newfp->header = NULL;
			newfp->espsfp = NULL;
#endif
			/*
			** Identify the TYPE of pcm file
			*/
			typefound = 0;
			for (i=0; pcm_types[i].recognizer != NULL; i++)
				if ((typefound = (*(pcm_types[i].recognizer))(newfp)) != -1)
					break;
			if (typefound != -1)
			{
				/*
				** Call the type-specific open function
				*/
				newfp->open = pcm_types[i].open;
				if ((*(newfp->open))(newfp) != -1)
					return newfp;
			}
			else pcm_errno = EINVAL;
			free(newfp->name);
		}
		else pcm_errno = ENOMEM;
		free(newfp);
	}
	else pcm_errno = ENOMEM;
	errno = pcm_errno;
	return NULL;
}

void pcm_close(PCMFILE *fp)
{
	/*
	** Call the type-specific close function
	*/
	if (fp->close != NULL)
		(*(fp->close))(fp);

	/*
	** Free the storage allocated by the generic open function
	*/
	free(fp->name);
	free(fp);
}

int pcm_read(PCMFILE *fp, short **buf_p, int *nsamples_p)
{
	/*
	** Validate arguments
	*/
	if ((buf_p != NULL) && (nsamples_p != NULL))
	{
		if (fp->read != NULL)
		{
			/*
			** Call the type-specific read function
			*/
			return (*(fp->read))(fp, buf_p, nsamples_p);
		}
	}
	errno = EINVAL;
	return -1;
}

int pcm_seek(PCMFILE *fp, int entry)
{
	/*
	** Call the type-specific seek function
	*/
	if (fp->seek != NULL)
		return (*(fp->seek))(fp, entry);
	errno = EINVAL;
	return -1;
}

/*
** request types:
**  PCMIOMALLOC
**  PCMIOMMAP
** 		Use to set preference for MALLOC or MMAP allocation methods...
**		The argument is ignored.
*/
int pcm_ctl(PCMFILE *fp, int request, void *arg)
{
	if (fp->ctl != NULL)
		return (*(fp->ctl))(fp, request, arg);
	errno = EINVAL;
	return -1;
}

int pcm_stat(PCMFILE *fp, struct pcmstat *buf)
{
	if (fp->stat != NULL)
		return (*(fp->stat))(fp, buf);
	errno = EINVAL;
	return -1;
}

int pcm_write(PCMFILE *fp, short *buf, int nsamples)
{
	if (fp->write != NULL)
		return (*(fp->write))(fp, buf, nsamples);
	errno = EINVAL;
	return -1;
}

