#pragma once
#include "pch.h"

class Searcher
{
public:
    Searcher(const std::string& regex, const std::string& fileFilter);
    
    void Search(const std::string& searchDir);

    void SetColoredOutput(bool v) { _colored = v; }
    
private:

    void SearchInternal(std::string searchDir, class concurrency::task_group& tasks);
    
    re2::RE2 _regex;
    re2::RE2 _fileRegex;
    bool _colored = true;

    std::mutex _outputMutex;
};
