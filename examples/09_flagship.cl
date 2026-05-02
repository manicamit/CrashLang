// The flagship CrashLang demo: use-after-free across function boundaries

fn parse_tokens(buf) {
    let data = buf.data;
    free(buf);
    return data;
}

let buf = new Buffer { data: "hello" };
let tokens = parse_tokens(buf);
let second = buf.data;  // buf was freed inside parse_tokens!
