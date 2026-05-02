// Phase 3 test: heap allocation, field access, and free

// Basic struct allocation
let p = new Point { x: 10, y: 20 };
println("Point:", p.x, p.y);

// Field mutation
p.x = 42;
println("After mutation:", p.x, p.y);

// Reference
let r = ref(p);
println("Ref:", r.x);

// Deref
let d = deref(r);
println("Deref:", d.x, d.y);

// Free
free(p);
println("Freed successfully.");
