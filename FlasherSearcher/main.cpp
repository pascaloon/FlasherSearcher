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

int main (int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Expected usage: [FILE_FILTER] [QUERY]";
        return -1;
    }

    const size_t BUFFER_SIZE = 512; 
    char cwd_buffer[BUFFER_SIZE];
    size_t cwdSize = ::GetCurrentDirectoryA(BUFFER_SIZE, cwd_buffer);
    
    std::string cwd(cwd_buffer, cwdSize);
    std::string fileFilter = argv[1];
    std::string query = argv[2];

    // std::cout << "CWD: " << cwd << std::endl;
    // std::cout << "Filter: " << fileFilter << std::endl;
    // std::cout << "Query: " << query << std::endl;
    
    Searcher s(query, fileFilter);
    s.Search(cwd);

    return 0;
}
