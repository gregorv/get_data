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
    std::string write_fmt;

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
        write_fmt = "%f";
        for(auto ch: ch_config) {
            if(ch != -1) {
                write_fmt += " %f";
            }
        }
        write_fmt += '\n';
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
    virtual bool write_frame(float* time, const std::array<float*, 4>& data) {
        char buf[65535];
        size_t len = sprintf(buf, "\n\n##FRAME:%i\n", frame_counter);
        for(int i=0; i<frames_per_sample; i++) {
            len += sprintf(buf+len, write_fmt.c_str(), time[i],
                           ch_config[0] != -1? data[ch_config[0]][i] : 0.0f,
                           ch_config[1] != -1? data[ch_config[1]][i] : 0.0f,
                           ch_config[2] != -1? data[ch_config[2]][i] : 0.0f,
                           ch_config[3] != -1? data[ch_config[3]][i] : 0.0f
                   );
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
