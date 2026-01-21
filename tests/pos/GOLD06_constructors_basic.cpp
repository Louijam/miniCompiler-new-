#include "hsbi_runtime.h"

class Point {
public:
  int x;
  int y;

  // Default-Konstruktor: ohne Initialisierungsliste, Felder im Body setzen
  Point() { x = 0; y = 0; }

  // Parametrisierter Konstruktor
  Point(int a, int b) { x = a; y = b; }

  int sum() { return x + y; }
};

int main() {
  Point p;            // Default-Konstruktor
  print_int(p.sum()); // 0

  Point q = Point(3, 4); // Parametrisierter Konstruktor
  print_int(q.sum());    // 7

  return 0;
}
/* EXPECT (Zeile für Zeile):
0
7
*/
