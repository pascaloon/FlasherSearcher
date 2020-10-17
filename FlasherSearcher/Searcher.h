#pragma once
#include "pch.h"

class Searcher
{
public:
    Searcher(std::string regex, std::string fileFilter);
    
    void Search(std::string searchDir);

private:

    void SearchInternal(std::string searchDir, class concurrency::task_group& tasks);
    
    re2::RE2 _regex;
    re2::RE2 _fileRegex;
};
