
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "vidio.h"

int vid_errno;

typedef struct
{
	int (*recognizer)(VIDFILE *);
	int (*open)(VIDFILE *);
} VIDTYPE;

VIDTYPE vid_types[] =
{
	{vidavi_recognizer, vidavi_open},
	{NULL, NULL}
};

VIDFILE *vid_open(char *filename, char *type)
{
	VIDFILE *newfp;
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

	vid_errno = 0;
	if ((newfp = (VIDFILE *)malloc(sizeof(VIDFILE))) != NULL)
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
			newfp->entry = 1;
			newfp->extra = NULL;

			/*
			** Identify the TYPE of vid file
			*/
			typefound = 0;
			for (i=0; vid_types[i].recognizer != NULL; i++)
				if ((typefound = (*(vid_types[i].recognizer))(newfp)) != -1)
					break;
			if (typefound != -1)
			{
				/*
				** Call the type-specific open function
				*/
				newfp->open = vid_types[i].open;
				if ((*(newfp->open))(newfp) != -1)
					return newfp;
			}
			else vid_errno = EINVAL;
			free(newfp->name);
		}
		else vid_errno = ENOMEM;
		free(newfp);
	}
	else vid_errno = ENOMEM;
	errno = vid_errno;
	return NULL;
}

void vid_close(VIDFILE *fp)
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

int vid_read(VIDFILE *fp, int framenum, char *framedata, unsigned long long *framentime)
{
	/*
	** Validate arguments
	*/
	if ((fp != NULL) && (framedata != NULL))
	{
		if (fp->read != NULL)
		{
			/*
			** Call the type-specific read function
			*/
			return (*(fp->read))(fp, framenum, framedata, framentime);
		}
	}
	errno = EINVAL;
	return -1;
}

int vid_seek(VIDFILE *fp, int entry)
{
	/*
	** Call the type-specific seek function
	*/
	if (fp->seek != NULL)
		return (*(fp->seek))(fp, entry);
	errno = EINVAL;
	return -1;
}

int vid_ctl(VIDFILE *fp, int request, void *arg)
{
	if (fp->ctl != NULL)
		return (*(fp->ctl))(fp, request, arg);
	errno = EINVAL;
	return -1;
}

int vid_stat(VIDFILE *fp, struct vidstat *buf)
{
	if (fp->stat != NULL)
		return (*(fp->stat))(fp, buf);
	errno = EINVAL;
	return -1;
}

int vid_write(VIDFILE *fp, short *buf, int nsamples)
{
	if (fp->write != NULL)
		return (*(fp->write))(fp, buf, nsamples);
	errno = EINVAL;
	return -1;
}

