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

struct dat_header {
    char magic[5];
    uint8_t version;
    uint16_t frames_per_sample;
    uint32_t num_frames;
    uint8_t compressed;
    char reserved[7];
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
    int optchar = -1;
    string output_directory("");
    string output_file("data.out");
    string output_file_prefix("sample_");
    unsigned int num_frames = 10;
    bool binary_output = false;
    bool mode_2048 = false;
//     bool auto_trigger = false;
    bool compress_data = false;
    int compression_level = 9;
    while((optchar = getopt(argc, argv, "12bd:p:n:hvf:cl:a")) != -1) {
        if(optchar == '?') return 1;
        else if(optchar == 'h') {
            std::cout << "Usage: " << argv[0]
                      << " [-d OUTPUT_DIR] [-f OUTPUT_FILE] [-p PREFIX] [-n NUM_FRAMES] [-v12bch]\n\n"
                      << "Command line arguments\n"
                      << " -1             1024-samples per frame (default)\n"
                      << " -2             2048-samples per frame\n"
                      << " -a             Free-running mode (no trigger)\n"
                      << " -b             binary output file\n"
                      << " -c             Enable zlib compression (only works with single text file).\n"
                      << " -d             Output directory (will create one file per frame!)\n"
                      << " -f             File output (creates a single file for all frame, with meta information\n"
                      << " -l LVL         Set compression level (default 9). Only used if -c is set\n"
                      << " -n NUM_FRAMES  Number of frames to record\n"
                      << " -p PREFIX      Filename prefix for directory output\n"
                      << " -v             Verbose output\n"
                      << std::endl;
            return 1;
        }
        else if(optchar == 'd') output_directory = optarg;
        else if(optchar == 'f') { output_file = optarg; binary_output = true; }
        else if(optchar == 'p') output_file_prefix = optarg;
        else if(optchar == 'a') auto_trigger = true;
        else if(optchar == 'c') compress_data = true;
        else if(optchar == 'l') compression_level = atoi(optarg);
        else if(optchar == 'n') {
            istringstream in(optarg);
            in >> num_frames;
        }
        else if(optchar == 'v') verbose = true;
        else if(optchar == 'b') binary_output = true;
        else if(optchar == '2') mode_2048 = true;
        else if(optchar == '1') mode_2048 = false;
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
        b->SetTriggerSource(1); // CH1
    //     b->SetTriggerDelayNs(int(1024/0.69));
        b->SetTriggerDelayPercent(100);
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
    FILE* output = 0;
    gzFile output_compressed = 0;
    struct dat_header header;
    if(single_file_output) {
        if(binary_output) {
            output = fopen(output_file.c_str(), "wb");
            memset(&header, 0, sizeof(struct dat_header));
            header.magic[0] = '#';
            header.magic[1] = 'D';
            header.magic[2] = 'T';
            header.magic[3] = 'A';
            header.magic[4] = '\n';
            header.version = 1;
            header.compressed = false;
            header.frames_per_sample = mode_2048? 2048: 1024;
            header.num_frames = 0;
            fwrite(&header, sizeof(struct dat_header), 1, output);
        }
        else {
            sprintf(buf,
                    "##METATEXT\n"
                    "# version_i = 1\n"
                    "# compressed_b = %b\n"
                    "# frames_per_sample_i = %i\n",
//                     "# num_frames_i = %i\n
                    compress_data, mode_2048? 2048: 1024);
            if(compress_data) {
                // write header with lvl0 compression (uncompressed)
                output_compressed = gzopen(output_file.c_str(), "w0");
                gzwrite(output_compressed, buf, strlen(buf));
                // switch to specified compression level
                gzsetparams(output_compressed, compression_level, Z_DEFAULT_STRATEGY);
            }
            else {
                output = fopen(output_file.c_str(), "w");
                fwrite(buf, 1, strlen(buf), output);
            }
        }
    }
    
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
        if(single_file_output) {
            if(binary_output) {
                fwrite(&time, sizeof(float), num_samples, output);
                fwrite(&data, sizeof(float), num_samples, output);
                num_frames_written++;
            }
            else {
                size_t len = sprintf(buf, "##FRAME:%i\n", i);
                if(compress_data) gzwrite(buf, len, output_compressed);
                else fwrite(buf, 1, len, output);
                len = 0;
                for(int i=0; i<num_samples; i++) {
                    len += sprintf(buf+len, "%f %f\n", time[i], data[i]);
                }
                if(compress_data) gzwrite(buf, len, output_compressed);
                else fwrite(buf, 1, len, output);
            }
        }
        else {
            ostringstream fname;
            fname << output_directory << output_file_prefix << i+1;
            if(binary_output) {
                fname << ".dat";
                FILE* f = fopen(fname.str().c_str(), "wb");
                fwrite("#BIN\n", strlen("#BIN\n"), 1, f);
                fwrite(&time, sizeof(float), num_samples, f);
                fwrite(&data, sizeof(float), num_samples, f);
                fclose(f);
            }
            else {
                fname << ".csv";
                FILE* f = fopen(fname.str().c_str(), "w");
                fwrite("#TXT\n", strlen("#TXT\n"), 1, f);
                for(int i=0; i<num_samples; i++) {
                    fprintf(f, "%f", time[i]);
                    fprintf(f, " %f", data[i]);
                    fprintf(f, "\n");
                }
                fclose(f);
            }
        }
    }
    if(single_file_output) {
        rewind(output);
        header.num_frames = num_frames_written;
        fwrite(&header, sizeof(struct dat_header), 1, output);
        fclose(output);
    }
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
