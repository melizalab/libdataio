
/* Entry header */
typedef struct
{
	unsigned short		recordsize;
	unsigned short		controlword;
	char 				*initkey;         /* 28 bytes */
	int					datetime[2];
	unsigned int		segmentsize;
	unsigned int		pcmstart;
	unsigned int		gain;
	unsigned int		samplerate;
} p2header;

/* Segment */
typedef struct
{
	/** First seg **/
	unsigned short		recordsize1;
	unsigned short		controlword1;
	char 				*matchkey;        /* 28 bytes */
	unsigned int		recordwords;
	short				*samples1;        /* 1005 samples */
	/** Second seg **/
	unsigned short		recordsize2;
	unsigned short		controlword2;
	short				*samples2;        /* 1021 samples */
	/** Third seg **/
	unsigned short		recordsize3;
	unsigned short		controlword3;
	short				*samples3;        /* 22 samples */
} p2segment;

#define CACHESIZE 500

typedef struct
{
	FILE *fp;

	int currententry;
	int lastentry;
	int type;
	long entry_pos_cache[CACHESIZE];
	long entry_size_cache[CACHESIZE];
	int entry_sr_cache[CACHESIZE];
	unsigned long long entry_time_cache[CACHESIZE];

	p2header p2hdr;
	p2segment p2seg;

	char hdrbuf[56];
	char segbuf[4140];
} P2FILE;


P2FILE *pcmseq2_open(char *filename);
void pcmseq2_close(P2FILE *p2fp);
int pcmseq2_read(P2FILE *p2fp, int entry, long start, long stop, short *buf, long *numwritten_p, long *entrysize_p);
int pcmseq2_getinfo(P2FILE *p2fp, int entry, long *entrysize_p, int *samplerate_p, unsigned long long *datetime_p, int *nentries_p);
int pcmseq2_seektoentry(P2FILE *p2fp, int entry);

