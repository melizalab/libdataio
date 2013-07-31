
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include "lblio.h"

int main(int ac, char *av[])
{
	LBLFILE *fp, *outfp;
	float *lbltimes;
	char **lblnames;
	int i, lblcount;

	if (ac < 3) {
		printf("Usage: %s <infile> <outfile>\n\n", av[0]);
		return -1;
	}

	/*
	** Open the input file
	*/
	if ((fp = lbl_open(av[1], "r")) == NULL)
	{
		fprintf(stderr, "Error opening file '%s'\n", av[1]);
		return -1;
	}

	if (lbl_read(fp, &lbltimes, &lblnames, &lblcount) == -1)
	{
		fprintf(stderr, "Error reading from file '%s'\n", av[1]);
		lbl_close(fp);
		return -1;
	}

	printf("read %d labels\n", lblcount);
	for (i=0; i < lblcount; i++)
		printf("%8.5f  %s\n", lbltimes[i], lblnames[i]);

	if ((outfp = lbl_open(av[2], "w")) == NULL)
	{
		fprintf(stderr, "Error opening file '%s'\n", av[2]);
		return -1;
	}

	if (lbl_write(outfp, lbltimes, lblnames, lblcount) == -1)
	{
		fprintf(stderr, "Error writing to file '%s'\n", av[2]);
		lbl_close(outfp);
		return -1;
	}
	lbl_close(outfp);

	lbl_close(fp);
	return 0;
}

