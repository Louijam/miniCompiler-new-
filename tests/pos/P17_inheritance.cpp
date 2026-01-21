#include "hsbi_runtime.h"

class A {
public:     // es reicht, wenn alles public ist (hier nur, damit das Beispiel mit g++ kompiliert)
    A() { aval = 99; }
    A(int x) { aval = x; }

    void foo() { print_char('A'); print_char('f'); print_int(aval); }

    int aval;
};

class B : public A {
public:     // es reicht, wenn alles public ist (hier nur, damit das Beispiel mit g++ kompiliert)
    B(int x) { bval = x; }

    // überschriebene Methode aus A
    void foo() { print_char('B'); print_char('f'); print_int(aval); print_int(bval); }

    // eigene Methode
    void bar() { print_char('B'); print_char('b'); print_int(aval); print_int(bval); }

    int bval;
};


int main() {
    // Vererbung: Initialisierung Basisklasse; Überschreiben von Methoden
    A x = A(2);
    B y = B(7);

    x.foo();    // A, f, 2
    y.foo();    // B, f, 99, 7
    y.bar();    // B, b, 99, 7

    x.aval = 8;
    y.bval = 4;

    x.foo();    // A, f, 8
    y.foo();    // B, f, 99, 4
    y.bar();    // B, b, 99, 4

    return 0;
}
/* EXPECT:
A
f
2
B
f
99
7
B
b
99
7
A
f
8
B
f
99
4
B
b
99
4
*/
