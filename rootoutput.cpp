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

#include "rootoutput.h"

#include <TGraph.h>
#include <TMultiGraph.h>
#include <TObjString.h>

RootOutput::RootOutput()
 : m_data_graphs{nullptr, nullptr, nullptr, nullptr}
{
    for(size_t i=0; i<4; i++) {
        m_data_graphs[i] = new TGraph;
    }
}

RootOutput::~RootOutput()
{
}

bool RootOutput::init_stream()
{
    m_file = std::make_shared<TFile>(filename.c_str(), "create");
    m_tree = std::make_shared<TTree>("data", "data");
    m_tree->Branch("t_0", &m_record_timestamp, "t_0/L");
    m_tree->Branch("ch1", "TGraph", &m_data_graphs[0]);
    m_tree->Branch("ch2", "TGraph", &m_data_graphs[1]);
    m_tree->Branch("ch3", "TGraph", &m_data_graphs[2]);
    m_tree->Branch("ch4", "TGraph", &m_data_graphs[3]);
    return m_file.get() != nullptr;
}

bool RootOutput::write_header()
{
    std::ostringstream text;
    text << command_line;
    auto config_text = std::make_shared<TObjString>(text.str().c_str());
    config_text->Write("record_settings");
    return true;
}

bool RootOutput::write_frame(const nanoseconds& record_time, float* time, float* data)
{
    std::array<float*, 4> data_array{ {data, nullptr, nullptr, nullptr} };
    std::array< int, 4> my_ch_config{ {0, -1, -1, -1} };
    return write_frame(record_time, time, data_array, my_ch_config);
}

bool RootOutput::write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data)
{
    return write_frame(record_time, time, data, ch_config);
}

bool RootOutput::write_frame(const nanoseconds& record_time, float* time,
                             const std::array< float*, 4  >& data,
                             std::array< int, 4  > my_ch_config)
{
    m_record_timestamp = record_time.count();
    for(auto ch: ch_config) {
        if(ch == -1) {
            continue;
        }
        if(m_data_graphs[ch]->GetN() != frames_per_sample) {
            m_data_graphs[ch]->Set(frames_per_sample);
        }
        std::ostringstream col_name;
        col_name << "Channel " << ch;
        for(size_t i = 0; i<frames_per_sample; i++) {
            m_data_graphs[ch]->SetPoint(i, time[i], data[ch][i]);
        }
    }
    m_tree->Fill();
    return true;
}

bool RootOutput::finalize()
{
//     m_tree->Write();  <- don't! Written automatically
    m_file->Write();
    m_tree.reset();  // if the tree is not deleted, closing the file will crash!

    m_file->Close();
    m_file.reset();
    return true;
}



