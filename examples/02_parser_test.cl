// Parser stress test — full CrashLang syntax

fn fibonacci(n) {
    if n <= 1 {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

let result = fibonacci(10);
println(result);

// Memory operations
let p = new Point { x: 1, y: 2 };
let r = ref(p);
let val = deref(r);
let q = move(p);
free(q);

// Arrays and indexing
let arr = [10, 20, 30];
let first = arr[0];
arr[1] = 99;

// Control flow
for i in range(0, 5) {
    if i == 3 {
        break;
    }
    println(i);
}

let x = 10;
while x > 0 {
    x = x - 1;
    if x == 5 {
        continue;
    }
}

// Logical operators and precedence
let check = x > 0 and x < 100 or not flag;

// Nested field access
let d = p.x;

// Else-if chains
if x == 0 {
    println("zero");
} else if x == 1 {
    println("one");
} else {
    println("other");
}
