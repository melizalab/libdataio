
/* Copyright (C) 2004 by Amish S. Dave (adave3@uic.edu) */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "vidio.h"

int main(int ac, char *av[])
{
	VIDFILE *fp;
	char *framedata = NULL;
	unsigned long long framentime = 0LL;
	int nframes, width, height, ncomps, microsecsperframe;

	if (ac < 2) {
		printf("Usage: %s <infile>\n\n", av[0]);
		return -1;
	}

	/*
	** Open the input file
	*/
	if ((fp = vid_open(av[1], "r")) == NULL)
	{
		fprintf(stderr, "Error opening file '%s'\n", av[1]);
		return -1;
	}

	vid_ctl(fp, VIDIOGETNFRAMES, &nframes);
	vid_ctl(fp, VIDIOGETMICROSECPERFRAME, &microsecsperframe);
	vid_ctl(fp, VIDIOGETWIDTH, &width);
	vid_ctl(fp, VIDIOGETHEIGHT, &height);
	vid_ctl(fp, VIDIOGETNCOMPONENTS, &ncomps);
	printf("nframes = %d\n", nframes);
	printf("microsecsperframe = %d\n", microsecsperframe);
	printf("width = %d\n", width);
	printf("height = %d\n", height);
	printf("ncomps = %d\n", ncomps);

	framedata = (char *)malloc(width * height * ncomps);
	if (vid_read(fp, 0, framedata, &framentime) != 0)
	{
		fprintf(stderr, "Error reading frame #0 for file '%s'\n", av[1]);
		return -1;
	}
	printf("read frame #0 successfully\n");
	vid_close(fp);
	return 0;
}

