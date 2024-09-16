#include <fstream>
#include <iostream>
#include <filesystem>

#include "CRCpp/inc/CRC.h"

int main(int argc, char *argv[])
{
    std::filesystem::path binFilePath;
    std::ifstream binFile;

    // Make sure that an argument is entered
    if (argc > 1)
    {
        binFilePath = argv[1];

        // Bail if file name doesn't have .bin extension
        if (binFilePath.extension() != ".bin")
        {
            std::cout << "The input file must have .bin extension. Exiting ..." << std::endl;
            return 1;
        }

        // Bail if the file doesn't exist
        if (!std::filesystem::exists(binFilePath))
        {
            std::cout << "Could not locate file: " << binFilePath << ". Exiting ..." << std::endl;
            return 1;
        }

        // Open the file
        binFile.open(binFilePath, std::ios::binary);
        binFile.seekg(0, std::ios::end);
        size_t binFileSize = binFile.tellg();
        binFile.seekg(0, std::ios::beg);

        // Bail if it is > 252 bytes in size
        if (binFileSize > 252)
        {
            std::cout << "The input must be 252 Bytes in size at max. Exiting ..." << std::endl;
            return 1;
        }

        // All good, now load the file into an array
        unsigned char binFileData[256] = {0};
        binFile.read((char *)binFileData, 252);
        binFile.close();

        // Calculate CRC32 for the first 252 bytes of data
        unsigned char crc[4] = {0};
        *reinterpret_cast<std::uint32_t*>(crc) = CRC::Calculate(binFileData, 252, CRC::CRC_32_MPEG2());

        // Place the crc value at the end of the array
        for (size_t i = 0; i < 4; ++i)
            binFileData[252 + i] = crc[i];

        // Output the contents of the array to a .cpp file
        std::ofstream cppFile(binFilePath.replace_filename("boot2").replace_extension("c"));
        cppFile << "unsigned char boot2[256] __attribute__((section(\".boot2\"))) = {" << std::endl;
        for (size_t i = 0; i < 256; ++i)
            cppFile << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)binFileData[i] << ((i + 1) % 16 == 0 ? ",\n" : ",");
        cppFile << "};";
        cppFile.close();
    }
    else
    {
        std::cout << "An input file with .bin extension must be provided. Exiting ..." << std::endl;
        return 1;
    }

    return 0;
}