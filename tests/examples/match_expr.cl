// Test: Match expressions
let x = 2;
let result = match x {
    when 1 => "one",
    when 2 => "two",
    when 3 => "three",
    when _ => "other",
};
println(result);  // two

// Match with arithmetic
let grade = 85;
let letter = match grade {
    when 100 => "A+",
    when 85 => "B+",
    when _ => "C",
};
println(letter);  // B+
