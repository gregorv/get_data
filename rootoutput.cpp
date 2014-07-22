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
 : m_frame_counter(0)
{
}

RootOutput::~RootOutput()
{
}

bool RootOutput::init_stream()
{
    m_file = std::make_shared<TFile>(filename.c_str(), "create");
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
    std::ostringstream recordTimestampsText;
    recordTimestampsText << record_time.count();
    auto timestampText = std::make_shared<TObjString>(recordTimestampsText.str().c_str());
    timestampText->Write("record_timestamps");

    auto graph = std::make_shared<TGraph>(frames_per_sample, time, data);
    std::ostringstream name;
    name << "frame " << m_frame_counter++;
    graph->SetTitle(name.str().c_str());
    graph->Write("frame");

//     m_recordTimestampsText << name << record_time.count() << " ns\n";
    return true;
}

bool RootOutput::write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data)
{
    std::ostringstream recordTimestampsText;
    recordTimestampsText << record_time.count();
    auto timestampText = std::make_shared<TObjString>(recordTimestampsText.str().c_str());
    timestampText->Write("record_timestamps");

    auto multi = std::make_shared<TMultiGraph>();
    for(auto ch: ch_config) {
        if(ch == -1) {
            continue;
        }
        std::ostringstream col_name;
        col_name << "Channel " << ch;
        auto graph = std::make_shared<TGraph>(frames_per_sample, time, data[ch]);
        multi->Add(graph.get(), col_name.str().c_str());
    }
    std::ostringstream frame_name;
    frame_name << "frame_" << m_frame_counter++;
    multi->SetTitle(frame_name.str().c_str());
    multi->Write("frame");

    m_recordTimestampsText << record_time.count() << " ns\n";
    return true;
}

bool RootOutput::finalize()
{
//     auto timestampText = std::make_shared<TObjString>(m_recordTimestampsText.str().c_str());
//     timestampText->Write("record_timestamps");
    m_file->Write();
    m_file->Close();
    m_file.reset();
    return true;
}



