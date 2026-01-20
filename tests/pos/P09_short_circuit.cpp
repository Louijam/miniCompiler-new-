#include "hsbi_runtime.h"

int ping() { print_char('x'); return 1; }

int main() {
    if (false && (ping() == 1)) { print_char('!'); } //

    if (true || (ping() == 1)) { print_char('+'); } // '+'

    return 0;
}
/* EXPECT (Zeile für Zeile):
+
*/
