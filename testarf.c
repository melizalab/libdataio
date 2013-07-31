
#include <sys/time.h>
#include "arf_support.h"
#include "pcmio.h"

int main(int argc, char* argv[])
{
	int entry_count, entry;
	int len, sample_rate;
	short *buf;
	hid_t hd_file;
	char* fname = argv[1];
	struct timeval time;
        PCMFILE *fp;

	if (argc < 2) {
		printf("Usage: %s <arffile>\n\n", argv[0]);
		return -1;
	}

	hd_file = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
	hdf5_scan_entries(hd_file, &entry_count);
	printf("Entry count = %d\n", entry_count);
	/* for(entry=0; entry < entry_count; entry++) */
	/* { */
	/* 	hdf5_read_entry(hd_file, entry, &len, &sample_rate, &time.tv_sec, &time.tv_usec, &buf); */
	/* 	printf("timestamp: %s", ctime(&time.tv_sec)); */
	/* } */
	H5Fclose(hd_file);

        fp = pcm_open(fname,"r");
        pcm_close(fp);
}
