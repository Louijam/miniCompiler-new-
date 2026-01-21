#include "hsbi_runtime.h"

int main() {
    // simple while-loop
    bool a = true;
    while (a) {
        print_bool(a);  // 1
        a = false;
    }

    // more complex loop
    int i = 0;
    while (i<5) {
        i = i + 1;
        print_int(i);
    }

    return 0;
}
/* EXPECT:
1
1
2
3
4
5
*/
