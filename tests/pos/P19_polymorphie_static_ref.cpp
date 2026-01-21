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
    B() { bval = 77; }
    B(int x) { bval = x; }

    // überschriebene Methode aus A
    virtual void foo() { print_char('B'); print_char('f'); print_int(aval); print_int(bval); }

    // eigene Methode
    void bar() { print_char('B'); print_char('b'); print_int(aval); print_int(bval); }

    int bval;
};


int main() {
    // Statische Polymorphie (trotz Basisklassenreferenz)
    print_char('B');  // 'B'

    B b = B(9);
    A &a = b;

    b.foo();    // B, f, 99, 9
    b.bar();    // B, b, 99, 9

    a.foo();    // A, f, 99         => statische Polymorphie (Referenz genutzt, aber nicht virtual in A)

    return 0;
}
/* EXPECT:
B
B
f
99
9
B
b
99
9
A
f
99
*/
