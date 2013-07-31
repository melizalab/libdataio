
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

/*
** Return 0 if the file is the file is of this type; return -1 otherwise
*/
int lblesps_recognizer(LBLFILE *fp)
{
	if ((strncasecmp(fp->name + strlen(fp->name) - 4, ".lbl", 4)) &&
		(strncasecmp(fp->name + strlen(fp->name) - 7, ".labels", 7)))
	{
		return -1;
	}
	return 0;
}

int lblesps_open(LBLFILE *fp)
{
	fp->len = 0;
	fp->addr = NULL;
	fp->close = lblesps_close;
	fp->read = lblesps_read;
	fp->write = lblesps_write;
	fp->seek = lblesps_seek;
	fp->ctl = lblesps_ctl;
	fp->stat = lblesps_stat;
	fp->entry = 1;
	fp->fd = -1;
	fp->lbltimes = NULL;
	fp->lblnames = NULL;
	fp->lblcount = 0;
	fp->outfp = NULL;
	return 0;
}

void lblesps_close(LBLFILE *fp)
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
		if (fp->outfp != NULL) fclose(fp->outfp);
	}
	return;
}

int lblesps_read(LBLFILE *fp, float **lbltimes_p, char ***lblnames_p, int *lblcount_p)
{
	int i, j, store, fd, len, count;
	struct stat statbuf;
	void *addr;
	char *buf;
	float time, *lbltimes = NULL;
	char **lblnames = NULL;
	int lblcount = 0;

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

		if ((fd = open(fp->name, O_RDONLY)) != -1)
		{
			if (fstat(fd, &statbuf) != -1)
			{
				len = statbuf.st_size;
				if ((addr = (void *)malloc(len)) != NULL)
				{
					if (read(fd, addr, len) != -1)
					{
						/* Skip past the header */
						buf = (char *)addr;
						for (i=0; i < len; i++)
						{
							if (buf[i] == '#')
							{
								while ((buf[i] == '\n') || (buf[i] == '\r') || (buf[i] == '#')) i++;
								lblcount = 0;
								for (j=i; j < len; j++)
								{
									if (buf[j] == '\n') lblcount++;
									if ((buf[j] == '\n') || (buf[j] == '\r')) buf[j] = '\0';
								}
								if (lblcount > 0)
								{
									lbltimes = (float *)malloc(lblcount * sizeof(float));
									lblnames = (char **)malloc(lblcount * sizeof(char *));
									count = 0;
									j = i;
									while ((j < len) && (sscanf(buf+j, "%f", &time) == 1))
									{
										while ((j < len) && (buf[j] != '\0')) j++;
										store = j--;
										while ((buf[j] != ' ') && (buf[j] != '\t')) j--;
										lblnames[count] = buf + j + 1;
										lbltimes[count] = time * 1000.0;
										count++;
										j = store + 1;
										while ((j < len) && (buf[j] == '\0')) j++;
									}
									if (j < len)
									{
										free(lbltimes);
										free(lblnames);
									}
								}
								if ((lblcount == 0) || (j == len))
								{
									fp->fd = -1;
									fp->addr = addr;
									fp->len = len;
									fp->lbltimes = *lbltimes_p = lbltimes;
									fp->lblnames = *lblnames_p = lblnames;
									fp->lblcount = *lblcount_p = lblcount;
									close(fd);
									return 0;
								}
							}
						}
						lbl_errno = EINVAL;
					}
					else lbl_errno = errno;
					free(addr);
				}
				else lbl_errno = EINVAL;
			}
			else lbl_errno = errno;
			close(fd);
		}
		else lbl_errno = errno;
		return -1;
	}
	lbl_errno = EOPNOTSUPP;
	return -1;
}

int lblesps_seek(LBLFILE *fp, int entry)
{
	if (entry != 1)
	{
		lbl_errno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

int lblesps_ctl(LBLFILE *fp, int request, void *arg)
{
	lbl_errno = EOPNOTSUPP;
	return -1;
}

int lblesps_stat(LBLFILE *fp, struct lblstat *buf)
{
	lbl_errno = EOPNOTSUPP;
	return -1;
}

int lblesps_write(LBLFILE *fp, float *lbltimes, char **lblnames, int lblcount)
{
	int i;

	if (fp->flags == O_WRONLY)
	{
		if ((fp->outfp = fopen(fp->name, "w")) != NULL)
		{
			fprintf(fp->outfp, "signal feasd\n");
			fprintf(fp->outfp, "type 0\n");
			fprintf(fp->outfp, "color 121\n");
			fprintf(fp->outfp, "font *-fixed-bold-*-*-*-15-*-*-*-*-*-*-*\n");
			fprintf(fp->outfp, "separator ;\n");
			fprintf(fp->outfp, "nfields 1\n");
			fprintf(fp->outfp, "#\n");
			for (i=0; i < lblcount; i++)
				fprintf(fp->outfp, "%12.6f  121 %s\n", lbltimes[i] / 1000.0, lblnames[i]);
			fclose(fp->outfp);
			fp->outfp = NULL;
			return 0;
		}
	}
	lbl_errno = EINVAL;
	return -1;
}

