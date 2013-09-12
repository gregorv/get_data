#include <iostream>
#include <string>
#include <sstream>
#include <drs.h>
#include <unistd.h>
#include <getopt.h>

using namespace std;

DRSBoard* board;

bool verbose = false;

struct sample
{
    float time[1024];
    float data[8][1024];
};

void captureSample() {
    board->StartDomino();
    while(board->IsBusy());
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


int main(int argc, char **argv) {
    int optchar = -1;
    string output_directory("./");
    string output_file_prefix("sample_");
    unsigned int num_samples = 10;
    bool binary_output = false;
    while((optchar = getopt(argc, argv, "d:p:n:hv")) != -1) {
        if(optchar == '?') return 1;
        else if(optchar == 'h') {
            std::cout << "Usage: " << argv[0]
                      << "[-d OUTPUT_DIR] [-p PREFIX] [-n NUM_SAMPLES] [-vbh]\n\nThis *is* usefull help!"
                      << std::endl;
            return 1;
        }
        else if(optchar == 'd') output_directory = optarg;
        else if(optchar == 'p') output_file_prefix = optarg;
        else if(optchar == 'n') {
            istringstream in(optarg);
            in >> num_samples;
        }
        else if(optchar == 'v') verbose = true;
        else if(optchar == 'b') binary_output = true;
    }

    if(output_directory[output_directory.length()-1] != '/')
        output_directory += '/';

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

    // trigger settings
    b->SetTranspMode(1);
    b->EnableTrigger(1, 0);
    b->SetTriggerSource(1); // CH1
//     b->SetTriggerDelayNs(int(1024/0.69));
    b->SetTriggerDelayPercent(100);
    if(verbose) std::cout << "Trigger delay " << b->GetTriggerDelayNs() << "ns, " << b->GetTriggerDelay() << std::endl;

    b->SetTriggerLevel(-0.01, true); // (V), pos. edge == false

    b->SetChannelConfig(0, 1, 4);

    if(verbose) std::cout << "Sampling Rate " << b->GetFrequency() << " GSp/s" << std::endl;
    if(verbose) std::cout << "Record " << num_samples << " frames" << std::endl;
    for(unsigned int i=0; i<num_samples; i++) {
        if(verbose) {
            if(i % 10 == 0)
                std::cout << "\33[2K\rSample " << i << " of " << num_samples << std::flush;
        }
        ostringstream fname;
        fname << output_directory << output_file_prefix << i+1;
        captureSample();
        float time[2048];
        float data[2048];
        board->GetTime(0, board->GetTriggerCell(0), time);
        board->GetWave(0, 0, data);
        if(binary_output) {
            fname << ".dat";
            FILE* f = fopen(fname.str().c_str(), "wb");
            fwrite("#BIN\n", strlen("#BIN\n"), 1, f);
            fwrite(&time, sizeof(time), 1, f);
            fwrite(&data, sizeof(data), 1, f);
            fclose(f);
        }
        else {
            fname << ".csv";
            FILE* f = fopen(fname.str().c_str(), "w");
            fwrite("#TXT\n", strlen("#TXT\n"), 1, f);
            for(int i=0; i<2048; i++) {
                fprintf(f, "%f", time[i]);
                fprintf(f, " %f", data[i]);
                fprintf(f, "\n");
            }
            fclose(f);
        }
    }
    if(verbose) {
        std::cout << "\33[2K\rDone reading samples" << std::endl;
    }

    delete drs;
    return 0;
}
