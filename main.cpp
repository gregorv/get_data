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

#include "configuration.h"

#include <iostream>
#include <string>
#include <sstream>
#include <array>
#include <boost/lexical_cast.hpp>
#include <drs.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <vector>
#include <cassert>
#include <chrono>
#include "textstream.h"
#include "multifile.h"
#include "yaml_binary.h"
#include "binary.h"
#include "detectorcontrol.h"
#ifdef ROOT_FOUND
 #include "rootoutput.h"
#endif

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::time_point;
using std::chrono::nanoseconds;

enum output_format_t {
    OF_MULTIFILE,
    OF_MULTIFILE_BIN,
    OF_TEXTSTREAM,
    OF_BINARY,
    OF_YAML_BINARY,
    OF_ROOT
};

DRSBoard* board;

bool verbose = true;
bool abort_measurement = false;
bool auto_trigger = false;

struct sample
{
    float time[1024];
    float data[8][1024];
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
    string output_file("");
    output_format_t output_format = OF_TEXTSTREAM;
    unsigned int num_frames = 10;
//     bool auto_trigger = false;
    bool compress_data = false;
    int compression_level = 9;
    float trigger_delay_percent = 100;
    vector<string> user_header;
    bool do_multichannel_recording = false;
    std::array<int, 4> ch_num{ { 0, -1, -1, -1} };
    int trigger_ch_num = 0;
    float T_soll = 0;
    float trigger_threshold = -0.05;
    float sample_rate = 0.68;
    bool trigger_edge_negative = true;
    bool use_control = false;
    string unix_socket("/tmp/detector_control.unix");
    while((optchar = getopt(argc, argv, "bBd:p:n:ho:f:F:PCH:l:aT:D:Us:t:c:v")) != -1) {
        if(optchar == '?') return 1;
        else if(optchar == 'h') {
            std::cout << "Usage: " << argv[0]
                      << " [OPTIONS]\n\n"
                      << "Command line arguments\n"
                      << " -a               Free-running mode (no trigger)\n"
                      << " -c CH1[,CH2,...] Set one or more readout channel numbers (default = 1)\n"
                      << " -C               Enable zlib compression (only works with single text file).\n"
                      << " -d               Output directory (will create one file per frame!)\n"
                      << " -f FORMAT        Set the format of the recorded data.\n"
                      << "                  FORMAT is one of MULTIFILE, MULTIFILE_BIN, TEXT, BIN, YAML or ROOT.\n"
                      << " -F f_SAMPLE      Sampling frequency in GSp/s, range ~0.68-5, default 0.68GSp/s\n"
                      << " -H user_header   Add a line to the user header\n"
//                       << " -k COMMENT_VARS Add commentary variables to output file (single-file ASCII only)
                      << " -l LVL           Set compression level (default 9). Only used if -c is set\n"
                      << " -n NUM_FRAMES    Number of frames to record\n"
                      << " -o               Name of the output file(s).\n"
                      << " -P               Trigger on positive edge [default NEGATIVE]\n"
//                       << " -v               Verbose output\n"
                      << " -t TrigTrheshV   Set the trigger threshold in Volts. Default = -0.05V\n"
                      << " -T [CH_NUM|ext]  Trigger on channel CH_NUM or 'ext' for external trigger\n"
                      << " -D delay         Trigger Delay in percent\n"
                      << " -U socket        UNIX domain socket for detector control.\n"
                      << "                  Default: /tmp/detector_control.unix\n"
                      << " -s T_soll        Enable temperature stabilized measurement.\n"
                      << "                  Value in Kelvin if suffixed by K, other wise\n"
                      << "                  it is interpreted as degree Celsius\n"
#ifndef ROOT_FOUND
                      << "\nThis version of get_data was compiled without ROOT support!\n"
#endif
                      << std::endl;
            return 1;
        }
        else if(optchar == 'd') {
            output_directory = optarg;
        }
        else if(optchar == 'o') { output_file = optarg; }
        else if(optchar == 'f') {
            std::string format_str(optarg);
            if(format_str == "MULTIFILE") output_format = OF_MULTIFILE;
            else if(format_str == "MULTIFILE_BIN") output_format = OF_MULTIFILE_BIN;
            else if(format_str == "TEXT") output_format = OF_TEXTSTREAM;
            else if(format_str == "BIN") output_format = OF_BINARY;
            else if(format_str == "YAML") output_format = OF_YAML_BINARY;
            else if(format_str == "ROOT") output_format = OF_ROOT;
            else {
                std::cerr << argv[0] << ": Unknown output format " << optarg << std::endl;
                return 1;
            }
        }
        else if(optchar == 'F') {
            try {
                sample_rate = boost::lexical_cast<float>(optarg);
            } catch(boost::bad_lexical_cast const& e) {
                std::cerr << argv[0] << ": Cannot parse sample rate '" << optarg << "', must be floating point number." << std::endl;
                return 1;
            }
        }
        else if(optchar == 'a') auto_trigger = true;
        else if(optchar == 'C') compress_data = true;
        else if(optchar == 'l') compression_level = atoi(optarg);
	else if(optchar == 't') trigger_threshold = atof(optarg);
        else if(optchar == 'n') {
            istringstream in(optarg);
            in >> num_frames;
        }
        else if(optchar == 'v') std::cout << "Note: verbose mode active by default! Changes on console status display comming in future versions! :-)" << std::endl;
        else if(optchar == 'H') user_header.push_back(optarg);
        else if(optchar == 'D') trigger_delay_percent = atof(optarg);
        else if(optchar == 'T') {
            if(strcmp(optarg, "EXT") == 0 || strcmp(optarg, "ext") == 0) {
                trigger_ch_num = 4; // external
            }
            else {
                try {
                    trigger_ch_num = boost::lexical_cast<int>(optarg);
                } catch(const boost::bad_lexical_cast& e) {
                    std::cerr << argv[0] << ": Invalid trigger channel number '"
                              << optarg << "'!" << std::endl;
                    return -1;
                }
                trigger_ch_num -= 1; // we want "Channel 1" to have the internal number 0
                if(trigger_ch_num < 0 || trigger_ch_num > 3) {
                    std::cerr << argv[0] << ": Invalid trigger channel number " << optarg
                              << "! Must be  in range 1..4" << std::endl;
                    return -1;
                }
            }
        }
        else if(optchar == 'P') {
            trigger_edge_negative = false;
        }
        else if(optchar == 's') {
            istringstream is(optarg);
            is >> T_soll;
            if(optarg[strlen(optarg)-1] != 'K') {
                T_soll += 273.15;
            }
            use_control = true;
        }
        else if(optchar == 'U') {
            unix_socket = optarg;
        }
        else if(optchar == 'c') {
            std::istringstream ss(optarg);
            std::string ch_string;
            for(size_t i = 0; i<4 and std::getline(ss, ch_string, ','); i++) {
                try {
                    ch_num[i] = boost::lexical_cast<int>(ch_string);
                } catch(boost::bad_lexical_cast const& e) {
                    std::cerr << argv[0] << ": Cannot parse channel record specification '"
                              << optarg << "', token '"
                              << ch_string << "'! Channel numbers must be comma separated integers."
                              << std::endl;
                    return -1;
                }
                if(ch_num[i] < 1 || ch_num[i] > 4) {
                    std::cerr << argv[0] << ": Cannot parse channel record specification '"
                              << optarg << "', token '"
                              << ch_string << "'! Channel numbers must be in 1..4 range."
                              << std::endl;
                    return -1;
                }
                if(i > 0) do_multichannel_recording = true;
                ch_num[i]--;
            }
        }
    }

#ifndef ROOT_FOUND
    if(output_format == OF_ROOT) {
        std::cerr << argv[0] << ": get_data was not compiled with ROOT support!" << std::endl;
        return 1;
    }
#endif

    DetectorControl* control = NULL;
    if(use_control) {
        if(verbose) std::cout << "Set detector control temperature to " << T_soll << "K (" << T_soll-273.15 << "C)" << std::endl;
        control = new DetectorControl(unix_socket);
        if(!control->connect_control())
            return -1;
        control->setTsoll(T_soll);
        sleep(2);
        control->disconnect_control();
    }

    if(!compress_data)
        compression_level = -1;

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
    b->SetFrequency(sample_rate, true); // sampling freq in GHz
    b->SetInputRange(0);

    if(!auto_trigger) {
        // trigger settings
        b->SetTranspMode(1);
        b->EnableTrigger(1, 0);
        b->SetTriggerSource(1<<trigger_ch_num); // CH1
    //     b->SetTriggerDelayNs(int(1024/0.69));
        b->SetTriggerDelayPercent(trigger_delay_percent);
        if(verbose) {
            std::cout << "Selected channels: column 1: CH " << ch_num[0] + 1;
            for(size_t i=1; i<4; i++) {
                if(ch_num[i] != -1) std::cout << "\n                   column " << i+1 << ": CH " << ch_num[i] + 1;
            }
            std::cout << std::endl;
            std::cout << "Trigger delay " << b->GetTriggerDelayNs() << "ns, " << b->GetTriggerDelay() << "%" << std::endl;
            std::cout << "Trigger threshold " << trigger_threshold << " V, "
                      << (trigger_edge_negative? "negative" : "positive")
                      << " polarity" << endl;
            if(trigger_ch_num == 4)
                cout << "Trigger source: External" << endl;
            else
                cout << "Trigger source: Channel " << trigger_ch_num+1 << endl;
        }
        b->SetTriggerLevel(trigger_threshold, trigger_edge_negative); // (V), pos. edge == false
    }

    std::unique_ptr<DataStream> datastream;
    bool binary_output = true;
    if(output_format == OF_MULTIFILE) {
        binary_output = false;
        datastream.reset(new MultiFileStream);
    } else if(output_format == OF_MULTIFILE_BIN) {
        binary_output = true;
        datastream.reset(new MultiFileStream);
    } else if(output_format == OF_BINARY) {
        binary_output = true;
        datastream.reset(new BinaryStream);
    } else if(output_format == OF_YAML_BINARY) {
        binary_output = true;
        datastream.reset(new YAMLBinaryStream);
    } else if(output_format == OF_TEXTSTREAM) {
        binary_output = compress_data;
        datastream.reset(new TextStream);
#ifdef ROOT_FOUND
    } else if(output_format == OF_ROOT) {
        datastream.reset(new RootOutput);
//     } else if(output_format == OF_ROOT_TREE) {
//         datastream.reset(new RootTree);
#endif
    } else {
        std::cerr << argv[0] << ": output format not implemented!" << std::endl;
        return 1;
    }
    if(use_control)
        datastream->add_user_entry("T_soll_f", T_soll);
    datastream->init(output_directory, output_file, 1024,
                     compression_level, auto_trigger, binary_output,
                     trigger_delay_percent, ch_num,
                     argc, argv
                    );
    datastream->write_header();

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = terminate;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    if(verbose) std::cout << "Sampling Rate " << b->GetFrequency() << " GSp/s" << std::endl;
    if(verbose) std::cout << "Record " << num_frames << " frames" << std::endl;
    uint32_t num_frames_written = 0;
    bool temperature_stable = false;
    int subframe_set = 500;
    float time[2048];
    float data_1[2048];
    float data_2[2048];
    float data_3[2048];
    float data_4[2048];
    std::array<float*, 4> data_array{ {data_1, data_2, data_3, data_4} };
    auto start_time = high_resolution_clock::now();
    for(unsigned int i=0; i<num_frames && !abort_measurement; i++) {
        if(temperature_stable) {
            control->connect_control();
            if(!control->temperatureStable()) {
                temperature_stable = false;
                abort_measurement = true;
                std::cout << "\33[2K\rTemperature stabilization failed, probably cooling limits reached. Abort measurement" << std::endl;
                control->disconnect_control();
                continue;
            }
            control->disconnect_control();
        } else {
            while(use_control && !temperature_stable && !abort_measurement) {
                control->connect_control();
                temperature_stable = control->temperatureStable();
                float T_ist = control->getTist();
                control->disconnect_control();
                if(verbose) 
                    std::cout << "\33[2K\rWait for detector temperature to stabilize... T_ist="
                              << T_ist << "K, T_ist-T_soll=" << T_ist-T_soll << std::flush;
                sleep(1);
            }
        }
        if(abort_measurement) {
            continue;
        }
        if(use_control) {
            control->connect_control();
            control->hold_start();
//             std::cout << "Hold start" << std::endl;
        }
        unsigned int j=0;

        for(j=i; j<(i+subframe_set) && j<num_frames && !abort_measurement; j++) {
//             std::cout << "Sample!" << std::endl;
            if(verbose) {
                if(j % 10 == 0)
                    std::cout << "\33[2K\rSample " << j << " of " << num_frames << std::flush;
            }
            captureSample();
            if(abort_measurement) break;
            auto record_time = duration_cast<nanoseconds>(high_resolution_clock::now() - start_time);
            board->GetTime(0, board->GetTriggerCell(0), time);
            board->GetWave(0, 0, data_1);
            if(do_multichannel_recording) {
                board->GetWave(0, 2, data_2);
                board->GetWave(0, 4, data_3);
                board->GetWave(0, 6, data_4);
                if(!datastream->write_frame(record_time, time, data_array)) {
                    abort_measurement = true;
                    break;
                }
            } else {
                if(!datastream->write_frame(record_time, time, data_1)) {
                    abort_measurement = true;
                    break;
                }
            }
            num_frames_written++;
        }
        i = j;
        if(use_control) {
//             std::cout << "Hold end" << std::endl;
            control->hold_end();
            control->disconnect_control();
            usleep(1000);
        }
    }
    datastream->finalize();
    datastream.release();
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
