
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "toeio.h"

/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int toelis_recognizer(TOEFILE *fp)
{
	if (strncasecmp(fp->name + strlen(fp->name) - 8, ".toe_lis", 8))
		return -1;
	return 0;
}

int toelis_open(TOEFILE *fp)
{
	int nunits;

	fp->len = 0;
	fp->addr = NULL;
	fp->close = toelis_close;
	fp->read = toelis_read;
	fp->write = toelis_write;
	fp->seek = toelis_seek;
	fp->ctl = toelis_ctl;
	fp->stat = toelis_stat;
	fp->entry = 1;
	fp->fd = -1;
	fp->toedata = NULL;
	fp->toe_repcounts = NULL;
	fp->toe_repptrs = NULL;
	fp->toe_numunits = 0;
	fp->toe_numreps = 0;
	fp->toe_unit = 1;
	if (fp->flags == O_RDONLY)
	{
		if (toelis_ctl(fp, TOEIOGETNUNITS, &nunits) == -1)
		{
			toe_errno = EINVAL;
			return -1;
		}
		fp->toe_numunits = nunits;
	}
	if (fp->flags == O_WRONLY)
	{
		toe_errno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

void toelis_close(TOEFILE *fp)
{
	if (fp->flags == O_RDONLY)
	{
		if (fp->toedata != NULL) free(fp->toedata);
		if (fp->toe_repcounts != NULL) free(fp->toe_repcounts);
		if (fp->toe_repptrs != NULL) free(fp->toe_repptrs);
	}
	if (fp->flags == O_WRONLY)
	{
	}
	return;
}

#define readint(tfp, buf, bufsize, intptr) ((fgets((buf), (bufsize), (tfp)) != NULL) && (sscanf((buf), "%d", (intptr)) == 1))
#define readflt(tfp, buf, bufsize, fltptr) ((fgets((buf), (bufsize), (tfp)) != NULL) && (sscanf((buf), "%f", (fltptr)) == 1))
#define skiplines(tfp, buf, bufsize, nlines) { int i; for (i=0; i < (nlines); i++) fgets((buf), (bufsize), (tfp)); }

int toelis_read(TOEFILE *fp, float ***toe_repptrs_p, int **toe_repcounts_p, int *toe_numunits_p, int *toe_numreps_p)
{
	FILE *tfp;
	int i;
	char buf[80];
	float *data, *toedata, **toe_repptrs;
	int *toe_repcounts, unitcount, unitline;
	int unit, nunits, nreps;

	unit = fp->entry;
	*toe_repptrs_p = NULL;
	*toe_repcounts_p = NULL;
	*toe_numunits_p = 0;
	*toe_numreps_p = 0;
	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->toedata != NULL) free(fp->toedata);
		if (fp->toe_repptrs != NULL) free(fp->toe_repptrs);
		if (fp->toe_repcounts != NULL) free(fp->toe_repcounts);

		if ((tfp = fopen(fp->name, "r")) != NULL)
		{
			if (readint(tfp, buf, sizeof(buf), &nunits))
			{
				if ((unit > 0) && (unit <= nunits))
				{
					if (readint(tfp, buf, sizeof(buf), &nreps))
					{
						if ((toe_repcounts = (int *)calloc(1 + nreps, sizeof(int))) != NULL)
						{
							skiplines(tfp, buf, sizeof(buf), unit - 1);
							if (readint(tfp, buf, sizeof(buf), &unitline)) {};
							rewind(tfp);
							skiplines(tfp, buf, sizeof(buf), unitline - 1);
							for (i=0; i < nreps; i++)
								if (!readint(tfp, buf, sizeof(buf), &(toe_repcounts[i+1])))
									break;
							if (i == nreps)
							{
								unitcount = 0;
								for (i=0; i < nreps; i++)
									unitcount += toe_repcounts[i+1];
								if ((toedata = (float *)calloc(1 + unitcount, sizeof(float))) != NULL)
								{
									for (i=0; i < unitcount; i++)
										if (!readflt(tfp, buf, sizeof(buf), &(toedata[i])))
											break;
									if (i == unitcount)
									{
										if ((toe_repptrs = (float **)calloc(1 + nreps, sizeof(float *))) != NULL)
										{
											data = toedata;
											for (i=0; i < nreps; i++)
											{
												toe_repptrs[i+1] = data;
												data += toe_repcounts[i+1];
											}
											fp->toedata = toedata;
											fp->toe_numunits = nunits;
											fp->toe_numreps = nreps;
											fp->toe_unit = unit;
											fp->toe_repptrs = toe_repptrs;
											fp->toe_repcounts = toe_repcounts;
											*toe_repptrs_p = toe_repptrs;
											*toe_repcounts_p = toe_repcounts;
											*toe_numunits_p = nunits;
											*toe_numreps_p = nreps;
											fclose(tfp);
											return 0;
										}
									}
									free(toedata);
								}
							}
							free(toe_repcounts);
						}
					}
				}
			}
			fclose(tfp);
			tfp = NULL;
		}
	}
	toe_errno = EOPNOTSUPP;
	return -1;
}

int toelis_seek(TOEFILE *fp, int entry)
{
	int nunits;

	if (fp->flags == O_RDONLY)
	{
		if (toelis_ctl(fp, TOEIOGETNUNITS, &nunits) == 0)
		{
			if ((entry > 0) && (entry <= nunits))
			{
				fp->entry = entry;
				return 0;
			}
		}
	}
	toe_errno = EINVAL;
	return -1;
}

int toelis_ctl(TOEFILE *fp, int request, void *arg)
{
	struct toestat buf;

	if (fp->flags == O_RDONLY)
	{
		if ((request == TOEIOGETNUNITS) && (arg != NULL))
		{
			if (toelis_stat(fp, &buf) == 0)
			{
				*(int *)arg = buf.nunits;
				return 0;
			}
		}
		if ((request == TOEIOGETNREPS) && (arg != NULL))
		{
			if (toelis_stat(fp, &buf) == 0)
			{
				*(int *)arg = buf.nreps;
				return 0;
			}
		}
		if ((request == TOEIOGETUNIT) && (arg != NULL))
		{
			*(int *)arg = fp->entry;
			return 0;
		}
	}
	toe_errno = EOPNOTSUPP;
	return -1;
}

int toelis_stat(TOEFILE *fp, struct toestat *buf)
{
	FILE *tfp;
	char tempbuf[80];
	int nunits, nreps;

	if (fp->flags == O_RDONLY)
	{
		if ((tfp = fopen(fp->name, "r")) != NULL)
		{
			if ((readint(tfp, tempbuf, sizeof(tempbuf), &nunits)) && readint(tfp, tempbuf, sizeof(tempbuf), &nreps))
			{
				buf->nunits = nunits;
				buf->nreps = nreps;
				fclose(tfp);
				return 0;
			}
			fclose(tfp);
		}
	}
	toe_errno = EOPNOTSUPP;
	return -1;
}

int toelis_write(TOEFILE *fp, float **toe_repptrs, int *toe_repcounts, int toe_numunits, int toe_numreps)
{
	toe_errno = EOPNOTSUPP;
	return -1;
}

