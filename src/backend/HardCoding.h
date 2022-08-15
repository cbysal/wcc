#ifndef __HARD_CODING_H__
#define __HARD_CODING_H__

#include <unordered_map>
#include <utility>

extern std::unordered_map<unsigned, unsigned> num2powerMap;
extern std::unordered_map<unsigned, std::pair<unsigned, unsigned>>
    num2power2Map;
extern std::unordered_map<unsigned, std::pair<unsigned, unsigned>> num2lineMap;

#endif