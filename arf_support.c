
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "arf_support.h"

typedef enum {
        DSET_UNKNOWN = 0,
        DSET_SAMPLED = 1,
        DSET_TABLE = 2,
        DSET_EVENT = 3
} dataset_type_t;

const char* CHANNEL_SAMPLE_RATE = "sampling_rate";
const char* DSET_TIMESTAMP = "timestamp";

/**
 * Infer the type of data (sampled, table, event) stored in a dataset
 *
 * @param dset_id   handle for dataset object
 * @return dataset_type_t
 */
static dataset_type_t
hdf5_dataset_type(hid_t dset_id)
{
        hid_t dtype_id;
        H5T_class_t dtype_class;
        dataset_type_t ret = 0;

        // Is datatype simple?
        dtype_id = H5Dget_type(dset_id);
        dtype_class = H5Tget_class(dtype_id);
        if (dtype_class==H5T_COMPOUND)
                ret = DSET_TABLE;
        else if (dtype_class==H5T_VLEN)
                ret = DSET_EVENT;
        else if (dtype_class==H5T_INTEGER || dtype_class==H5T_FLOAT) {
                // does dataset have a sampling_rate attribute?
                if (H5Aexists(dset_id,CHANNEL_SAMPLE_RATE))
                        ret = DSET_SAMPLED;
                else
                        ret = DSET_EVENT;
        }
        // all other datatype classes aren't supported
        H5Tclose(dtype_id);
        return ret;
}

/**
 * Callback function for counting the number of datasets (i.e. "entries") in the file.
 */
static herr_t
hdf5_cb_count_datasets(hid_t id, const char * name, const H5L_info_t *link_info, void *data)
{
        // Does link point to a dataset?
        H5O_info_t object_info;
        H5Oget_info_by_name(id, name, &object_info, H5P_DEFAULT);
        if (object_info.type!=H5O_TYPE_DATASET)
                return 0;

        int *count = (int*)data;
        hid_t dset_id = H5Dopen(id, name, H5P_DEFAULT);
        int dset_type = hdf5_dataset_type(dset_id);
        if (dset_type==DSET_SAMPLED)
                *count += 1;
        H5Dclose(dset_id);
        return 0;
}

typedef struct {
        int current_entry;
        int target_entry;
        double sample_rate;
        int timestamp[2];
        char * name;
        int len;
        void *addr;
} entry_iter_info;

/**
 * Read data from a dataset.  For multicolumn datasets, only the first
 * column is read.  This function allocates the memory it reads into.
 */
static herr_t
read_entry(hid_t dset_id, entry_iter_info * info)
{
        hid_t attribute, file_dataspace, mem_dataspace;
        hsize_t *dims;
        int i, ndim;

        attribute = H5Aopen_name(dset_id, CHANNEL_SAMPLE_RATE);
        H5Aread(attribute, H5T_NATIVE_DOUBLE, &(info->sample_rate));
        H5Aclose(attribute);

        file_dataspace = H5Dget_space(dset_id);
        ndim    = H5Sget_simple_extent_ndims(file_dataspace);
        dims    = (hsize_t*) malloc(sizeof(hsize_t)*ndim);
        H5Sget_simple_extent_dims(file_dataspace, dims, NULL);

        // restrict read dataspace to first column
        if (ndim > 1) {
                // not sure if this is right?  should the blocks be larger?
                hsize_t * start = (hsize_t*)malloc(sizeof(hsize_t)*ndim);
                hsize_t * stride = (hsize_t*)malloc(sizeof(hsize_t)*ndim);
                hsize_t * count = (hsize_t*)malloc(sizeof(hsize_t)*ndim);
                fprintf(stderr,"Warning: only reading first column from dataset\n");
                for (i=0; i < ndim; ++i) {
                        start[i] = 0;
                        stride[i] = 1;
                        count[i] = 1;
                }
                count[0] = dims[0];
                H5Sselect_hyperslab(file_dataspace, H5S_SELECT_SET, start, stride, count, NULL);
                free(start);
                free(stride);
                free(count);
        }
        else
                H5Sselect_all(file_dataspace);

        info->len = H5Sget_select_npoints(file_dataspace);
        // allocate output memory
        short * buf = (short*)malloc(info->len * sizeof(short));
        mem_dataspace = H5Screate_simple(1, dims, NULL);
        // if data are stored as floats they need to be rescaled
        hid_t storage_type = H5Dget_type(dset_id);
        if (H5Tget_class(storage_type)==H5T_FLOAT) {
                float * fbuf = (float*)malloc(info->len * sizeof(float));
                H5Dread(dset_id, H5T_NATIVE_FLOAT, mem_dataspace, file_dataspace, H5P_DEFAULT, fbuf);
                for (i = 0; i < info->len; ++i)
                        buf[i] = (short)(floor(fbuf[i] * 32768));
                free(fbuf);
        }
        else
                H5Dread(dset_id, H5T_STD_I16LE, mem_dataspace, file_dataspace, H5P_DEFAULT, buf);

        info->addr = buf;
        H5Tclose(storage_type);
        H5Sclose(file_dataspace);
        H5Sclose(mem_dataspace);
        return 0;
}

/**
 * Load timestamp information for an entry. This requires
 * back-navigating to the containing group and looking up the
 * timestamp attribute.
 */
static void
entry_timestamp(hid_t id, const char * name, int *timestamp)
{
        // I LOVE working with C strings
        char * slash = strrchr(name,'/');
        char * entry = (char*)calloc(slash-name+1,sizeof(char));
        strncpy(entry,name,slash-name);
        /* printf("entry name: %s\n", entry); */
        hid_t file_id = H5Iget_file_id(id);
        hid_t entry_id = H5Gopen(file_id, entry, H5P_DEFAULT);
        hid_t attr_id = H5Aopen_name(entry_id, DSET_TIMESTAMP);
        H5Aread(attr_id, H5T_NATIVE_INT, timestamp);
        H5Aclose(attr_id);
        H5Gclose(entry_id);
        H5Fclose(file_id);
}

/**
 * Callback function for reading datasets.  This is somewhat kludgy:
 * iterate forward to the requested entry, read the data, then stop
 * the iterator.  Probably reasonable for files that don't have too
 * many entries/channels, and I don't have to store entry names this way.
 */
static herr_t
hdf5_cb_read_dataset(hid_t id, const char * name, const H5L_info_t *link_info, void *data)
{
        int ret=0;

        // Does link point to a dataset?
        H5O_info_t object_info;
        H5Oget_info_by_name(id, name, &object_info, H5P_DEFAULT);
        if (object_info.type!=H5O_TYPE_DATASET)
                return 0;

        hid_t dset_id = H5Dopen(id, name, H5P_DEFAULT);
        int dset_type = hdf5_dataset_type(dset_id);
        if (dset_type==DSET_SAMPLED) {
                // increment counter
                entry_iter_info * info = (entry_iter_info*) data;
                if (info->current_entry>=info->target_entry) {
                        read_entry(dset_id, info);
                        fprintf(stdout, "Read from dataset %s: %d samples @ %3.2f Hz\n",
                                name, info->len, info->sample_rate);
                        entry_timestamp(id, name, info->timestamp);
                        ret = 1;
                }
                info->current_entry += 1;
        }
        H5Dclose(dset_id);
        return ret;

}


/**
 * Get the number of entries in the file.  These aren't entries as ARF
 * defines them, but datasets with sampled data.
 */
void hdf5_scan_entries(hid_t hd_file, int *counter)
{
        *counter = 0;
        H5Lvisit(hd_file, H5_INDEX_CRT_ORDER, H5_ITER_INC, hdf5_cb_count_datasets, counter);
}

/**
 * Read an entry from the file.
 * @param hd_file       identifier for file
 * @param entry_number  the entry to read (0-indexed)
 * @param len           output, number of samples
 * @param sample_rate   output, sampling rate of data (in Hz)
 * @param addr          output sample buffer. Allocates memory.
 */
int hdf5_read_entry(hid_t hd_file, int entry_number, int* len, int* sample_rate,
                    int* timestamp, int* microtimestamp,
                    short** addr)
{
        int ret = 0;
        entry_iter_info info;
        info.current_entry = 0;
        info.target_entry = entry_number;
        ret = H5Lvisit(hd_file, H5_INDEX_CRT_ORDER, H5_ITER_INC, hdf5_cb_read_dataset, &info);
        if (ret > 0) {
                if (len) *len = info.len;
                if (sample_rate) *sample_rate = info.sample_rate;
                if (addr) *addr = info.addr;
                if (timestamp) *timestamp = info.timestamp[0];
                if (microtimestamp) *microtimestamp = info.timestamp[1];
                return 0;
        }
        // iteration ended without reading data
        else
                return -1;
}
