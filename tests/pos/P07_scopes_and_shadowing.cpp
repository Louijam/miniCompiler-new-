#include "hsbi_runtime.h"

int main() {
    int x = 1;
    int &rx = x;

    { int x = 5; print_int(x); print_int(rx); } // 5, 1

    rx = 7;
    print_int(x); // 7

    {
        int y = 3;
        int &ry = y;
        { int y = 9; print_int(ry); print_int(y); } // 3, 9
        print_int(ry); // 3
        print_int(y); // 3
    }

    return 0;
}
/* EXPECT:
5
1
7
3
9
3
3
*/
