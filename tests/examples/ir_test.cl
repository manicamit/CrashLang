fn add(a, b) {
    return a + b;
}

fn max(x, y) {
    if x > y {
        return x;
    }
    return y;
}

fn factorial(n) {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}
