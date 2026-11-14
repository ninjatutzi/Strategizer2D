#pragma once
#include <map>
#include <string>
#include <vector>

// Minimal CSV reader used for all game data files.
//  * first non-comment line is the header (column names, case-insensitive)
//  * lines starting with '#' and empty lines are ignored
//  * delimiter is ',' or ';' (auto-detected from the header, so files saved by
//    a German Excel work too; in that case "1,5" is accepted as a number)
//  * "quoted,fields" are supported
// All errors throw std::runtime_error with "file:line" so typos are easy to find.
struct CsvRow {
    std::string file;
    int line = 0;
    std::map<std::string, std::string> fields;

    bool has(const std::string& col) const;
    std::string str(const std::string& col, const std::string& def = "") const;
    std::string requireStr(const std::string& col) const;
    int requireInt(const std::string& col) const;
    int intOr(const std::string& col, int def) const;
    float requireFloat(const std::string& col) const;
    float floatOr(const std::string& col, float def) const;
    [[noreturn]] void fail(const std::string& msg) const;
};

std::vector<CsvRow> readCsv(const std::string& path);

// Shared string helpers
std::string trim(const std::string& s);
std::string toLower(std::string s);
std::vector<std::string> split(const std::string& s, char delim);
