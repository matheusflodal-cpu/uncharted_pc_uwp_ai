// SPDX-License-Identifier: GPL-2.0-or-later
// Desktop-only harness for byte parity against the independent Python preparer.
#include "prepare_game.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using UnchartedPrepare::Bytes;
static Bytes read(const char* path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    auto size=file.tellg();
    if(!file||size<=0||size>(512ll<<20)) throw std::runtime_error("unreadable or oversized input");
    Bytes data(static_cast<size_t>(size)); file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(data.data()),size)) throw std::runtime_error("short read");
    return data;
}
static void write(const char* path,const Bytes& data) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if(!file.write(reinterpret_cast<const char*>(data.data()),data.size())) throw std::runtime_error("short write");
}
int main(int argc,char** argv) {
    if(argc!=7) return 2;
    try {
        const auto files=UnchartedPrepare::Prepare({read(argv[1]),read(argv[2]),read(argv[3]),read(argv[4])});
        write(argv[5],files.boot); write(argv[6],files.content);
        std::cout<<"Native preparation: PASS; boot="<<files.boot.size()<<" content="<<files.content.size()<<'\n';
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
