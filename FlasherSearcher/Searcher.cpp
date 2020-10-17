#include "pch.h"
#include "Searcher.h"


Searcher::Searcher(const std::string& regex, const std::string& fileFilter)
    : _regex("(" + regex + ")")
    , _fileRegex(fileFilter)
{
}

void Searcher::Search(const std::string& searchDir)
{
    concurrency::task_group tasks;
    
    tasks.run([&]{SearchInternal(searchDir, tasks);});

    tasks.wait();
}

void Searcher::SearchInternal(std::string searchDir, concurrency::task_group& tasks)
{
    std::vector<std::string> files;
    files.reserve(15);
    
    // std::cout << "Searching: " << searchDir << std::endl;
    std::string dirFilter = searchDir + "\\*";

    WIN32_FIND_DATAA ffd;
    // Skip "." 
    HANDLE hfind = ::FindFirstFileA(dirFilter.c_str(), &ffd);
    if (hfind == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Invalid path!" << std::endl;
        return;
    }
    
    // Skip ".."
    ::FindNextFileA(hfind, &ffd);


    while (::FindNextFileA(hfind, &ffd))
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

    // https://en.wikipedia.org/wiki/ANSI_escape_code
    static const std::string pathForeground = "\033[1;90m";
    static const std::string matchForeground = "\033[1;32m";
    static const std::string resetForeground = "\033[0m";
    
    for (size_t i = 0; i < files.size(); ++i)
    {
        //    vvvvvvvvvvv copy! array resize would make it dangle
        const std::string fullFileName = files[i];
        HANDLE hfile = ::CreateFileA(fullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        if (hfile == INVALID_HANDLE_VALUE)
        {
            std::cerr << "Couldn't open file!" << std::endl;
            continue;
        }

        DWORD size = ::GetFileSize(hfile, 0);

        HANDLE mapping = ::CreateFileMappingA(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
        char* dataBuffer = static_cast<char*>(::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size));

        re2::StringPiece data(dataBuffer, size);
        if (re2::RE2::PartialMatch(data, _regex))
        {
            std::lock_guard<std::mutex> lock(_outputMutex);
            // std::cout << fullFileName << std::endl;

            unsigned int lineNumber = 1;
            size_t lineBegin = 0;
            size_t pos = 0;
            while (true)
            {
                char c = data[pos];
                if (c == '\r' || c == '\n')
                {
                    // new line
                    const size_t lineSize = pos - lineBegin;
                    if (lineSize != 0)
                    {
                        re2::StringPiece line(dataBuffer + lineBegin, lineSize);
                        re2::StringPiece match;
                        if (re2::RE2::PartialMatch(line, _regex, &match))
                        {
                            size_t pos = match.data() - line.begin();
                            std::cout << pathForeground << fullFileName << ":" << lineNumber << ":";
                            if (pos > 0)
                            {
                                re2::StringPiece matchPrefix(line.begin(), pos);
                                std::cout << resetForeground << matchPrefix;
                            }
                            std::cout << matchForeground << match;
                            if (match.end() != line.end())
                            {
                                re2::StringPiece matchSuffix(match.end(), line.end() - match.end());
                                std::cout << resetForeground << matchSuffix;
                            }
                            else
                            {
                                std::cout << resetForeground;                                
                            }
                            std::cout << std::endl;


                            // SetConsoleTextAttribute & WriteConsoleA don't work on Rider embedded terminal :(
                        }
                    }
                    ++pos;
                    c = data[pos];
                    if (c == '\n')
                        ++pos;
                    
                    lineBegin = pos;
                    ++lineNumber;
                }
                else
                {
                    ++pos;
                }

                if (pos == size)
                    break;
            }
        }

        ::CloseHandle(mapping);
        ::CloseHandle(hfile);
    }
}

