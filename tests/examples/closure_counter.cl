// Test: VM closure with mutable upvalues (counter pattern)
fn make_counter() {
    let count = 0;
    fn increment() {
        count = count + 1;
        return count;
    }
    return increment;
}

let counter = make_counter();
println(counter());  // 1
println(counter());  // 2
println(counter());  // 3
