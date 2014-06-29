#include <iostream>
#include <string>
#include <sstream>
#include <array>
#include <boost/lexical_cast.hpp>
#include <drs.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <zlib.h>
#include <time.h>
#include <vector>
#include <assert.h>
#include "textstream.h"
#include "multifile.h"
#include "yaml_binary.h"
#include "binary.h"

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


class DetectorControl {
private:
    int sock;
    FILE* stream;
    string socket_file;
    bool send_msg(string message) {
        clearerr(stream);
        size_t written = fwrite(message.c_str(), 1, message.length(), stream);
        if(written == 0) {
            if(ferror(stream)) {
                std::cerr << "Detector control: cannot send command" << std::endl;
                return false;
            }
        }
        fflush(stream);
        return true;
    }
    
    string recv_line() {
        string line("");
        char* lineptr = NULL;
        size_t n = 0;
        errno = 0;
        if(getline(&lineptr, &n, stream) == -1) {
            if(errno) {
                std::cerr << "Detector control: cannot receive response: " << strerror(errno) << std::endl;
            }
        }
        else {
            line = lineptr;
        }
        if(lineptr) free(lineptr);
        return line;
    }
public:
    DetectorControl(string sfile)
    : sock(0), stream(0), socket_file(sfile) {
    }
    ~DetectorControl() {
        disconnect_control();
    }

    bool connect_control() {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if(sock == -1) {
            std::cerr << "Detector control: cannot create socket: " << strerror(errno) << std::endl;
            return false;
        }
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, socket_file.c_str());
        int len = strlen(addr.sun_path) + sizeof(addr.sun_family);
        if(connect(sock, (const sockaddr*)&addr, len) != 0) {
            std::cerr << "Detector control: cannot connect to UNIX domain socket: " << strerror(errno) << std::endl;
            return false;
        }
        stream = fdopen(sock, "r+");
        return true;
    }

    void disconnect_control() {
        close(sock);
        sock = 0;
    }

    void setTsoll(float T_soll) {
        ostringstream line;
        line << "T_soll=" << T_soll << "\n";
        send_msg(line.str());
    }

    float getTsoll() {
    }

    float getTist() {
        send_msg("T_detector\n");
        istringstream is(recv_line());
        float temperature;
        is >> temperature;
        return temperature;
    }

    bool temperatureStable() {
        send_msg("stable\n");
        string line = recv_line();
        return line.find("True") != string::npos;
    }

    void interruptMeasurement() {
        send_msg("interrupt\n");
    }

    void contineMeasurement() {
        send_msg("continue\n");
    }
    
    void hold_start() {
        send_msg("hol");
    }
    
    void hold_end() {
        send_msg("d\n");
    }
};


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
    bool do_multichannel_recording = false;
    std::array<int, 4> ch_num = { 0, -1, -1, -1};
    int trigger_ch_num = 0;
    float T_soll = 0;
    float trigger_threshold = -0.05;
    bool use_control = false;
    string unix_socket("/tmp/detector_control.unix");
    while((optchar = getopt(argc, argv, "12bBd:p:n:hvf:CH:l:aT:D:Us:t:c:")) != -1) {
        if(optchar == '?') return 1;
        else if(optchar == 'h') {
            std::cout << "Usage: " << argv[0]
                      << " [-d OUTPUT_DIR] [-1 OUTPUT_FILE] [-p PREFIX] [-n NUM_FRAMES] [-H user_header] [-c channel] [-D DELAY] [-t TRIGGER_LEVEL(V)] [-T CH_NUM|ext] [-v12bBCh]\n\n"
                      << "Command line arguments\n"
                      << " -1               1024-samples per frame (default)\n"
                      << " -2               2048-samples per frame\n"
                      << " -a               Free-running mode (no trigger)\n"
                      << " -B               binary output file; YAML header\n"
                      << " -b               binary output file, version 1\n"
                      << " -c CH1[,CH2,...] Set one or more readout channel numbers (default = 1)\n"
                      << " -C               Enable zlib compression (only works with single text file).\n"
                      << " -d               Output directory (will create one file per frame!)\n"
                      << " -f               File output (creates a single file for all frame, with meta information\n"
                      << " -H user_header   Add a line to the user header\n"
//                       << " -k COMMENT_VARS Add commentary variables to output file (single-file ASCII only)
                      << " -l LVL           Set compression level (default 9). Only used if -c is set\n"
                      << " -n NUM_FRAMES    Number of frames to record\n"
                      << " -p PREFIX        Filename prefix for directory output\n"
                      << " -v               Verbose output\n"
                      << " -t TrigTrheshV   Set the trigger threshold in Volts. Default = -0.05V\n"
                      << " -T [CH_NUM|ext]  Trigger on channel CH_NUM or 'ext' for external trigger\n"
                      << " -D delay         Trigger Delay in percent\n"
                      << " -U socket        UNIX domain socket for detector control.\n"
                      << "                  Default: /tmp/detector_control.unix\n"
                      << " -s T_soll        Enable temperature stabilized measurement.\n"
                      << "                  Value in Kelvin if suffixed by K, other wise\n"
                      << "                  it is interpreted as degree Celsius\n"
                      << std::endl;
            return 1;
        }
        else if(optchar == 'd') output_directory = optarg;
        else if(optchar == 'f') { output_file = optarg; }
        else if(optchar == 'p') output_file_prefix = optarg;
        else if(optchar == 'a') auto_trigger = true;
        else if(optchar == 'C') compress_data = true;
        else if(optchar == 'l') compression_level = atoi(optarg);
	else if(optchar == 't') trigger_threshold = atof(optarg);
        else if(optchar == 'n') {
            istringstream in(optarg);
            in >> num_frames;
        }
        else if(optchar == 'v') verbose = true;
        else if(optchar == 'B') {
            binary_output = true;
            binary_version = 2;
        }
        else if(optchar == 'b') {
            binary_output = true;
            binary_version = 1;
        }
        else if(optchar == '2') mode_2048 = true;
        else if(optchar == '1') mode_2048 = false;
        else if(optchar == 'H') user_header.push_back(optarg);
        else if(optchar == 'D') trigger_delay_percent = atof(optarg);
        else if(optchar == 'T') {
            if(strcmp(optarg, "EXT") == 0 || strcmp(optarg, "ext") == 0) {
                trigger_ch_num = 4; // external
            }
            else {
                trigger_ch_num = strtol(optarg, 0, 10);
                if(((trigger_ch_num == LLONG_MIN || trigger_ch_num == LLONG_MAX) && errno == ERANGE) ||
                   (trigger_ch_num == 0 && errno == EINVAL)) {
                    std::cerr << argv[0] << ": Invalid trigger channel number '" << optarg << "'!" << std::endl;
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
        b->SetTriggerSource(1<<trigger_ch_num); // CH1
    //     b->SetTriggerDelayNs(int(1024/0.69));
        b->SetTriggerDelayPercent(trigger_delay_percent);
        if(verbose) {
            std::cout << "Selected channels: column 1: CH " << ch_num[0] + 1;
            for(size_t i=1; i<4; i++) {
                if(ch_num[i] != -1) std::cout << "\n                   column " << i+1 << ": CH " << ch_num[i] + 1;
            }
            std::cout << std::endl;
            std::cout << "Trigger delay " << b->GetTriggerDelayNs() << "ns, " << b->GetTriggerDelay() << std::endl;
            std::cout << "Trigger threshold " << trigger_threshold << endl;
            if(trigger_ch_num == 4)
            	cout << "Trigger source: External" << endl;
            else
                cout << "Trigger source: Channel " << trigger_ch_num << endl;
        }
        b->SetTriggerLevel(trigger_threshold, true); // (V), pos. edge == false
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
        if(binary_output && binary_version == 2) {
            if(do_multichannel_recording) {
                std::cerr << argv[0] << ": Multi-channel recording does not work with binary output format :(" << std::endl;
                return -1;
            }
            datastream = new YAMLBinaryStream;
        }
        else if(binary_output && binary_version == 1) {
            if(do_multichannel_recording) {
                std::cerr << argv[0] << ": Multi-channel recording does not work with binary output format :(" << std::endl;
                return -1;
            }
            datastream = new BinaryStream;
        }
        else
            datastream = new TextStream();
    }
    else
        datastream = new MultiFileStream();
    if(use_control)
        datastream->add_user_entry("T_soll_f", T_soll);
    datastream->init(output_directory, output_file, mode_2048? 2048:1024,
                     compression_level, auto_trigger, binary_output,
                     trigger_delay_percent, ch_num);
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
    bool temperature_stable = false;
    int subframe_set = 500;
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
        }
        else {
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

        float time[2048];
        float data_1[2048];
        float data_2[2048];
        float data_3[2048];
        float data_4[2048];
        std::array<float*, 4> data_array = {data_1, data_2, data_3, data_4};
        for(j=i; j<(i+subframe_set) && j<num_frames && !abort_measurement; j++) {
//             std::cout << "Sample!" << std::endl;
            if(verbose) {
                if(j % 10 == 0)
                    std::cout << "\33[2K\rSample " << j << " of " << num_frames << std::flush;
            }
            captureSample();
            if(abort_measurement) break;
            board->GetTime(0, board->GetTriggerCell(0), time);
            board->GetWave(0, mode_2048? 0 : 0, data_1);
            if(do_multichannel_recording) {
                board->GetWave(0, 2, data_2);
                board->GetWave(0, 4, data_3);
                board->GetWave(0, 6, data_4);
                datastream->write_frame(time, data_array);
            } else {
                board->GetWave(0, mode_2048? 0 : 0, data_1);
                datastream->write_frame(time, data_1);
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
    delete datastream;
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
