// VM smoke test — features that work on the bytecode path.

// ── Variables and arithmetic ───────────────────────────────────────────────────
let x = 42;
let y = 8;
println("x + y:", x + y);

// ── Functions ──────────────────────────────────────────────────────────────────
fn add(a, b) {
    return a + b;
}
println("add(3, 4):", add(3, 4));

fn factorial(n) {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}
println("5! =", factorial(5));

// ── Control flow ───────────────────────────────────────────────────────────────
let result = 0;
let i = 0;
while i < 10 {
    result = result + i;
    i = i + 1;
}
println("sum 0..9:", result);

// ── Arrays and for-in ──────────────────────────────────────────────────────────
let arr = [10, 20, 30];
let total = 0;
for item in arr {
    total = total + item;
}
println("array sum:", total);

// ── Strings ────────────────────────────────────────────────────────────────────
let msg = "Hello" + " " + "VM!";
println(msg);
println("len:", len(msg));
println("upper:", upper(msg));

// ── Builtins ───────────────────────────────────────────────────────────────────
println("type_of:", type_of(42));
println("sqrt:", sqrt(144));

// ── Lambdas ────────────────────────────────────────────────────────────────────
let double = fn(x) { return x * 2; };
println("double(21):", double(21));

// ── If/else ────────────────────────────────────────────────────────────────────
if true {
    println("true branch");
} else {
    println("false branch");
}

println("All VM tests passed.");
