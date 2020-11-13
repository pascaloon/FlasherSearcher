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
    files.reserve(16);
    
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

        constexpr size_t EXPECTED_LINEMATCHES = 8;

        std::vector<re2::StringPiece> matches;
        matches.reserve(EXPECTED_LINEMATCHES);

        re2::StringPiece data2(dataBuffer, size);
        re2::StringPiece match;
        while (re2::RE2::FindAndConsume(&data2, _regex, &match))
        {
            matches.push_back(match);
        }

        if (!matches.empty())
        {
            std::vector<re2::StringPiece> lines;
            lines.reserve(EXPECTED_LINEMATCHES);
            std::vector<unsigned int> lineNumbers;
            lineNumbers.reserve(EXPECTED_LINEMATCHES);
            
            int matchId = 0;
            const re2::StringPiece* cMatch = &matches[matchId];
            const char* lineBegin = data.begin();
            unsigned int lineNumber = 1;
            int matchesPerLine = 0;
            
            for (const char* c = data.begin(); c < data.end(); ++c)
            {
                if (cMatch && cMatch->begin() == c)
                {
                    ++matchesPerLine;
                    ++matchId;
                    cMatch = matchId != matches.size()
                        ? &matches[matchId]
                        : nullptr;
                }
                if (*c == '\r' || *c == '\n')
                {
                    for (int i = 0; i < matchesPerLine; ++i)
                    {
                        lines.push_back(re2::StringPiece(lineBegin, c - lineBegin));                            
                        lineNumbers.push_back(lineNumber);                            
                    }
                    
                    matchesPerLine = 0;
                    if (!cMatch)
                        break;
                    
                    ++lineNumber;
                    if (c != (data.end() - 1) && *(c + 1) == '\n')
                        ++c;
                    lineBegin = c + 1; 
                }
            }

            // In case we reach EOF before an newline and we matched something on the very last line
            for (int i = 0; i < matchesPerLine; ++i)
            {
                lines.push_back(re2::StringPiece(lineBegin, data.end() - lineBegin));                            
                lineNumbers.push_back(lineNumber);                            
            }

            // https://en.wikipedia.org/wiki/ANSI_escape_code
            static constexpr char pathForeground[] = "\033[1;90m";
            static constexpr char matchForeground[] = "\033[1;32m";
            static constexpr char resetForeground[] = "\033[0m";
            
            std::lock_guard<std::mutex> lock(_outputMutex);
            for (size_t i = 0; i < lines.size(); ++i)
            {
                const re2::StringPiece& line = lines[i];
                const re2::StringPiece& match = matches[i];
                int clineNumber = lineNumbers[i];
                
                size_t pos = match.data() - line.begin();

                if (_colored)
                {
                    std::cout << pathForeground << fullFileName << ":" << clineNumber << ":";
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
                    std::cout << fullFileName << ":" << clineNumber << ":";
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

