#include "decompression/decompressor.hpp"

#include <lz4.h>
#include <iostream>
#include <fstream>
#include <cstring>

enum {
    MESSAGE_MAX_BYTES   = (1024 * 512),
    RING_BUFFER_BYTES   = MESSAGE_MAX_BYTES * 8 + MESSAGE_MAX_BYTES,
    DEC_BUFFER_BYTES    = RING_BUFFER_BYTES + MESSAGE_MAX_BYTES
};

typedef struct {
    uint64_t inputFileCurrentOffset;
    uint64_t outputFileCurrentOffset;
    uint32_t decompressionOffset;
    uint32_t decompressedBytes;
    char decompressedBuffer[DEC_BUFFER_BYTES];
} LZ4_decoderIntermediateState_t;


void saveDecompressionState(const LZ4_decoderIntermediateState_t& state, const char* stateFileName)
{
    // Open the state file in binary mode
    std::ofstream stateFile(stateFileName, std::ios::binary);
    if (!stateFile.is_open()) {
        std::cerr << "Error opening state file: " << stateFileName << std::endl;
        stateFile.close();
        return;
    } else {
        stateFile.write(reinterpret_cast<const char*>(&state), sizeof(state));
    }
}

void loadDecompressionState(LZ4_decoderIntermediateState_t& state, const char* stateFileName)
{
    // Open the state file in binary mode
    std::ifstream stateFile(stateFileName, std::ios::binary);
    if (!stateFile.is_open()) {
        std::cerr << "Error opening state file: " << stateFileName << std::endl;
        stateFile.close();
        return;
    } else {
        stateFile.read(reinterpret_cast<char*>(&state), sizeof(state));
    }    
}

void removeDecompressionState(const char* stateFileName) {
    remove(stateFileName);
}

void decompressFile(const char* inputFileName, const char* outputFileName, const char* stateFileName)
{
    bool isResumeUpdate = false;
    bool isSimulatedExitPerformed = false;

    // Open the input file in binary mode
    std::ifstream inputFile(inputFileName, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error opening input file: " << inputFileName << std::endl;
        return;
    }

    // Open the output file in binary mode
    std::ofstream outputFile(outputFileName, std::ios::binary | std::ios::app);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening output file: " << outputFileName << std::endl;
        inputFile.close();
        return;
    }

    // declare LZ4 decompression context
    LZ4_streamDecode_t* const lz4StreamDecode = LZ4_createStreamDecode();

    // declare decompression state
    LZ4_decoderIntermediateState_t decompressionState{0,0,{},0,0};

    std::ifstream stateFile(stateFileName);
    if(stateFile)
    {
        std::cout << "Resuming decompression of file :: " << inputFileName << std::endl;
        loadDecompressionState(decompressionState, stateFileName);
        lz4StreamDecode->internal_donotuse.prefixSize = decompressionState.decompressedBytes;
        isResumeUpdate = true;
        isSimulatedExitPerformed = true;
    } else {
        std::cout << "Started performing decompression of file :: " << inputFileName << std::endl;
        // stateFileName is not present, starting decompression from beginning
        // Setup LZ4 decompression context
        LZ4_setStreamDecode(lz4StreamDecode, nullptr, 0);
        outputFile.close();
        outputFile.open(outputFileName, std::ios::binary); // re-opening file in non-apending mode
    }

    //declare buffers
    char* compressedBuffer = new char[LZ4_COMPRESSBOUND(MESSAGE_MAX_BYTES)];

    // compressed bytes
    uint32_t compressedBlockSize = 0; // compressed bytes in each block will not exceed LZ4_COMPRESSBOUND(MESSAGE_MAX_BYTES) i.e 1044.

    // seek position for input and output file
    inputFile.seekg(decompressionState.inputFileCurrentOffset);
    outputFile.seekp(decompressionState.outputFileCurrentOffset);

    while(true)
    {   
        // read size of data in compressed data block
        inputFile.read(reinterpret_cast<char*>(&compressedBlockSize), sizeof(compressedBlockSize));

        if(0 == compressedBlockSize)
        {
            std::cout << "End Mark detected." << std::endl;
            break;
        }

        // adjust pointer
        char* const decompressedBufferPtr = &decompressionState.decompressedBuffer[decompressionState.decompressionOffset];

        if(isResumeUpdate)
        {
            lz4StreamDecode->internal_donotuse.prefixEnd = (uint8_t*)decompressedBufferPtr;
            isResumeUpdate = false;
        }

        // read "compressedBlockSize" number of bytes from data block
        inputFile.read(compressedBuffer, compressedBlockSize);

        decompressionState.decompressedBytes = LZ4_decompress_safe_continue(lz4StreamDecode, compressedBuffer, decompressedBufferPtr, compressedBlockSize, MESSAGE_MAX_BYTES);

        if (decompressionState.decompressedBytes < 0) {
            std::cerr << "Error during decompression" << std::endl;
            break;
        }

        // write "decompressed data" from decompressedBufferPtr to output file
        outputFile.write(decompressedBufferPtr, decompressionState.decompressedBytes);
        decompressionState.decompressionOffset += decompressionState.decompressedBytes;

        if ((size_t)decompressionState.decompressionOffset >= DEC_BUFFER_BYTES - MESSAGE_MAX_BYTES) 
        {
            decompressionState.decompressionOffset = 0;
        }

        // if(!isSimulatedExitPerformed) // DEBUG_VISHAL : set condition to decide frequency to store states
        // {
        //     // update current state in file :: stateFileName
        //     decompressionState.outputFileCurrentOffset = outputFile.tellp();
        //     decompressionState.inputFileCurrentOffset = inputFile.tellg();
        //     saveDecompressionState(decompressionState, stateFileName);
        // }

        // perform simulated exit
        // if((isSimulatedExitPerformed == false) && (inputFile.tellg() >= 1024 * 1024 * 1024))
        // {
        //     std::cerr << "performing simulated exit !!!" << std::endl;
        //     return;
        // }
    }

    // Clean up
    delete[] compressedBuffer;
    inputFile.close();
    outputFile.close();
    removeDecompressionState(stateFileName);

    std::cout << "Decompression is successful. Output written to: " << outputFileName << std::endl;
}


/**
 * Following data needed to resume the Decompression - 
 * 
 * 1. lz4StreamDecode
 *        |-> internal_donotuse
 *                |-> prefixSize
 *                |-> prefixEnd
 * 
 * 2. inputFile.tellg() -> inputFile current position till where data is already processed 
 *    it's data type should be of uint64_t, to accomodate more than 50 GB of data (NAVI DB Compressed file size)
 * 
 * 3. outputFile.tellp() -> outputFile current position till where decompressed data is already written
 *    it's data type should be of uint64_t, to accomodate more than 70 GB of data (NAVI DB raw file size)
 *    Note : but, this is not needed in project as streaming will be used
 * 
 * 4. decompressedBuffer -> copy of decompressed buffer of size DEC_BUFFER_BYTES (ex : 10240)
 * 
 * 5. decompressionOffset -> pointer poisition of decompressedBufferPtr
*/

/**
 * Tunable variables to ensure robustness and performance :
 * 
 * 1. condition to decide how frequently saveDecompressionState() should be called.
 * 
 * 2. 
*/