#include <jpeglib.h>

#if JPEG_LIB_VERSION < 80
//extern void jpeg_mem_dest(j_compress_ptr cinfo, JOCTET *outbuffer, int outbuffer_size, int *outbuffer_nbytes_p);
//extern void jpeg_mem_src(j_decompress_ptr cinfo, JOCTET *inbuffer, int inbuffer_size);
#endif
