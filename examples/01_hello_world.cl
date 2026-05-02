// Fibonacci — tests recursion, arithmetic, and if/else
fn fibonacci(n) {
    if n <= 1 {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

println("Fibonacci sequence:");
for i in range(0, 10) {
    println(i, "->", fibonacci(i));
}

// Closures — counter that captures its environment
fn make_counter() {
    let count = 0;
    fn increment() {
        count = count + 1;
        return count;
    }
    return increment;
}

let counter = make_counter();
println("Counter:", counter(), counter(), counter());

// Arrays and built-ins
let numbers = [5, 3, 8, 1, 9, 2, 7, 4, 6];
println("Array:", numbers);
println("Length:", len(numbers));

push(numbers, 10);
println("After push:", numbers);

let last = pop(numbers);
println("Popped:", last);

// Bubble sort to test nested loops and array mutation
fn bubble_sort(arr) {
    let n = len(arr);
    let i = 0;
    while i < n {
        let j = 0;
        while j < n - i - 1 {
            if arr[j] > arr[j + 1] {
                let tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

bubble_sort(numbers);
println("Sorted:", numbers);

// String operations
let greeting = "hello" + ", " + "world";
println(greeting);
println("Length:", len(greeting));
println("Type:", type_of(greeting));

// Type coercions
println(to_string(42) + " is a number");
println(to_int("100") + 1);

// assert
assert(1 + 1 == 2, "math is broken");
println("All assertions passed.");
