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

#ifndef ROOTOUTPUT_H
#define ROOTOUTPUT_H

#include "datastream.h"

#include <TFile.h>
#include <memory>

class RootOutput : public DataStream
{
public:
    RootOutput();
    virtual ~RootOutput();

    virtual bool write_frame(const nanoseconds& record_time, float* time, float* data);
    virtual bool write_frame(const nanoseconds& record_time, float* time, const std::array<float*, 4>& data);
    virtual bool write_header();
    virtual bool finalize();

    virtual std::string get_file_extension() const { return std::string(".root"); }

protected:
    virtual bool init_stream();

private:
    std::shared_ptr<TFile> m_file;
    std::ostringstream m_recordTimestampsText;
    size_t m_frame_counter;


};

#endif // ROOTOUTPUT_H
