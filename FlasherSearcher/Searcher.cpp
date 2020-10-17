#include "pch.h"
#include "Searcher.h"


Searcher::Searcher(const std::string& regex, const std::string& fileFilter)
    : _regex("(?i)(" + regex + ")")
    , _fileRegex("(?i)" + fileFilter)
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
        DWORD errorId = GetLastError();
        LPSTR messageBuffer = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        {
            std::lock_guard<std::mutex> lock(_outputMutex);
            std::cerr << "Error occured for directory: '" << searchDir << "'. Error: " << messageBuffer << std::endl;
        }
        LocalFree(messageBuffer);
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
    
    for (size_t i = 0; i < files.size(); ++i)
    {
        //    vvvvvvvvvvv copy! array resize would make it dangle
        const std::string fullFileName = files[i];
        HANDLE hfile = ::CreateFileA(fullFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        if (hfile == INVALID_HANDLE_VALUE)
        {
            DWORD errorId = GetLastError();
            LPSTR messageBuffer = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                         NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            {
                std::lock_guard<std::mutex> lock(_outputMutex);
                std::cerr << "Error occured for file: '" << fullFileName << "'. Error: " << messageBuffer << std::endl;
            }
            LocalFree(messageBuffer);
            continue;;
        }

        DWORD size = ::GetFileSize(hfile, 0);

        HANDLE mapping = ::CreateFileMappingA(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
        char* dataBuffer = static_cast<char*>(::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size));

        re2::StringPiece data(dataBuffer, size);
        if (re2::RE2::PartialMatch(data, _regex))
        {
            constexpr size_t EXPECTED_LINEMATCHES = 5;
            std::vector<re2::StringPiece> lines;
            lines.reserve(EXPECTED_LINEMATCHES);
            std::vector<re2::StringPiece> matches;
            matches.reserve(EXPECTED_LINEMATCHES);
            
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
                            lines.push_back(line);
                            matches.push_back(match);
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

            // https://en.wikipedia.org/wiki/ANSI_escape_code
            static const std::string pathForeground = "\033[1;90m";
            static const std::string matchForeground = "\033[1;32m";
            static const std::string resetForeground = "\033[0m";
            
            std::lock_guard<std::mutex> lock(_outputMutex);
            for (size_t i = 0; i < lines.size(); ++i)
            {
                const re2::StringPiece& line = lines[i];
                const re2::StringPiece& match = matches[i];

                size_t pos = match.data() - line.begin();
    
                if (_colored)
                {
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
                }
                else
                {
                    std::cout << fullFileName << ":" << lineNumber << ":";
                    if (pos > 0)
                    {
                        re2::StringPiece matchPrefix(line.begin(), pos);
                        std::cout << matchPrefix;
                    }
                    std::cout << match;
                    if (match.end() != line.end())
                    {
                        re2::StringPiece matchSuffix(match.end(), line.end() - match.end());
                        std::cout << matchSuffix;
                    }
                    std::cout << std::endl;
                }

                // SetConsoleTextAttribute & WriteConsoleA don't work on Rider embedded terminal :(
            }
        }

        ::CloseHandle(mapping);
        ::CloseHandle(hfile);
    }
}

