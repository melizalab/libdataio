
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "lblio.h"

int lbl_errno;

typedef struct
{
	int (*recognizer)(LBLFILE *);
	int (*open)(LBLFILE *);
} LBLTYPE;

LBLTYPE lbl_types[] =
{
	{lblesps_recognizer, lblesps_open},
	{lbltoe_recognizer, lbltoe_open},
	{NULL, NULL}
};

LBLFILE *lbl_open(char *filename, char *type)
{
	LBLFILE *newfp;
	int typefound, i;

	/*
	** Validate the arguments
	*/
	if ((type == NULL) || ((type[0] != 'r') && (type[0] != 'w')))
	{
		lbl_errno = EINVAL;
		return NULL;
	}
	if ((type[0] == 'r') && (access(filename, R_OK|F_OK) == -1))
	{
		lbl_errno = errno;
		return NULL;
	}

	/*
	** Allocate the LBLFILE structure that will be returned on success.
	** Initialize some fields
	*/
	if ((newfp = (LBLFILE *)malloc(sizeof(LBLFILE))) == NULL)
	{
		lbl_errno = ENOSPC;
		return NULL;
	}
	if ((newfp->name = (char *)malloc(strlen(filename) + 1)) == NULL)
	{
		lbl_errno = ENOSPC;
		free(newfp);
		return NULL;
	}
	strcpy(newfp->name, filename);
	if (type[0] == 'w')
		newfp->flags = O_WRONLY;
	else
		newfp->flags = O_RDONLY;
	newfp->close = NULL;
	newfp->read = NULL;
	newfp->write = NULL;
	newfp->seek = NULL;
	newfp->ctl = NULL;
	newfp->stat = NULL;
	newfp->entry = 1;

	/*
	** Identify the TYPE of lbl file
	*/
	typefound = 0;
	i=0;
	while (lbl_types[i].recognizer != NULL)
	{
		typefound = (*(lbl_types[i].recognizer))(newfp);
		if (typefound != -1)
			break;
		i++;
	}
	if (typefound == -1)
	{
		lbl_errno = EINVAL;
		free(newfp->name);
		free(newfp);
		return NULL;
	}
	newfp->open = lbl_types[i].open;

	/*
	** Call the type-specific open function
	*/
	if ((*(newfp->open))(newfp) == -1)
	{
		free(newfp->name);
		free(newfp);
		return NULL;
	}
	return newfp;
}

void lbl_close(LBLFILE *fp)
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

int lbl_read(LBLFILE *fp, float **lbltimes_p, char ***lblnames_p, int *lblcount_p)
{
	/*
	** Validate arguments
	*/
	if ((lbltimes_p == NULL) || (lblnames_p == NULL) || (lblcount_p == NULL))
	{
		lbl_errno = EINVAL;
		return -1;
	}

	/*
	** Call the type-specific read function
	*/
	if (fp->read != NULL)
		return (*(fp->read))(fp, lbltimes_p, lblnames_p, lblcount_p);
	lbl_errno = EINVAL;
	return -1;
}

int lbl_seek(LBLFILE *fp, int entry)
{
	if (fp->seek != NULL)
		return (*(fp->seek))(fp, entry);
	lbl_errno = EINVAL;
	return -1;
}

int lbl_ctl(LBLFILE *fp, int request, void *arg)
{
	if (fp->ctl != NULL)
		return (*(fp->ctl))(fp, request, arg);
	lbl_errno = EINVAL;
	return -1;
}

int lbl_stat(LBLFILE *fp, struct lblstat *buf)
{
	if (fp->stat != NULL)
		return (*(fp->stat))(fp, buf);
	lbl_errno = EINVAL;
	return -1;
}

int lbl_write(LBLFILE *fp, float *lbltimes, char **lblnames, int lblcount)
{
	if (fp->write != NULL)
		return (*(fp->write))(fp, lbltimes, lblnames, lblcount);
	lbl_errno = EINVAL;
	return -1;
}

