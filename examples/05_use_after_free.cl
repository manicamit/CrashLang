// Use-after-free: the flagship CrashLang demo

let buf = new Buffer { data: "hello", size: 5 };
println("Allocated:", buf.data);

free(buf);

// This should crash with UseAfterFree
let val = buf.data;
println(val);
