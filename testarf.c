
#include <sys/time.h>
#include "arf_support.h"
#include "pcmio.h"

int main(int argc, char* argv[])
{
        int entry;
        short *buf;
        int nsamples;
        char* fname = argv[1];
        PCMFILE *fp;

        if (argc < 2) {
                printf("Usage: %s <arffile>\n\n", argv[0]);
                return -1;
        }

        struct pcmstat stat;
        fp = pcm_open(fname,"r");
        pcm_stat(fp, &stat);
        printf("Entry count = %d\n", stat.nentries);
        for(entry=0; entry < stat.nentries; entry++)
        {
                pcm_seek(fp, entry);
                pcm_read(fp, &buf, &nsamples);
        }
        pcm_close(fp);
        return 0;
}
