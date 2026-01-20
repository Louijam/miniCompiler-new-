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

class C : public B {
public:     // es reicht, wenn alles public ist (hier nur, damit das Beispiel mit g++ kompiliert)
    C(int x) { cval = x; }

    // überschriebene virtual Methode aus B
    void foo() { print_char('C'); print_char('f'); print_int(aval); print_int(bval); print_int(cval); }

    // überschriebene Methode aus B
    void bar() { print_char('C'); print_char('b'); print_int(aval); print_int(bval); print_int(cval); }

    int cval;
};


int main() {
    // Dynamische Polymorphie (Basisklassenreferenz und virtuelle Methode)
    print_char('C');  // 'C'

    C c = C(9);
    B &b = c;

    c.foo();    // C, f, 99, 77, 9
    c.bar();    // C, b, 99, 77, 9

    b.foo();    // C, f, 99, 77, 9      => dynamische Polymorphie (Referenz genutzt, virtual in B)
    b.bar();    // B, b, 99, 77         => statische Polymorphie (Referenz genutzt, aber nicht virtual in B)

    B bb = c;
    bb.foo();    // B, f, 99, 77        => statische Polymorphie (keine Referenz genutzt, virtual in B)

    return 0;
}
/* EXPECT:
C
C
f
99
77
9
C
b
99
77
9
C
f
99
77
9
B
b
99
77
B
f
99
77
*/
