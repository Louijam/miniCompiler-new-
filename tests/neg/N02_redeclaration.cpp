int main() {
    bool a = true;
    bool &aa = a;

    // Re-Definition darf nicht akzeptiert werden
    bool &a = aa;

    return 0;
}
