
int pcmseq2_write_hdr(PCMFILE *fp);
int pcmseq2_write_2048(PCMFILE *fp, short *data, int lastsegment);
int pcmseq2_write_data(PCMFILE *fp, short *data, int nsamples, int lastsegment);

