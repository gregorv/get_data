
#ifndef _DATASTREAM_H_
#define _DATASTREAM_H_

#include <map>
#include <string>

using namespace std;

class DataStream {
protected:
    int frames_per_sample;
    bool compress_data;
    int compression_level;
    bool free_trigger;
    bool binary_output;
    float trigger_delay_percent;
    map<string, string> user_header;
    string filename;
    string directory;
    
    virtual bool init_stream() = 0;
    
public:
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
    virtual bool init(string p_directory, string p_filename, int p_frames_per_sample,
                      int p_compression_level, bool p_free_trigger, bool p_binary_output, float p_trigger_delay_percent) {
        filename = p_filename;
        directory = p_directory;
        frames_per_sample = p_frames_per_sample;
        compression_level = p_compression_level;
        compress_data = p_compression_level != -1;
        free_trigger = p_free_trigger;
        binary_output = p_binary_output;
        trigger_delay_percent = p_trigger_delay_percent;
        return init_stream();
    }
    virtual void add_user_entry(string key, string value) {
        user_header[key] = value;
    }
    virtual void add_user_entry(string key, int value) {
        ostringstream oss;
        oss << value;
        add_user_entry(key, oss.str());
    }
    virtual void add_user_entry(string key, bool value) {
        ostringstream oss;
        oss << value;
        add_user_entry(key, oss.str());
    }
    virtual void add_user_entry(string key, float value) {
        ostringstream oss;
        oss << value;
        add_user_entry(key, oss.str());
    }
    virtual bool write_header() = 0;
    virtual bool write_frame(float* time, float* data) = 0;
    virtual bool finalize() = 0;
};

#endif