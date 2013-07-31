
/* Copyright (C) 2001 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include "toeio.h"

int main(int ac, char *av[])
{
	TOEFILE *fp;
	int i, j;
	float **repptrs, *data;
	int *repcounts, nunits, nreps;

	if (ac < 2) {
		printf("Usage: %s <infile>\n\n", av[0]);
		return -1;
	}

	/*
	** Open the input file
	*/
	if ((fp = toe_open(av[1], "r")) == NULL)
	{
		fprintf(stderr, "Error opening file '%s'\n", av[1]);
		return -1;
	}

	if (toe_ctl(fp, TOEIOGETNUNITS, &nunits) == -1)
	{
		fprintf(stderr, "Error getting nunits from file '%s'\n", av[1]);
		toe_close(fp);
		return -1;
	}

	for (i = 1; i <= nunits; i++)
	{
		if (toe_seek(fp, i) == -1)
		{
			fprintf(stderr, "Error seeking to unit %d\n", i);
			toe_close(fp);
			return -1;
		}

		if (toe_read(fp, &repptrs, &repcounts, &nunits, &nreps) == -1)
		{
			fprintf(stderr, "Error reading from file '%s'\n", av[1]);
			toe_close(fp);
			return -1;
		}

		printf("UNIT: %d, units: %d, reps: %d\n", i, nunits, nreps);
		for (j=1; j <= nreps; j++)
			printf("%d spikes in rep %d\n", repcounts[j], j);
		data = repptrs[1];
		for (j=0; j < repcounts[1]; j++)
			printf("%f\n", data[j]);
	}

	toe_close(fp);
	return 0;
}

