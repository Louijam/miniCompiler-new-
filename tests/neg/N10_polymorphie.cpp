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

    // überschriebene virtuelle Methode aus B
    void foo() { print_char('C'); print_char('f'); print_int(aval); print_int(bval); print_int(cval); }

    // überschriebene Methode aus B
    void bar() { print_char('C'); print_char('b'); print_int(aval); print_int(bval); print_int(cval); }

    int cval;
};


int main() {
    // Statische Polymorphie (Normalfall)
    B b = B(9);
    A a = b;

    a.bar();    // nicht erlaubt!

    return 0;
}
