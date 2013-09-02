#include <iostream>
#include <drs.h>

DRSBoard* board;

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
    DRS* drs = new DRS();
    if(drs->GetNumberOfBoards() > 0)
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
    
    std::cout << "Sampling Rate " << b->GetFrequency() << " GSp/s" << std::endl;
    
    sample s;
    getSample(s);
    FILE* f = fopen("sample.csv", "w");
    for(int i=0; i<1024; i++) {
        fprintf(f, "%f", s.time[i]);
        for(int ch=0; ch<8; ch++)
            fprintf(f, " %f", s.data[ch][i]);
        fprintf(f, "\n");
    }
    fclose(f);
    
    delete drs;
    return 0;
}
