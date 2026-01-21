#include "hsbi_runtime.h"

void f(int  r) { print_int(r); print_string("f(int)");  }
void f(int& r) { print_int(r); print_string("f(int&)"); }

int main() {
    int a = 42;

    f(a);  // error: f(int) AND f(int&) would match - which one shall be used?

    return 0;
}
