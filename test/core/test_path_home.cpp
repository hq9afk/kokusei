#include <cassert>
#include <cstdlib>

#include "core/path_home.h"

void test_path_home() {
    setenv("HOME", "/home/tester", 1);

    assert(path_collapse_home("/home/tester") == "~");
    assert(path_collapse_home("/home/tester/Pictures") == "~/Pictures");
    assert(path_collapse_home("/etc/passwd") == "/etc/passwd");
    assert(path_collapse_home("") == "");
    assert(path_collapse_home("/home/testerwork") == "/home/testerwork");

    assert(path_expand_home("~") == "/home/tester");
    assert(path_expand_home("~/Videos") == "/home/tester/Videos");
    assert(path_expand_home("/usr/share") == "/usr/share");
    assert(path_expand_home("~foo") == "~foo");
    assert(path_expand_home("") == "");

    assert(path_expand_home(path_collapse_home("/home/tester/a/b")) ==
           "/home/tester/a/b");
    assert(path_collapse_home(path_expand_home("~/a/b")) == "~/a/b");
}
