// FlasherSearcher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#include "Searcher.h"

// Conan
// https://docs.conan.io/en/latest/using_packages/workflows.html
// https://docs.conan.io/en/latest/reference/generators/visualstudiomulti.html#visual-studio-multi

// C++ prebuild target hooks (for debug)
// https://docs.microsoft.com/en-us/visualstudio/msbuild/how-to-extend-the-visual-studio-build-process?view=vs-2015&redirectedfrom=MSDN

int test1();

int main()
{
    // return test1();

    Searcher s("FOpaqueVelocityMeshProcessor", ".*\\.(cpp|c|h|hpp)$");
    // s.Search("D:\\Repository\\godot", ".*\.(cpp|c|h|hpp)$");
    s.Search("C:\\UE4\\UE_4.25\\Engine");

    return 0;
}

int test1()
{
    re2::RE2 re("test");
    if (!re.ok())
    {
        std::cerr << "Couldn't compile regex!" << std::endl;
        return -1;
    }

    std::cout << re2::RE2::FullMatch("ruby:1234", re);

    HANDLE hfile = ::CreateFileA("D:\\Repository\\FlasherSearcher\\FileToSearch.txt", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if (hfile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Couldn't open file!" << std::endl;
        return -1;
    }

    DWORD size = ::GetFileSize(hfile, 0);

    HANDLE mapping = ::CreateFileMappingA(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* data = static_cast<char*>(::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size));

    ::CloseHandle(mapping);
    ::CloseHandle(hfile);


    std::cout << data << std::endl;

    //bool r = re2::RE2::FullMatch(data, re);
    bool r = re2::RE2::PartialMatch(data, re);
    std::cout << "is match: " << r << std::endl;

    return 0;
}

