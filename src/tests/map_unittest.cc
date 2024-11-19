#include "../include/types.h"
#include "../include/zstore_controller.h"
#include "../zstore_controller.cc"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using ObjectKeyHash = unsigned long long;
using TargetDev = std::string;
using Lba = u64;
using Length = unsigned int;
using TargetLbaTuple = std::tuple<TargetDev, Lba, Length>;
using MapEntry = std::tuple<TargetLbaTuple, TargetLbaTuple, TargetLbaTuple>;
using ZstoreMap = boost::concurrent_flat_map<ObjectKeyHash, MapEntry>;

ZstoreMap mMap;

// Write ZstoreMap to a file
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
        ObjectKeyHash key = x.first;
        outFile.write(reinterpret_cast<char *>(&key),
                      sizeof(key)); // Assuming 32-byte hash

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
    log_debug("write entry done");
}

// Read ZstoreMap from a file
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
        ObjectKeyHash key = 0;
        inFile.read(reinterpret_cast<char *>(&key), sizeof(key));

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
    // Populate map with sample data
    mMap.emplace(12345, std::make_tuple(std::make_tuple("DeviceA", 100, 50),
                                        std::make_tuple("DeviceB", 200, 25),
                                        std::make_tuple("DeviceC", 300, 75)));

    mMap.emplace(67890, std::make_tuple(std::make_tuple("DeviceD", 400, 100),
                                        std::make_tuple("DeviceE", 500, 60),
                                        std::make_tuple("DeviceF", 600, 80)));

    log_info("Map size: {}", mMap.size());

    // Write to file
    writeMapToFile(mMap, "zstore_map.txt");
    log_info("write map to file");

    // Clear map and read back from file
    ZstoreMap loadedMap = readMapFromFile("zstore_map.txt");
    log_info("load map from file");

    assert(mMap.size() == loadedMap.size() && "Map size mismatch");
    assert(mMap == loadedMap && "Map contents mismatch");

    // Verify contents
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
