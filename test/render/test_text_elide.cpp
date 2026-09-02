#include <cassert>

#include "render/text_elide.h"

void test_text_elide() {
    assert(elide("short", 10) == "short");
    assert(elide("abcdefghij", 5) == "abcd…");

    assert(elide_middle("short", 10) == "short");
    assert(elide_middle("/home/user/a", 12) == "/home/user/a");

    std::string m =
        elide_middle("/home/user/projects/kokusei/src/file.cpp", 20);
    assert(m.find("…") != std::string::npos);
    assert(m.front() == '/');
    assert(m.back() == 'p');

    std::string u = elide_middle("αβγδεζηθικλμν", 6);
    assert(u == "αβγ…μν");
}
