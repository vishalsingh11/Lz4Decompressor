#include "decompression/decompressor.hpp"

#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

int main()
{
    std::chrono::_V2::system_clock::time_point start, stop;

    std::string compressedFileName = "apricot-image-ui-adelegg.ext4.lz4";
    std::string outputFileName = "apricot-image-ui-adelegg_lz4.ext4";
    std::string stateFileName = "apricot-image-ui-adelegg_lz4_state.bin";

    start = high_resolution_clock::now();
    decompressFile(compressedFileName.c_str(), outputFileName.c_str(), stateFileName.c_str());
    stop = high_resolution_clock::now();

    std::cout << "Time taken to decompress :: " << compressedFileName << " with lz4 is :: " << duration_cast<minutes>(stop - start).count() << " minutes" << std::endl;

    compressedFileName = "apricot-image-ui-adelegg.ext4.lz4hc";
    outputFileName = "apricot-image-ui-adelegg_lz4hc.ext4";
    stateFileName = "apricot-image-ui-adelegg_lz4hc_state.bin";

    start = high_resolution_clock::now();
    decompressFile(compressedFileName.c_str(), outputFileName.c_str(), stateFileName.c_str());
    stop = high_resolution_clock::now();

    std::cout << "Time taken to decompress :: " << compressedFileName << " with lz4 is :: " << duration_cast<minutes>(stop - start).count() << " minutes" << std::endl;

    return 0;
}