
#include <hdf5.h>

int hdf5_scan_entries(hid_t hd_file, int* counter);
int hdf5_read_entry(hid_t hd_file, int entry_number, int* len, int* sample_rate,
		    int* timestamp, int* microtimestamp,
		    void* addr);
