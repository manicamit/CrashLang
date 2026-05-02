// Test: break and continue in VM mode
let sum = 0;
let i = 0;
while i < 10 {
    i = i + 1;
    if i == 5 {
        continue;
    }
    if i == 8 {
        break;
    }
    sum = sum + i;
}
println(sum);  // 1+2+3+4+6+7 = 23
