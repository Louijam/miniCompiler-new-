class A { public: int x; };

int main() {
    A a;
    a.y = 1; // Fehler: Feld existiert nicht

    return 0;
}
