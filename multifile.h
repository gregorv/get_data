
#ifndef _MULTIFILE_H_
#define _MULTIFILE_H_

#include "datastream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace std;

class MultiFileStream : public DataStream {
protected:
    int frame_counter;
    virtual bool init_stream() {
        return true;
    }

public:
    MultiFileStream() : frame_counter(0) {
    }
    virtual bool write_header() {
        return true;
    }
    virtual bool write_frame(float* time, float* data) {
        if(frame_counter >= 1000000)
            return false;
        if(binary_output) {
            char fname[1024];
            sprintf(fname, "%s%s_%05i.dat", directory.c_str(), filename.c_str(), frame_counter);
            FILE* f = fopen(fname, "wb");
            fwrite("#BIN\n", strlen("#BIN\n"), 1, f);
            fwrite(&time, sizeof(float), frames_per_sample, f);
            fwrite(&data, sizeof(float), frames_per_sample, f);
            fclose(f);
        }
        else {
            char fname[1024];
            sprintf(fname, "%s%s_%06i.csv", directory.c_str(), filename.c_str(), frame_counter);
            FILE* f = fopen(fname, "w");
            fwrite("#TXT\n", strlen("#TXT\n"), 1, f);
            for(int i=0; i<frames_per_sample; i++) {
                fprintf(f, "%f", time[i]);
                fprintf(f, " %f", data[i]);
                fprintf(f, "\n");
            }
            fclose(f);
        }
        frame_counter++;
        return true;
    }
    virtual bool finalize() {
        return true;
    }
};

#endif