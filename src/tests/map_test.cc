#include "../include/types.h"
#include "../include/zstore_controller.h"
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

using namespace std;

// Type definitions
using ObjectKey = std::string;
using ObjectKeyHash = unsigned char *;
using TargetDev = std::string;
using Lba = uint64_t;
using Length = uint32_t;

// Device types and map entry
using TargetLbaTuple = std::tuple<TargetDev, Lba, Length>;
using MapEntry = std::tuple<TargetLbaTuple, TargetLbaTuple, TargetLbaTuple>;
// using MapType = boost::container::concurrent_flat_map<ObjectKeyHash,
// MapEntry>;

// Helper function to write a single string to the file
void writeString(std::ofstream &outFile, const std::string &str)
{
    size_t size = str.size();
    outFile.write(reinterpret_cast<const char *>(&size), sizeof(size));
    outFile.write(str.c_str(), size);
}

// Helper function to read a single string from the file
std::string readString(std::ifstream &inFile)
{
    size_t size;
    inFile.read(reinterpret_cast<char *>(&size), sizeof(size));
    std::string str(size, '\0');
    inFile.read(&str[0], size);
    return str;
}

// Function to write the map to a file
void writeMapToFile(const ZstoreMap &map, const std::string &filename)
{
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing: " << filename
                  << std::endl;
        return;
    }

    size_t mapSize = 0;
    map.visit_all([&mapSize](auto &x) { ++mapSize; });

    // Write the map size
    outFile.write(reinterpret_cast<const char *>(&mapSize), sizeof(mapSize));

    // Iterate over each key-value pair and write them to the file
    map.visit_all([&outFile](auto &x) {
        // Serialize the key
        outFile.write(reinterpret_cast<const char *>(x.first),
                      32); // Assuming 32-byte hash

        // Serialize the entry
        auto writeTargetLbaTuple = [&outFile](const TargetLbaTuple &tuple) {
            writeString(outFile, std::get<0>(tuple)); // TargetDev
            outFile.write(reinterpret_cast<const char *>(&std::get<1>(tuple)),
                          sizeof(Lba)); // Lba
            outFile.write(reinterpret_cast<const char *>(&std::get<2>(tuple)),
                          sizeof(Length)); // Length
        };
        writeTargetLbaTuple(std::get<0>(x.second));
        writeTargetLbaTuple(std::get<1>(x.second));
        writeTargetLbaTuple(std::get<2>(x.second));
    });
}

// Function to read the map from a file
ZstoreMap readMapFromFile(const std::string &filename)
{
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading: " << filename
                  << std::endl;
        return {};
    }

    ZstoreMap map;
    size_t mapSize;
    inFile.read(reinterpret_cast<char *>(&mapSize), sizeof(mapSize));

    for (size_t i = 0; i < mapSize; ++i) {
        // Deserialize the key
        unsigned char *key = new unsigned char[32];
        inFile.read(reinterpret_cast<char *>(key), 32);

        // Deserialize the entry
        auto readTargetLbaTuple = [&inFile]() -> TargetLbaTuple {
            std::string dev = readString(inFile);
            Lba lba;
            Length length;
            inFile.read(reinterpret_cast<char *>(&lba), sizeof(Lba));
            inFile.read(reinterpret_cast<char *>(&length), sizeof(Length));
            return std::make_tuple(dev, lba, length);
        };

        MapEntry entry = std::make_tuple(
            readTargetLbaTuple(), readTargetLbaTuple(), readTargetLbaTuple());
        map.emplace(key, entry);
    }

    return map;
}

int main()
{
    ZstoreMap mMap;

    // Sample data
    unsigned char key1[32] = "sample_key_1";
    unsigned char key2[32] = "sample_key_2";
    MapEntry entry1 = {std::make_tuple("device1", 1000, 256),
                       std::make_tuple("device2", 2000, 512),
                       std::make_tuple("device3", 3000, 128)};
    MapEntry entry2 = {std::make_tuple("device1", 1500, 128),
                       std::make_tuple("device2", 2500, 256),
                       std::make_tuple("device3", 3500, 64)};
    mMap.emplace(key1, entry1);
    mMap.emplace(key2, entry2);

    // Write the map to a file
    std::string filename = "mMap_data.bin";
    writeMapToFile(mMap, filename);

    // Read the map back from the file
    ZstoreMap loadedMap = readMapFromFile(filename);

    // Print the loaded map
    loadedMap.visit_all([](auto &x) {
        std::cout << "Key: " << x.first << "\n";
        std::tuple<
            std::tuple<std::basic_string<char>, unsigned long, unsigned int>,
            std::tuple<std::basic_string<char>, unsigned long, unsigned int>,
            std::tuple<std::basic_string<char>, unsigned long, unsigned int>>
            big_tuple = x.second;
        auto first = std::get<0>(big_tuple);
        std::cout << "  Device: " << std::get<0>(first)
                  << ", LBA: " << std::get<1>(first)
                  << ", Length: " << std::get<2>(first) << "\n";
        auto second = std::get<1>(big_tuple);
        std::cout << "  Device: " << std::get<0>(second)
                  << ", LBA: " << std::get<1>(second)
                  << ", Length: " << std::get<2>(second) << "\n";
        auto third = std::get<2>(big_tuple);
        std::cout << "  Device: " << std::get<0>(third)
                  << ", LBA: " << std::get<1>(third)
                  << ", Length: " << std::get<2>(third) << "\n";
    });

    return 0;
}
