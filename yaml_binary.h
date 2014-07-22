/*
 * get_data - Record data from DRS4 Evaluation boards via provided libs
 * Copyright (C) 2014  Gregor Vollmer <vollmer@ekp.uni-karlsruhe.de>
 *                     and those in the CONTRIBUTORS file
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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
        return true;
    }
    virtual bool write_frame(const nanoseconds& record_time, float* time, float* data) {
        if(frame_counter > 999999999)
            return false;
        frame_counter++;
        fwrite(time, sizeof(float), frames_per_sample, file);
        fwrite(data, sizeof(float), frames_per_sample, file);
        return true;
    }
    virtual bool write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data)
    {
        throw not_suppported_write("YAML binary with multi channel recording.");
        return true;
    }
    virtual bool finalize() {
        return true;
    }

    virtual std::string get_file_extension() const {
        return std::string(".ybin");
    }
};

#endif
