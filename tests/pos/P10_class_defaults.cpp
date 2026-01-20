#include "hsbi_runtime.h"

class A {
public:     // es reicht, wenn alles public ist (hier nur, damit das Beispiel mit g++ kompiliert)
    int value;
};


int main() {
    // Klasse mit Defaults (C'tor, Zuweisung)
    print_char('A');        // 'A'

    A x;
    x.value = 9;
    print_int(x.value);     // 9

    A y = A(x);
    print_int(x.value);     // 9
    print_int(y.value);     // 9

    y.value = 7;
    print_int(x.value);     // 9
    print_int(y.value);     // 7

    A z = y;
    print_int(x.value);     // 9
    print_int(y.value);     // 7
    print_int(z.value);     // 7

    A foo;
    foo = z;
    z.value = 99;
    print_int(foo.value);   // 7
    print_int(z.value);     // 99

    return 0;
}
/* EXPECT:
A
9
9
9
9
7
9
7
7
7
99
*/
