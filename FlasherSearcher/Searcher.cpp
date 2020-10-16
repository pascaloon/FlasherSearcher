#include "pch.h"
#include "Searcher.h"


Searcher::Searcher(std::string regex, std::string fileFilter)
    : _regex(regex)
    , _fileRegex(fileFilter)
{
    
}

void Searcher::Search(std::string searchDir)
{
    concurrency::task_group tasks;

    tasks.run([&]{SearchInternal(searchDir, tasks);});

    tasks.wait();

    std::cout << "DONE!" << std::endl;
}

void Searcher::SearchInternal(std::string searchDir, concurrency::task_group& tasks)
{
    std::vector<std::string> files;
    files.reserve(15);
    
    // std::cout << "Searching: " << searchDir << std::endl;
    std::string dirFilter = searchDir + "\\*";

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
            std::string fullName = searchDir + "\\" + std::string(ffd.cFileName);
            tasks.run([&, fullName]{SearchInternal(fullName, tasks);});
        }
        else if (re2::RE2::FullMatch(ffd.cFileName, _fileRegex))
        {
            std::string fullName = searchDir + "\\" + std::string(ffd.cFileName);
            files.push_back(fullName);
        }
    }

    ::FindClose(hfind);

    for (size_t i = 0; i < files.size(); ++i)
    {
        const std::string& fullFileName = files[i];
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
        }

        ::CloseHandle(mapping);
        ::CloseHandle(hfile);
    }
}

