#include "pch.h"
#include "Searcher.h"

Searcher::Searcher(std::string regex)
    : _regex(regex)
{
    
}

void Searcher::Search(std::string searchDir, std::string fileFilter)
{
    re2::RE2 fileRegex(fileFilter);

    int results = 0;
    
    std::vector<std::string> dirs;
    dirs.reserve(100);
    dirs.push_back(searchDir);
    
    for (size_t i = 0; i < dirs.size(); ++i)
    {
        // vvvvvvvv copy value or crash on resize :) 
        std::string dir = dirs[i];
        // std::cout << "Searching: " << dir << std::endl;
        std::string dirFilter = dir + "\\*";

        WIN32_FIND_DATAA ffd;
        // Skip "." 
        HANDLE hfind = FindFirstFileA(dirFilter.c_str(), &ffd);
        if (hfind == INVALID_HANDLE_VALUE)
        {
            std::cerr << "Invalid path!" << std::endl;
            return;
        }
        
        // Skip ".."
        FindNextFileA(hfind, &ffd);


        while (FindNextFileA(hfind, &ffd))
        {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                dirs.push_back(dir + "\\" + std::string(ffd.cFileName));
            }
            else if (re2::RE2::FullMatch(ffd.cFileName, fileRegex))
            {
                std::string fullFileName = dir + "\\" + std::string(ffd.cFileName);
                HANDLE hfile = ::CreateFileA(fullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
                if (hfile == INVALID_HANDLE_VALUE)
                {
                    std::cerr << "Couldn't open file!" << std::endl;
                    continue;
                }

                DWORD size = ::GetFileSize(hfile, 0);

                HANDLE mapping = ::CreateFileMappingA(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
                char* data = static_cast<char*>(::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size));

                if (re2::RE2::PartialMatch(std::string(data, size), _regex))
                {
                    std::cout << fullFileName << std::endl;
                    ++results;
                }

                ::CloseHandle(mapping);
                ::CloseHandle(hfile);
            }
        }

        ::FindClose(hfind);
    }
    std::cout << "Results: " << results << std::endl;

}

