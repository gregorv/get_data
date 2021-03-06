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

#ifndef _BINARY_H_
#define _BINARY_H_

#include "datastream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>

using namespace std;

#define DAT_COMPRESSED 1
#define DAT_FREE_TRIGGER 2

struct dat_header {
    /*20 byte -> 0x14*/
    char magic[5];
    uint8_t version;
    uint16_t frames_per_sample;
    uint32_t num_frames;
    uint8_t flags;
    char reserved1[3];
    uint16_t data_offset;
    char reserved2[2];
};
static_assert(sizeof(dat_header) == 20, "dat_header struct has unexpected size on this platform!");

class BinaryStream : public DataStream {
protected:
    FILE* file;
    unsigned int frame_counter;
    dat_header header;

    virtual bool init_stream() {
        file = fopen64(filename.c_str(), "wb");
        return true;
    }

public:
    BinaryStream() : frame_counter(0), header() {
    }
    virtual ~BinaryStream() {
        if(file) fclose(file);
    }
    virtual bool write_header() {
	memset(&header, 0, sizeof(header));
	header.magic[0] = '#';
	header.magic[1] = 'D';
	header.magic[2] = 'T';
	header.magic[3] = 'A';
	header.magic[4] = '\n';
	header.frames_per_sample = frames_per_sample;
	header.num_frames = 0;
	header.version = 1;
	header.flags = 0;
	if(free_trigger)
            header.flags |= DAT_FREE_TRIGGER;
        string user_header_string = "";
	for(map<string, string>::iterator it = user_header.begin();
	    it != user_header.end(); it++) {
            user_header_string += it->first;
	    user_header_string += '=';
	    user_header_string += it->second;
	    user_header_string += '\n';
	}
	header.data_offset = user_header_string.length();
	fwrite(&header, sizeof(header), 1, file);
	fwrite(user_header_string.c_str(), user_header_string.length(), 1, file);
	fflush(file);
	return true;
    }

    virtual bool write_frame(const nanoseconds& record_time, float* time, float* data) {
        if(frame_counter == 4294967295UL)
            return false;
        frame_counter++;
        fwrite(time, sizeof(float), frames_per_sample, file);
        fwrite(data, sizeof(float), frames_per_sample, file);
	fflush(file);
        return true;
    }

    virtual bool write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data)
    {
        throw not_suppported_write("Binary format with multi channel recording.");
    }

    virtual bool finalize() {
        header.num_frames = frame_counter;
	rewind(file);
	fwrite(&header, sizeof(header), 1, file);
	fflush(file);
        return true;
    }

    virtual std::string get_file_extension() const {
        return std::string(".cdt");
    }
};

#endif
