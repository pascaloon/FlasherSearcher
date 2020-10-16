#pragma once
#include "pch.h"

class Searcher
{
public:
    Searcher(std::string regex);
    
    void Search(std::string searchDir, std::string fileFilter);

private:
    
    
    re2::RE2 _regex;
};
