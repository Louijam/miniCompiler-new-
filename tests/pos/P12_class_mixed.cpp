#include "hsbi_runtime.h"

class C {
public:     // es reicht, wenn alles public ist (hier nur, damit das Beispiel mit g++ kompiliert)
    C() { value = 0; }
    C(int x) { value = x; }

    int value;
};


int main() {
    // Klasse mit TEILWEISE selbst implementierten C'toren und Zuweisung, Rest Default vom Compiler
    print_char('C');        // 'C'

    C x;
    x.value = 9;
    print_int(x.value);     // 9

    C y = C(x);
    print_int(x.value);     // 9
    print_int(y.value);     // 9

    y.value = 7;
    print_int(x.value);     // 9
    print_int(y.value);     // 7

    C z = y;
    print_int(x.value);     // 9
    print_int(y.value);     // 7
    print_int(z.value);     // 7

    C foo;
    foo = z;
    z.value = 99;
    print_int(foo.value);   // 7
    print_int(z.value);     // 99

    return 0;
}
/* EXPECT:
C
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
