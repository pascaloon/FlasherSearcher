// FlasherSearcher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

// https://dfs-minded.com/build-integrate-re2-c-project-windows/
#include <re2/re2.h>

// https://www.flipcode.com/archives/Faster_File_Access_With_File_Mapping.shtml
// http://imadiversion.co.uk/2016/12/08/c-17-and-memory-mapped-io/
#include <windows.h>

// C++ prebuild target hooks (for debug)
// https://docs.microsoft.com/en-us/visualstudio/msbuild/how-to-extend-the-visual-studio-build-process?view=vs-2015&redirectedfrom=MSDN

int main()
{
    std::cout << "Hello World!\n";

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

}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
