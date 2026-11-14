#include "Csv.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

namespace {

std::vector<std::string> splitCsvLine(const std::string& line, char delim, bool* badQuote = nullptr) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else {
                    quoted = false;
                    // after the closing quote only a delimiter (or the end of the line) may follow
                    if (badQuote && i + 1 < line.size() && line[i + 1] != delim && !std::isspace(static_cast<unsigned char>(line[i + 1])))
                        *badQuote = true;
                }
            } else cur += c;
        } else if (c == '"') quoted = true;
        else if (c == delim) { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(trim(cur));
    return out;
}

} // namespace

void CsvRow::fail(const std::string& msg) const {
    std::string hint;
    if (cells != headerCells && headerCells > 0)
        hint = "  [HINWEIS: diese Zeile hat " + std::to_string(cells) + " Spalten, die Kopfzeile " + std::to_string(headerCells) +
               " - fehlt oder steht ein Komma zu viel? Alle Werte verschieben sich dann]";
    throw std::runtime_error(file + ":" + std::to_string(line) + ": " + msg + hint);
}

bool CsvRow::has(const std::string& col) const {
    auto it = fields.find(col);
    return it != fields.end() && !it->second.empty();
}

std::string CsvRow::str(const std::string& col, const std::string& def) const {
    auto it = fields.find(col);
    return (it == fields.end() || it->second.empty()) ? def : it->second;
}

std::string CsvRow::requireStr(const std::string& col) const {
    if (!has(col)) fail("column '" + col + "' is missing or empty");
    return fields.at(col);
}

float CsvRow::requireFloat(const std::string& col) const {
    std::string v = requireStr(col);
    std::replace(v.begin(), v.end(), ',', '.');
    char* end = nullptr;
    float f = std::strtof(v.c_str(), &end);
    if (end == v.c_str() || *end != '\0')
        fail("column '" + col + "': '" + v + "' is not a number");
    return f;
}

float CsvRow::floatOr(const std::string& col, float def) const {
    return has(col) ? requireFloat(col) : def;
}

int CsvRow::requireInt(const std::string& col) const {
    std::string v = requireStr(col);
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0')
        fail("column '" + col + "': '" + v + "' is not an integer");
    return static_cast<int>(n);
}

int CsvRow::intOr(const std::string& col, int def) const {
    return has(col) ? requireInt(col) : def;
}

std::vector<CsvRow> readCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);

    std::vector<CsvRow> rows;
    std::vector<std::string> header;
    char delim = ',';
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3); // UTF-8 BOM
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (header.empty()) {
            if (t.find(';') != std::string::npos && t.find(',') == std::string::npos) delim = ';';
            for (auto& h : splitCsvLine(t, delim)) header.push_back(toLower(h));
            continue;
        }
        bool badQuote = false;
        auto cells = splitCsvLine(t, delim, &badQuote);
        // Rows consisting only of delimiters (empty Excel lines) are skipped
        if (std::all_of(cells.begin(), cells.end(), [](const std::string& c) { return c.empty(); }))
            continue;
        CsvRow row;
        row.file = path;
        row.line = lineNo;
        row.cells = static_cast<int>(cells.size());
        row.headerCells = static_cast<int>(header.size());
        if (badQuote) row.fail("nach dem schließenden Anführungszeichen (\") muss ein Komma folgen");
        if (cells.size() > header.size()) {
            bool extraEmpty = std::all_of(cells.begin() + header.size(), cells.end(),
                                          [](const std::string& c) { return c.empty(); });
            if (!extraEmpty) row.fail("more cells than header columns");
        }
        for (size_t i = 0; i < header.size() && i < cells.size(); ++i)
            row.fields[header[i]] = cells[i];
        rows.push_back(std::move(row));
    }
    if (header.empty()) throw std::runtime_error(path + ": no header row found");
    return rows;
}
