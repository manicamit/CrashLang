// Phase 5 test: Advanced features

// ── String indexing ────────────────────────────────────────────────────────────
let s = "CrashLang";
println("First char:", s[0]);
println("Last char:", s[len(s) - 1]);

// ── String iteration ───────────────────────────────────────────────────────────
let vowels = "";
for ch in "hello world" {
    if ch == "e" or ch == "o" {
        vowels = vowels + ch;
    }
}
println("Vowels:", vowels);

// ── Lambda expressions ─────────────────────────────────────────────────────────
let double = fn(x) { return x * 2; };
println("Double 5:", double(5));

let add = fn(a, b) { return a + b; };
println("3 + 4:", add(3, 4));

// Lambda as callback (immediately invoked)
let result = fn(n) { return n * n; }(7);
println("7^2:", result);

// Closures with lambdas
fn make_adder(n) {
    return fn(x) { return x + n; };
}
let add10 = make_adder(10);
println("15 + 10:", add10(15));

// ── String utilities ───────────────────────────────────────────────────────────
println("Upper:", upper("hello"));
println("Lower:", lower("WORLD"));
println("Trim:", trim("  spaces  "));
println("Contains:", contains("CrashLang", "Crash"));
println("Starts:", starts_with("CrashLang", "Crash"));
println("Ends:", ends_with("CrashLang", "Lang"));
println("Replace:", replace("foo bar foo", "foo", "baz"));
println("Substr:", substr("CrashLang", 5));
println("Substr:", substr("CrashLang", 0, 5));

// ── Split and join ─────────────────────────────────────────────────────────────
let parts = split("a,b,c,d", ",");
println("Split:", parts);
println("Join:", join(parts, " - "));

// ── Chars and reverse ──────────────────────────────────────────────────────────
println("Chars:", chars("abc"));
println("Reverse str:", reverse("CrashLang"));
println("Reverse arr:", reverse([1, 2, 3]));

println("All Phase 5 tests passed.");
