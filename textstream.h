
#ifndef _TEXT_STREAM_H_
#define _TEXT_STREAM_H_

#include "datastream.h"
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <string>

using namespace std;

class TextStream : public DataStream {
private:
    FILE* file;
    gzFile zfile;
    int frame_counter;

    void write_raw(string data, bool uncompressed) {
        if(compress_data) {
            if(uncompressed) {
                gzsetparams(zfile, 0, Z_DEFAULT_STRATEGY);
                gzwrite(zfile, data.c_str(), data.length());
                gzflush(zfile, Z_SYNC_FLUSH);
                gzsetparams(zfile, compression_level, Z_DEFAULT_STRATEGY);
            }
        }
        else {
            fwrite(data.c_str(), 1, data.length(), file);
            int ferrno = ferror(file);
            if(ferrno) {
                clearerr(file);
                fprintf(stderr, "Writing to output failed! Errno %i\n", ferrno);
            }
        }
    }
    
    virtual bool init_stream() {
        file = fopen64(filename.c_str(), "wb");
        if(compress_data) {
            zfile = gzdopen(fileno(file), "w0");
                gzsetparams(zfile, compression_level, Z_DEFAULT_STRATEGY);
        }
        return true;
    }
    
public:
    TextStream()
    : DataStream(), file(0), zfile(0), frame_counter(0) {
    }
    virtual ~TextStream() {
        if(zfile) gzclose(zfile);
        else if(file) fclose(file);
    }
    virtual bool write_header() {
        string plaintext_user_header;
        for(map<string, string>::iterator it = user_header.begin(); it != user_header.end(); it++) {
            plaintext_user_header += "# ";
            plaintext_user_header += it->first;
            plaintext_user_header += " = ";
            plaintext_user_header += it->second;
            plaintext_user_header += "\n";
        }
        char buf[65535];
        sprintf(buf,
            "##METATEXT\n"
            "# version_i = 1\n"
            "# compressed_b = %i\n"
            "# frames_per_sample_i = %i\n"
            "# free_trigger_b = %i\n"
            "##USERHEADER\n%s",
//                     "# num_frames_i = %i\n
            compress_data, frames_per_sample, free_trigger, plaintext_user_header.c_str());
        write_raw(buf, true);
        return true;
    }
    virtual bool write_frame(float* time, float* data) {
        char buf[65535];
        size_t len = sprintf(buf, "\n\n##FRAME:%i\n", frame_counter);
        for(int i=0; i<frames_per_sample; i++) {
            len += sprintf(buf+len, "%f %f\n", time[i], data[i]);
        }
        write_raw(buf, false);
        frame_counter++;
        return true;
    }
    virtual bool finalize() {
        return true;
    }
};

#endif