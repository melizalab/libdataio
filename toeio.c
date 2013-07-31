
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "toeio.h"

int toe_errno;

typedef struct
{
	int (*recognizer)(TOEFILE *);
	int (*open)(TOEFILE *);
} TOETYPE;

TOETYPE toe_types[] =
{
	{toelis_recognizer, toelis_open},
	{NULL, NULL}
};

TOEFILE *toe_open(char *filename, char *type)
{
	TOEFILE *newfp;
	int typefound, i;

	/*
	** Validate the arguments
	*/
	if ((type == NULL) || ((type[0] != 'r') && (type[0] != 'w')))
	{
		toe_errno = EINVAL;
		return NULL;
	}
	if ((type[0] == 'r') && (access(filename, R_OK|F_OK) == -1))
	{
		toe_errno = errno;
		return NULL;
	}

	/*
	** Allocate the TOEFILE structure that will be returned on success.
	** Initialize some fields
	*/
	if ((newfp = (TOEFILE *)malloc(sizeof(TOEFILE))) == NULL)
	{
		toe_errno = ENOSPC;
		return NULL;
	}
	if ((newfp->name = (char *)malloc(strlen(filename) + 1)) == NULL)
	{
		toe_errno = ENOSPC;
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
	** Identify the TYPE of toe file
	*/
	typefound = 0;
	i=0;
	while (toe_types[i].recognizer != NULL)
	{
		typefound = (*(toe_types[i].recognizer))(newfp);
		if (typefound != -1)
			break;
		i++;
	}
	if (typefound == -1)
	{
		toe_errno = EINVAL;
		free(newfp->name);
		free(newfp);
		return NULL;
	}
	newfp->open = toe_types[i].open;

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

void toe_close(TOEFILE *fp)
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

int toe_read(TOEFILE *fp, float ***toe_repptrs_p, int **toe_repcounts_p, int *toe_numunits_p, int *toe_numreps_p)
{
	/*
	** Validate arguments
	*/
	if ((toe_repptrs_p == NULL) || (toe_repcounts_p == NULL) || (toe_numunits_p == NULL) || (toe_numreps_p == NULL))
	{
		toe_errno = EINVAL;
		return -1;
	}

	/*
	** Call the type-specific read function
	*/
	if (fp->read != NULL)
		return (*(fp->read))(fp, toe_repptrs_p, toe_repcounts_p, toe_numunits_p, toe_numreps_p);
	toe_errno = EINVAL;
	return -1;
}

int toe_seek(TOEFILE *fp, int entry)
{
	if (fp->seek != NULL)
		return (*(fp->seek))(fp, entry);
	toe_errno = EINVAL;
	return -1;
}

int toe_ctl(TOEFILE *fp, int request, void *arg)
{
	if (fp->ctl != NULL)
		return (*(fp->ctl))(fp, request, arg);
	toe_errno = EINVAL;
	return -1;
}

int toe_stat(TOEFILE *fp, struct toestat *buf)
{
	if (fp->stat != NULL)
		return (*(fp->stat))(fp, buf);
	toe_errno = EINVAL;
	return -1;
}

int toe_write(TOEFILE *fp, float **toe_repptrs, int *toe_repcounts, int toe_numunits, int toe_numreps)
{
	if (fp->write != NULL)
		return (*(fp->write))(fp, toe_repptrs, toe_repcounts, toe_numunits, toe_numreps);
	toe_errno = EINVAL;
	return -1;
}

