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
    else {
        std::cerr << "No DRS Eval board can be accessed, aborting!" << std::endl;
        return 1;
    }
    DRSBoard* b = board = drs->GetBoard(0);
    std::cout << "Board type " << b->GetBoardType() << std::endl;
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
    
    b->SetTriggerLevel(-0.01, true); // (V), pos. edge == false
    b->SetTriggerDelayNs(0); // trigger delay (ns)
    
    if(verbose) std::cout << "Sampling Rate " << b->GetFrequency() << " GSp/s" << std::endl;
    if(verbose) std::cout << num_samples << " to be taken" << std::endl;
    for(unsigned int i=0; i<num_samples; i++) {
        sample s;
        ostringstream fname;
        fname << output_directory << output_file_prefix << i+1;
        getSample(s);
        if(binary_output) {
            fname << ".dat";
            FILE* f = fopen(fname.str().c_str(), "wb");
            fwrite(&s, sizeof(s), 1, f);
            fclose(f);
        }
        else {
            fname << ".csv";
            FILE* f = fopen(fname.str().c_str(), "w");
            for(int i=0; i<1024; i++) {
                fprintf(f, "%f", s.time[i]);
                for(int ch=0; ch<8; ch++)
                    fprintf(f, " %f", s.data[ch][i]);
                fprintf(f, "\n");
            }
            fclose(f);
        }
    }
    
    delete drs;
    return 0;
}
