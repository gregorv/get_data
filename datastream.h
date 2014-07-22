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

#ifndef _DATASTREAM_H_
#define _DATASTREAM_H_

#include <map>
#include <array>
#include <string>
#include <sstream>
#include <chrono>

using std::chrono::nanoseconds;

class DataStream {
protected:
    int frames_per_sample;
    bool compress_data;
    int compression_level;
    bool free_trigger;
    bool binary_output;
    float trigger_delay_percent;
    std::map<std::string, std::string> user_header;
    std::string filename;
    std::string directory;
    std::array<int, 4> ch_config;
    std::string command_line;

    virtual bool init_stream() = 0;

public:

    class not_suppported_write : std::exception
    {
    public:
        not_suppported_write(const std::string& wht)
         : m_what(wht)
        {
        }

        virtual const char* what() noexcept
        {
            return m_what.c_str();
        }
    private:
        std::string m_what;
    };

    DataStream()
    : frames_per_sample(1024),
    compress_data(false),
    compression_level(-1),
    free_trigger(false),
    binary_output(false),
    trigger_delay_percent(100.0),
    user_header(),
    filename(""),
    directory("")
    {
    }
    virtual ~DataStream() {
    };
    
    /**
    compression_level == -1: No compression
    */
    virtual bool init(std::string p_directory, std::string p_filename, int p_frames_per_sample,
                      int p_compression_level, bool p_free_trigger, bool p_binary_output, float p_trigger_delay_percent,
                      std::array<int, 4> p_ch_config, int argc, char** argv
                     ) {
        filename = p_filename;
        if(filename.length() == 0) {
            char default_filename[50];
            time_t now = time(0);
            strftime(default_filename, sizeof(default_filename)-1, "%Y-%m-%d_%H-%M-%S", gmtime(&now));
            filename = default_filename;
            filename += get_file_extension();
        }
        directory = p_directory;
        frames_per_sample = p_frames_per_sample;
        compression_level = p_compression_level;
        compress_data = p_compression_level != -1;
        free_trigger = p_free_trigger;
        binary_output = p_binary_output;
        trigger_delay_percent = p_trigger_delay_percent;
        ch_config = p_ch_config;
        std::ostringstream cmd_stream;
        cmd_stream << argv[0];
        for(int i=1; i<argc; i++) {
            cmd_stream << " " << argv[i];
        }
        command_line = cmd_stream.str();
        return init_stream();
    }
    virtual void add_user_entry(std::string key, std::string value) {
        user_header[key] = value;
    }
    virtual void add_user_entry(std::string key, int value) {
        std::ostringstream oss;
        oss << value;
        add_user_entry(key, oss.str());
    }
    virtual void add_user_entry(std::string key, bool value) {
        std::ostringstream oss;
        oss << value;
        add_user_entry(key, oss.str());
    }
    virtual void add_user_entry(std::string key, float value) {
        std::ostringstream oss;
        oss << value;
        add_user_entry(key, oss.str());
    }
    virtual bool write_header() = 0;
    virtual bool write_frame(const nanoseconds& record_time, float* time, float* data) = 0;
    virtual bool write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data) = 0;
    virtual bool finalize() = 0;
    virtual std::string get_file_extension() const = 0;
};

#endif
