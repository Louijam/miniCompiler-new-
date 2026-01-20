#include "hsbi_runtime.h"

class Base {
public:
  int x;

  Base() { x = 1; }          // Default-Konstruktor
  int g() { return x + 1; }  // nicht virtuell
  virtual int f() { return x; }
};

class Der : public Base {
public:
  int y;

  Der() { y = 5; x = 2; }       // Base-Teil wird default-konstruiert; Felder im Body setzen
  Der(int a, int b) { x = a; y = b; }

  int f() { return x + y; }     // virtuell überschrieben (ohne 'override')
};

int main() {
  Der d;                 // nutzt Der()
  print_int(d.f());      // 2 + 5 = 7
  print_int(d.g());      // g() aus Base am statischen Typ Der -> nicht virtuell -> 3

  Der e = Der(10, 4);    // nutzt Der(int,int)
  Base& rb = e;          // Referenz auf Basistyp
  print_int(rb.f());     // dynamisch -> Der::f -> 14
  print_int(rb.g());     // statisch -> Base::g -> 11

  return 0;
}
/* EXPECT (Zeile für Zeile):
7
3
14
11
*/
