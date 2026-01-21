#include "hsbi_runtime.h"

int add(int a, int b) { return a + b; }

int main() {
  int x = 5;
  int y = 7;
  print_int(add(x, y));      // 12

  bool b = x * 2 < y;        // 10 < 7 -> false
  print_bool(b);             // false

  char ch = 'A';
  print_char(ch);            // A

  string str = "foo";        // foo
  print_string(str);

  // while + if
  int s = 0;
  int i = 0;
  while (i < 4) {
    if (i % 2 == 0) { s = s + i; }
    i = i + 1;
  }
  print_int(s);              // 0 + 2 = 2

  return 0;
}
/* EXPECT (Zeile für Zeile):
12
0
A
foo
2
*/
