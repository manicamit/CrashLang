// Test: Constant folding optimizer
// These should be folded at compile time:
let a = 3 + 4;         // Folded to 7
let b = 10 * 2 + 5;    // Folded to 25
let c = "hello" + " " + "world";  // Folded at compile time
let d = not true;       // Folded to false

println(a);  // 7
println(b);  // 25
println(c);  // hello world
println(d);  // false
