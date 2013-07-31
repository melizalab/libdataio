
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "pcmio.h"

int main(int ac, char *av[])
{
	PCMFILE *fp, *outfp;
	struct pcmstat fp_stat;
	short *bufptr;
	int nsamples, entry;
	long ltime;

	if (ac < 3)
	{
		fprintf(stderr, "Usage: %s <inpfile> <outfile>\n", av[0]);
		fprintf(stderr, "  copies from inpfile to outfile, changing file format if necessary\n");
		exit(-1);
	}

	/*
	** Open the input file
	*/
	if ((fp = pcm_open(av[1], "r")) == NULL)
	{
		fprintf(stderr, "Error opening file '%s'\n", av[1]);
		return -1;
	}

	/*
	** Open the output file
	*/
	if ((outfp = pcm_open(av[2], "w")) == NULL)
	{
		fprintf(stderr, "Error creating file '%s'\n", av[2]);
		return -1;
	}

	entry = 1;
	for(;;)
	{
		if (pcm_seek(fp, entry) == -1)
		{
			fprintf(stderr, "Seek failed on entry %d\n", entry);
			break;
		}
		pcm_stat(fp, &fp_stat);
		if (pcm_read(fp, &bufptr, &nsamples) == -1)
		{
			fprintf(stderr, "Error reading from file '%s', entry %d\n", av[1], entry);
			break;
		}
		if (pcm_ctl(fp, PCMIOGETTIME, &ltime) != -1)
		{
			fprintf(stderr, "TIMESTAMP: %lu\n", ltime);
		}
		fprintf(stderr, "read %d samples from entry %d\n", nsamples, entry);
		if (pcm_seek(outfp, entry) == -1)
			fprintf(stderr, "warning: seek failed on entry %d of output file - APPENDING\n", entry);
		if (pcm_write(outfp, bufptr, nsamples) == -1)
		{
			fprintf(stderr, "Error writing to file '%s'\n", av[2]);
			break;
		}
		fprintf(stderr, "read %d samples into %lx\n", nsamples, (unsigned long)bufptr);
		fprintf(stderr, "1st sample = %d\n", bufptr[0]);
		fprintf(stderr, "2nd sample = %d\n", bufptr[1]);
		entry++;
	}

	pcm_close(fp);
	pcm_close(outfp);
	return 0;
}

