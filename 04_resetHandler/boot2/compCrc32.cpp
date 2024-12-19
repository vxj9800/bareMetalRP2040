#include <fstream>
#include <iostream>
#include <filesystem>

#include "CRCpp/inc/CRC.h"

int main(int argc, char *argv[])
{
    std::filesystem::path binFilePath;
    std::ifstream binFile;

    // Bail if enough arguments are not provided
    if (argc < 2)
    {
        std::cout << "An input file with .bin extension must be provided. Exiting ..." << std::endl;
        return 1;
    }

    // Get the path of the .bin file
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
        binFile.close();
        return 1;
    }

    // All good, now load the file contents into an array
    unsigned char binFileData[252] = {0};
    binFile.read((char *)binFileData, 252);
    binFile.close();

    // Calculate CRC32 for the 252 bytes of data
    unsigned char crc[4] = {0};
    *reinterpret_cast<std::uint32_t*>(crc) = CRC::Calculate(binFileData, 252, CRC::CRC_32_MPEG2());

    // Output the contents of the crc array to a .c file
    std::ofstream cppFile(binFilePath.replace_filename("crc").replace_extension("c"));
    cppFile << "__attribute__((section(\".crc\"))) unsigned char crc[4] = {"
            << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)crc[0] << ", "
            << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)crc[1] << ", "
            << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)crc[2] << ", "
            << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)crc[3] << "};";
    cppFile.close();

    return 0;
}