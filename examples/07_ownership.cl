// Ownership violation: using a value after it was moved

let a = new Widget { name: "hello" };
let b = move(a);

// 'a' has been moved — this should crash
println(a.name);
