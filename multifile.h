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

#ifndef _MULTIFILE_H_
#define _MULTIFILE_H_

#include "datastream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

using namespace std;

class MultiFileStream : public DataStream {
protected:
    int frame_counter;
    virtual bool init_stream() {
        filename = "sample";
        if(directory.length() == 0) {
            char default_dir[50];
            time_t now = time(0);
            strftime(default_dir, sizeof(default_dir)-1, "%Y-%m-%d_%H-%M-%S/", gmtime(&now));
            directory = default_dir;
        }
        if(directory[directory.length()-1] != '/') {
            directory += '/';
        }
        if(mkdir(directory.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
            if(errno != EEXIST) {
                std::cerr << "Cannot create output directory" << std::endl;
                return false;
            } else {
                std::cout << "Overwriting data in output directory!" << std::endl;
            }
        }
        return true;
    }

public:
    MultiFileStream() : frame_counter(0) {
    }
    virtual bool write_header() {
        return true;
    }
    virtual bool write_frame(const nanoseconds& record_time, float* time, float* data) {
        if(frame_counter >= 1000000)
            return false;
        if(binary_output) {
            char fname[1024];
            sprintf(fname, "%s%s_%05i.dat", directory.c_str(), filename.c_str(), frame_counter);
            FILE* f = fopen(fname, "wb");
            if(!f) {
                std::cerr << "Cannot open output file '" << fname << "', errno " << errno << std::endl;
                return false;
            }
            fwrite("#BIN\n", strlen("#BIN\n"), 1, f);
            fwrite(&time, sizeof(float), frames_per_sample, f);
            fwrite(&data, sizeof(float), frames_per_sample, f);
            fclose(f);
        }
        else {
            char fname[1024];
            sprintf(fname, "%s%s_%06i.csv", directory.c_str(), filename.c_str(), frame_counter);
            FILE* f = fopen(fname, "w");
            if(!f) {
                std::cerr << "Cannot open output file '" << fname << "', errno " << errno << std::endl;
                return false;
            }
            fwrite("#TXT\n", strlen("#TXT\n"), 1, f);
            fprintf(f, "# cmd: %s\n# record timestamp: %li us\n",
                    command_line.c_str(), record_time.count() / 1000);
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
    virtual bool write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data) {
        if(frame_counter >= 1000000)
            return false;
        if(binary_output) {
            throw not_suppported_write("Multi-channel recording with binary data stream");
        }
        else {
            char fname[1024];
            sprintf(fname, "%s%s_%06i.csv", directory.c_str(), filename.c_str(), frame_counter);
            FILE* f = fopen(fname, "w");
            fwrite("#TXT\n", 5, 1, f);
            fprintf(f, "# cmd: %s\n# record timestamp: %li us\n",
                    command_line.c_str(), record_time.count() / 1000);
            for(int i=0; i<frames_per_sample; i++) {
                fprintf(f, "%f", time[i]);
                for(auto ch: ch_config) {
                    if(ch != -1) {
                        fprintf(f, " %f", data[ch][i]);
                    }
                }
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

    virtual std::string get_file_extension() const {
        return std::string(".unused");
    }
};

#endif
