// Lexer stress test — exercises all token types

/* This is a
   block comment
   /* nested! */
   still going */

let x = 42;
let pi = 3.14159;
let name = "crash\nlang";
let flag = true;
let nothing = nil;

fn add(a, b) {
    return a + b;
}

if x >= 10 and not flag {
    let y = x * 2 - 1;
    let z = y % 3;
} else {
    while x > 0 {
        x = x / 2;
        if x == 0 {
            break;
        }
    }
}

for i in range(0, 10) {
    continue;
}

let p = new Point { x: 1, y: 2 };
let r = ref(p);
let val = deref(r);
free(p);
let q = move(p);

let arr = [1, 2, 3];
let first = arr[0];

// Operators: + - * / % = == != < <= > >=
let cmp = x != y or x <= 100;
