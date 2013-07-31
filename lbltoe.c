
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "lblio.h"
#include "toeio.h"

/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int lbltoe_recognizer(LBLFILE *fp)
{
	if (strncasecmp(fp->name + strlen(fp->name) - 8, ".toe_lis", 8))
		return -1;
	return 0;
}

int lbltoe_open(LBLFILE *fp)
{
	fp->len = 0;
	fp->addr = NULL;
	fp->close = lbltoe_close;
	fp->read = lbltoe_read;
	fp->write = lbltoe_write;
	fp->seek = lbltoe_seek;
	fp->ctl = lbltoe_ctl;
	fp->stat = lbltoe_stat;
	fp->entry = 1;
	fp->fd = -1;
	fp->lbltimes = NULL;
	fp->lblnames = NULL;
	fp->lblcount = 0;
	if (fp->flags == O_WRONLY)
	{
		lbl_errno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

void lbltoe_close(LBLFILE *fp)
{
	if (fp->flags == O_RDONLY)
	{
		if (fp->addr != NULL) free(fp->addr);
		if (fp->lbltimes != NULL) free(fp->lbltimes);
		if (fp->lblnames != NULL) free(fp->lblnames);
		if (fp->fd != -1) close(fp->fd);
	}
	if (fp->flags == O_WRONLY)
	{
		if (fp->fd != -1) close(fp->fd);
	}
	return;
}

static char *unittbl[20] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19"};

int lbltoe_read(LBLFILE *fp, float **lbltimes_p, char ***lblnames_p, int *lblcount_p)
{
	int i, count;
	float *lbltimes;
	char **lblnames;
	TOEFILE *tfp;
	float *data, **repptrs;
	int nunits, nreps, unit, rep, cnt, *repcounts, status;

	*lbltimes_p = NULL;
	*lblnames_p = NULL;
	*lblcount_p = 0;
	if (fp->flags == O_RDONLY)
	{
		/*
		** Clean up from previous calls
		*/
		if (fp->addr != NULL) free(fp->addr);
		if (fp->lbltimes != NULL) free(fp->lbltimes);
		if (fp->lblnames != NULL) free(fp->lblnames);
		fp->lblcount = 0;

		if ((tfp = toe_open(fp->name, "r")) != NULL)
		{
			if ((toe_ctl(tfp, TOEIOGETNUNITS, &nunits)) == 0)
			{
				count = 0;
				status = 0;
				for (unit=1; (status == 0) && (unit <= nunits); unit++)
				{
					if ((toe_seek(tfp, unit) == 0) && (toe_read(tfp, &repptrs, &repcounts, &nunits, &nreps)) == 0)
						for (i=1; i <= nreps; i++) count += repcounts[i];
					else status = -1;
				}
				if (status == 0)
				{
					if ((lbltimes = (float *)malloc(count * sizeof(float))) != NULL)
					{
						if ((lblnames = (char **)malloc(count * sizeof(char *))) != NULL)
						{
							cnt = 0;
							for (unit=1; (status == 0) && (unit <= nunits); unit++)
							{
								if ((toe_seek(tfp, unit) == 0) && (toe_read(tfp, &repptrs, &repcounts, &nunits, &nreps)) == 0)
								{
									for (rep=1; rep <= nreps; rep++)
									{
										data = repptrs[rep];
										for (i=0; i < repcounts[rep]; i++)
										{
											lbltimes[cnt] = data[i];
											lblnames[cnt] = unittbl[unit];
											cnt++;
										}
									}
								}
								else status = -1;
							}
							if (status == 0)
							{
								fp->fd = -1;
								fp->addr = NULL;
								fp->len = 0;
								fp->lbltimes = *lbltimes_p = lbltimes;
								fp->lblnames = *lblnames_p = lblnames;
								fp->lblcount = *lblcount_p = cnt;
								toe_close(tfp);
								return 0;
							}
						}
						free(lbltimes);
					}
				}
				if (status == -1) lbl_errno = toe_errno;
			}
			else lbl_errno = toe_errno;
			toe_close(tfp);
		}
		else lbl_errno = toe_errno;
		return -1;
	}
	lbl_errno = EOPNOTSUPP;
	return -1;
}

int lbltoe_seek(LBLFILE *fp, int entry)
{
	if (entry != 1)
	{
		lbl_errno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

int lbltoe_ctl(LBLFILE *fp, int request, void *arg)
{
	lbl_errno = EOPNOTSUPP;
	return -1;
}

int lbltoe_stat(LBLFILE *fp, struct lblstat *buf)
{
	lbl_errno = EOPNOTSUPP;
	return -1;
}

int lbltoe_write(LBLFILE *fp, float *lbltimes, char **lblnames, int lblcount)
{
	lbl_errno = EOPNOTSUPP;
	return -1;
}

