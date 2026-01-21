#include "hsbi_runtime.h"

int main() {
    // Variablen mit Basisdatentypen (ohne Initialisierung)
    bool a;
    int b;
    char c;
    string d;

    a = false;
    b = 7;
    c = 'a';
    d = "foo";

    print_bool(a);    // 0
    print_int(b);     // 7
    print_char(c);    // 'a'
    print_string(d);  // "foo"

    // Variablen mit Basisdatentypen (mit Initialisierung)
    bool aa = true;
    int bb = 42;
    char cc = 'c';
    string dd = "wuppie";

    print_bool(aa);    // 1
    print_int(bb);     // 42
    print_char(cc);    // 'c'
    print_string(dd);  // "wuppie"

    return 0;
}
/* EXPECT:
0
7
a
foo
1
42
c
wuppie
*/
