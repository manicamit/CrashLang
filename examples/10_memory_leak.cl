// Memory leak: allocated but never freed

let a = new Node { value: 1 };
let b = new Node { value: 2 };
let c = new Node { value: 3 };

free(a);
// b and c are never freed — leak!

println("done");
