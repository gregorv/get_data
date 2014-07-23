get_data
========

DRS4 Eval Board data acquisition tool.

Prerequisites
-------------
CMake is required for the installation of get_data. The following libraries are required dependices:
 * libusb-1.0
 * zlib
 * Boost
 * libdrs, read more about it below

The ROOT framework is an optional dependency. It is required to write ROOT files directly from get_data.

`libdrs` is a little issue. It is a static library composed of the source code from the [DRS4 Evaluation Board](http://www.psi.ch/drs/) from PSI and is used to access the hardware. I am not sure what license applies here, so for now I will not distribute it openly via github. Write me an E-Mail for further assistance ;-)

You can find the DRS4 Eval Board source code here:
[here](http://www.psi.ch/drs/SoftwareDownloadEN/drs-5.0.1.tar.gz)

Installation
------------
To get the source code, the easiest way is to clone the github repository

    git clone https://github.com/gregorv/get_data.git

Now, create a build directory and generate the Makefiles

    mkdir get_data/build
    cd get_data/build
    cmake ..

If you want to use ROOT, the `ROOTSYS` environment variable has to be set correctly, what should be the case if you installed ROOT correctly. If you get a message that ROOT was not found while CMake run, locate the `root-config` program comming with ROOT. You can try to use `type root-config` or similar. When you got the path, e.g. _/usr/root/bin/_, just run CMake again like this

    cmake .. -DROOT_CONFIG_SEARCHPATH=/usr/root/bin
    
Now there should be the message that ROOT was found.

To compile and install, just run

    make
    sudo make install

Verify that your setup is correct by running `get_data -h`, you should get the help text. It will also tell you whether ROOT-support is actually compiled into the executable.

How To Use
----------
`get_data -h`

;-)


Licensing
---------

* get_data ist distributed under the GPL3 license.
* FindROOT.cmake by <F.Uhlig@gsi.de> http://fairroot.gsi.de
* FindLibUSB.cmake by Michal Cihar, <michal@cihar.com>
