#include "hsbi_runtime.h"

int inc(int& r) { r = r + 1; return r; }
void swap2(int& a, int& b) { int t = a; a = b; b = t; }

int main() {
   int x = 10;
   print_int(inc(x));   // 11
   print_int(x);        // 11

   int a = 3;
   int b = 9;
   swap2(a, b);
   print_int(a);        // 9
   print_int(b);        // 3

   // lokale Referenz
   int& rx = x;
   rx = rx + 5;
   print_int(x);        // 16

   return 0;
}
/* EXPECT (Zeile für Zeile):
11
11
9
3
16
*/
