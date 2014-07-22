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

#include "detectorcontrol.h"

#include <iostream>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

bool DetectorControl::send_msg(std::string message)
{
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

std::string DetectorControl::recv_line()
{
    std::string line("");
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

DetectorControl::DetectorControl(std::string sfile)
: sock(0), stream(0), socket_file(sfile)
{
}
DetectorControl::~DetectorControl()
{
    disconnect_control();
}

bool DetectorControl::connect_control()
{
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

void DetectorControl::disconnect_control()
{
    close(sock);
    sock = 0;
}

void DetectorControl::setTsoll(float T_soll)
{
    std::stringstream line;
    line << "T_soll=" << T_soll << "\n";
    send_msg(line.str());
}

float DetectorControl::getTsoll()
{
}

float DetectorControl::getTist()
{
    send_msg("T_detector\n");
    std::istringstream is(recv_line());
    float temperature;
    is >> temperature;
    return temperature;
}

bool DetectorControl::temperatureStable()
{
    send_msg("stable\n");
    std::string line = recv_line();
    return line.find("True") != std::string::npos;
}

void DetectorControl::interruptMeasurement()
{
    send_msg("interrupt\n");
}

void DetectorControl::contineMeasurement()
{
    send_msg("continue\n");
}

void DetectorControl::hold_start()
{
    send_msg("hol");
}

void DetectorControl::hold_end()
{
    send_msg("d\n");
}
