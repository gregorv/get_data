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

#ifndef DETECTORCONTROL_H
#define DETECTORCONTROL_H

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>

class DetectorControl {
private:
    int sock;
    FILE* stream;
    std::string socket_file;

    bool send_msg(std::string message);
    std::string recv_line();

public:
    DetectorControl(std::string sfile);
    ~DetectorControl();
    bool connect_control();
    void disconnect_control();
    void setTsoll(float T_soll);
    float getTsoll();
    float getTist();
    bool temperatureStable();
    void interruptMeasurement();
    void contineMeasurement();
    void hold_start();
    void hold_end();
};

#endif // DETECTORCONTROL_H
