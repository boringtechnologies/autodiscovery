grpc-roc demo client-server


simple build flow

install gRPC as described https://github.com/grpc/grpc/blob/master/BUILDING.md

$ [sudo] apt-get install build-essential autoconf libtool pkg-config
$ [sudo] apt-get install cmake
$ [sudo] apt-get install clang libc++-dev

$ git clone -b RELEASE_TAG_HERE https://github.com/grpc/grpc
$ cd grpc
$ git submodule update --init

$ mkdir build && cd build
$ cmake ..
$ make
$ [sudo] make install

build sources

$ git clone https://github.com/Boring-App/autodiscovery.git
$ cd autodiscovery
$ mkdir build && cd build
$ cmake ..
$ make

Done!
