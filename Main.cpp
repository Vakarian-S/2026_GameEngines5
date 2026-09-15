#include <memory>
#include <iostream>
#include <Vector.h>
#include <vector>

#include "utils/MemoryMonitor.h"


int main(int argc, char* argv[])
{
    EnableConsoleColors();

    // The runtime allocates before main, so measure against this point
    const MemoryStats baseline = MemoryMonitor::GetStats();

    constexpr int n = 1;

    std::cout << MemColor::Section << "-- Operators on new int(42) --" << MemColor::Reset << std::endl;
    int* single = new int(42);
    delete single;
    std::cout << "\n" << std::endl;

    std::cout << MemColor::Section << "-- Operators on new int[n], Non-explicit sized call --" << MemColor::Reset <<
        std::endl;
    auto* explicitArr = new int[n];
    delete [] explicitArr;
    std::cout << "\n" << std::endl;

    std::cout << MemColor::Section << "-- Operators on new int[n], explicit sized call --" << MemColor::Reset <<
        std::endl;
    auto* explicitArr2 = new int[n];
    operator delete[](explicitArr2, n * sizeof(int));
    std::cout << "\n" << std::endl;

    std::cout << MemColor::Section << "-- Operators on struct Dummy --" << MemColor::Reset << std::endl;
    struct Dummy
    {
        int value;
        int volue;
        int vilue;
        int velue;

        ~Dummy()
        {
        }
    };

    const Dummy* boxedArr = new Dummy[n];
    delete[] boxedArr;
    std::cout << "\n" << std::endl;

    int* ptr = new int[1024];
    delete[] ptr;
    /// answer should be 4096 bytes

    std::cout << MemColor::Section << "-- Allocating Vectors --" << MemColor::Reset << std::endl;
    {
        std::vector<MATH::Vec3> vertices;
        vertices.reserve(1000);
        for (int i = 0; i < 1000; i++)
        {
            vertices.push_back(MATH::Vec3());
        }
    } // vertices frees its memory here, so the report shows only real leaks
    std::cout << "\n" << std::endl;

    MemoryMonitor::Report(baseline);

    return 0;
}
