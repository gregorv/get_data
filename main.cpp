#include <iostream>
#include <string>
#include <sstream>
#include <drs.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <signal.h>
#include <zlib.h>
#include <time.h>
#include <vector>
#include <assert.h>
#include "textstream.h"
#include "multifile.h"
#include "yaml_binary.h"

using namespace std;

DRSBoard* board;

bool verbose = false;
bool abort_measurement = false;
bool auto_trigger = false;

struct sample
{
    float time[1024];
    float data[8][1024];
};

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

void captureSample() {
    board->StartDomino();
    if(auto_trigger)
        board->SoftTrigger();
    while(board->IsBusy() && !abort_measurement);
    board->TransferWaves();
}

void getSample(sample& s) {
    board->StartDomino();
    while(board->IsBusy());
    board->TransferWaves();
    board->GetTime(0, board->GetTriggerCell(0), s.time);
    for(int i=0; i<8; i++) {
        board->GetWave(0, i, s.data[i]);
    }
}

void terminate(int signum) {
    if(signum == SIGTERM || signum == SIGINT) {
        abort_measurement = true;
    }
}

int main(int argc, char **argv) {
    // ENSURE that the header has the correct length...
    assert(sizeof(struct dat_header) == 20);
    int optchar = -1;
    string output_directory("");
    string output_file("");
    string output_file_prefix("sample_");
    unsigned int num_frames = 10;
    bool binary_output = false;
    int binary_version = 0;
    bool mode_2048 = false;
//     bool auto_trigger = false;
    bool compress_data = false;
    int compression_level = 9;
    float trigger_delay_percent = 100;
    vector<string> user_header;
    int ch_num = 0;
    while((optchar = getopt(argc, argv, "12bBd:p:n:hvf:CH:l:aT:D:")) != -1) {
        if(optchar == '?') return 1;
        else if(optchar == 'h') {
            std::cout << "Usage: " << argv[0]
                      << " [-d OUTPUT_DIR] [-f OUTPUT_FILE] [-p PREFIX] [-n NUM_FRAMES] [-H user_header] [-c channel] [-D DELAY] [-T CH_NUM|ext] [-v12bBCh]\n\n"
                      << "Command line arguments\n"
                      << " -1              1024-samples per frame (default)\n"
                      << " -2              2048-samples per frame\n"
                      << " -a              Free-running mode (no trigger)\n"
                      << " -b              binary output file; YAML header\n"
                      << " -B              binary output file, version 1\n"
//                       << " -c channel      Set readout channel number (default = 1)\n"
                      << " -C              Enable zlib compression (only works with single text file).\n"
                      << " -d              Output directory (will create one file per frame!)\n"
                      << " -f              File output (creates a single file for all frame, with meta information\n"
                      << " -H user_header  Add a line to the user header\n"
//                       << " -k COMMENT_VARS Add commentary variables to output file (single-file ASCII only)
                      << " -l LVL          Set compression level (default 9). Only used if -c is set\n"
                      << " -n NUM_FRAMES   Number of frames to record\n"
                      << " -p PREFIX       Filename prefix for directory output\n"
                      << " -v              Verbose output\n"
                      << " -T [CH_NUM|ext] Trigger on channel CH_NUM or 'ext' for external trigger\n"
                      << " -D delay        Trigger Delay in percent\n"
                      << std::endl;
            return 1;
        }
        else if(optchar == 'd') output_directory = optarg;
        else if(optchar == 'f') { output_file = optarg; }
        else if(optchar == 'p') output_file_prefix = optarg;
        else if(optchar == 'a') auto_trigger = true;
        else if(optchar == 'C') compress_data = true;
        else if(optchar == 'l') compression_level = atoi(optarg);
        else if(optchar == 'n') {
            istringstream in(optarg);
            in >> num_frames;
        }
        else if(optchar == 'v') verbose = true;
        else if(optchar == 'b') {
            binary_output = true;
            binary_version = 2;
        }
        else if(optchar == 'B') {
            binary_output = true;
            binary_version = 1;
        }
        else if(optchar == '2') mode_2048 = true;
        else if(optchar == '1') mode_2048 = false;
        else if(optchar == 'H') user_header.push_back(optarg);
        else if(optchar == 'D') trigger_delay_percent = atof(optarg);
        else if(optchar == 'T') {
            if(strcmp(optarg, "EXT") == 0 || strcmp(optarg, "ext")) {
                ch_num = 4; // external
            }
            else {
                std::cout << "Not implemented trigger" << std::endl;
            }
        }
    }
    if(!compress_data)
        compression_level = -1;
    if(output_file.length() == 0) {
        char default_filename[50];
        time_t now = time(0);
        strftime(default_filename, sizeof(default_filename)-1, "%Y-%m-%d_%H-%M",
            gmtime(&now)
        );
        output_file = default_filename;
        if(binary_output) output_file += ".dat";
        else output_file += ".cdt";
    }
    bool single_file_output = (output_directory.length() == 0);

    if(!single_file_output) {
        if(output_directory[output_directory.length()-1] != '/')
            output_directory += '/';
        if(mkdir(output_directory.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
            if(errno != EEXIST) {
                std::cerr << "Cannot create output directory" << std::endl;
                return 1;
            }
        }
    }

    DRS* drs = new DRS();
    if(drs->GetNumberOfBoards() > 0 && verbose)
        std::cout << "DRS Eval boards found, using first one" << std::endl;
    else if(drs->GetNumberOfBoards() == 0) {
        std::cerr << "No DRS Eval board can be accessed, aborting!" << std::endl;
        return 1;
    }
    DRSBoard* b = board = drs->GetBoard(0);
    std::cout << "Board type " << b->GetBoardType() << std::endl;
    std::cout << "DRS type " << b->GetDRSType() << std::endl;
    if(b->GetBoardType() != 8) {
        std::cerr << "DRS4 Eval Board required!" << std::endl;
        return 1;
    }
    // basic DRS setup
    b->Init();
    b->SetFrequency(0.68, true); // sampling freq in GHz
    b->SetInputRange(0);

    if(!auto_trigger) {
        // trigger settings
        b->SetTranspMode(1);
        b->EnableTrigger(1, 0);
        b->SetTriggerSource(1<<ch_num); // CH1
    //     b->SetTriggerDelayNs(int(1024/0.69));
        b->SetTriggerDelayPercent(trigger_delay_percent);
        if(verbose) std::cout << "Trigger delay " << b->GetTriggerDelayNs() << "ns, " << b->GetTriggerDelay() << std::endl;

        b->SetTriggerLevel(-0.01, true); // (V), pos. edge == false
    }
    
    // 2048 sample mode
    if(mode_2048) {
        b->SetChannelConfig(0, 1, 4);
        if(verbose) {
            std::cout << "2048 sample mode, channel cascading " << board->GetChannelCascading() << std::endl;
        }
    }
    else if(verbose) {
        std::cout << "1024 sample mode" << std::endl;
    }
    char buf[65536];
    DataStream* datastream;
    if(single_file_output) {
        if(binary_output && binary_version == 2)
            datastream = new YAMLBinaryStream;
        else if(binary_output && binary_version == 1) {
            std::cout << "Classical binary format not supported, at the moment. Sorry!" << std::endl;
            exit(-1);
        }
        else
            datastream = new TextStream();
    }
    else
        datastream = new MultiFileStream();
    datastream->init(output_directory, output_file, mode_2048? 2048:1024,
                     compression_level, auto_trigger, binary_output,
                     trigger_delay_percent);
    datastream->write_header();
    
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = terminate;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    if(verbose) std::cout << "Sampling Rate " << b->GetFrequency() << " GSp/s" << std::endl;
    if(verbose) std::cout << "Record " << num_frames << " frames" << std::endl;
    int num_samples = mode_2048? 2048 : 1024;
    uint32_t num_frames_written = 0;
    for(unsigned int i=0; i<num_frames && !abort_measurement; i++) {
        if(verbose) {
            if(i % 10 == 0)
                std::cout << "\33[2K\rSample " << i << " of " << num_frames << std::flush;
        }
        captureSample();
        if(abort_measurement) break;
        float time[2048];
        float data[2048];
        board->GetTime(0, board->GetTriggerCell(0), time);
        board->GetWave(0, mode_2048? 0 : 0, data);
        datastream->write_frame(time, data);
    }
    datastream->finalize();
    if(verbose) {
        if(abort_measurement)
            std::cout << "\33[K\rAborted reading samples after "
                      << num_frames_written << " frames" << std::endl;
        else
            std::cout << "\33[2K\rDone reading samples" << std::endl;
    }

    delete drs;
    return 0;
}
