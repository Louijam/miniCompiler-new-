#include "hsbi_runtime.h"

int main() {
    // Relational
    print_bool(1 < 2);      // 1
    print_bool(2 <= 2);     // 1
    print_bool(3 > 4);      // 0
    print_bool(3 >= 4);     // 0
    print_bool(5 == 5);     // 1
    print_bool(5 != 5);     // 0
    print_bool('a' < 'b');  // 1

    // Logisch
    if (true || false) { print_char('+'); }     // +
    if (false && true) { print_char('x'); }     //
    if (false || true) { print_char('+'); }     // +
    if (true && false) { print_char('x'); }     //

    // Negation
    print_bool(!true);      // 0
    print_bool(!false);     // 1

    return 0;
}
/* EXPECT (Zeile für Zeile):
1
1
0
0
1
0
1
+
+
0
1
*/
