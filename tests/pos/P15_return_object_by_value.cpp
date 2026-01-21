#include "hsbi_runtime.h"

class Box {
public:
    Box() { v = 0; }
    Box(int x) { v = x; }
    void set(int x) { v = x; }
    void print() { print_int(v); }
    int v;
};

Box makeBox(int x) {
    Box b = Box(x);
    return b; // Copy bei Return
}

int main() {
    Box a = makeBox(5);
    a.print(); // 5

    Box b;
    b = makeBox(9);
    b.print(); // 9

    return 0;
}
/* EXPECT:
5
9
*/
