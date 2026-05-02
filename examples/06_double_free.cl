// Double free detection

let obj = new Data { value: 42 };
free(obj);
free(obj); // Should crash: DoubleFree
