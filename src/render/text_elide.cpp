#include <vector>

#include "render/text_elide.h"

std::string elide(const std::string &s, size_t max_chars) {
    if (s.size() <= max_chars)
        return s;
    size_t cut = max_chars - 1;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
        --cut;
    return s.substr(0, cut) + "…";
}

std::string elide_middle(const std::string &s, size_t max_chars) {
    std::vector<size_t> starts;
    for (size_t i = 0; i < s.size(); ++i)
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
            starts.push_back(i);
    if (starts.size() <= max_chars)
        return s;
    if (max_chars < 2)
        return elide(s, max_chars);
    size_t keep = max_chars - 1;
    size_t head = (keep + 1) / 2;
    size_t tail = keep - head;
    return s.substr(0, starts[head]) + "…" +
           s.substr(starts[starts.size() - tail]);
}
