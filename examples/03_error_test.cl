// Error testing script

fn deeply_nested_call(a, b) {
    // Deliberate type mismatch error: trying to add an int to a boolean
    return a + b;
}

fn middle_function() {
    let x = 10;
    let y = true;
    deeply_nested_call(x, y);
}

fn main() {
    middle_function();
}

main();
