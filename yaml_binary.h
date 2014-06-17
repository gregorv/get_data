
#ifndef _YAMLBINARY_H_
#define _YAMLBINARY_H_

#include "datastream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>

using namespace std;

class YAMLBinaryStream : public DataStream {
protected:
    FILE* file;
    int frame_counter;
    int first_header_length;
    virtual bool init_stream() {
        file = fopen64(filename.c_str(), "wb");
        return true;
    }
    
    string format_header() {
        ostringstream header;
        char nframes_str[20];
        sprintf(nframes_str, "%09i", frame_counter);
        header << "---\n";
        header << " - version: 1\n";
        // header << " - num_frames: " << nframes_str << "\n";
        header << " - free_trigger: " << (free_trigger? "True\n" : "False\n");
        for(map<string, string>::iterator it=user_header.begin();
            it != user_header.end();
            it++) {
            header << " - " << it->first << ": " << it->second << "\n";
        }
        header << "...";
        return header.str();
    }

public:
    YAMLBinaryStream() : frame_counter(0), first_header_length(0) {
    }
    virtual ~YAMLBinaryStream() {
        if(file) fclose(file);
    }
    virtual bool write_header() {
        add_user_entry("samples_per_frame", frames_per_sample);
        add_user_entry("trigger_delay_percent", trigger_delay_percent);
        string header = format_header();
        first_header_length = header.length();
        fwrite(header.c_str(), header.length(), 1, file);
    }
    virtual bool write_frame(float* time, float* data) {
        if(frame_counter > 999999999)
            return false;
        frame_counter++;
        fwrite(time, sizeof(float), frames_per_sample, file);
        fwrite(data, sizeof(float), frames_per_sample, file);
        return true;
    }
    virtual bool finalize() {
        return true;
    }
};

#endif
