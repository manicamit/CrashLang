#include "crashlang/vm.hpp"
#include "crashlang/builtins.hpp"

#include <cmath>
#include <iostream>

namespace crashlang {
VM::VM(const SourceFile& source)
    : source_(source)
{
    stack_.reserve(STACK_MAX);
    frames_.reserve(FRAMES_MAX);
    register_builtins();
}

VM::~VM() = default;
ObjUpvalue* VM::capture_upvalue(size_t stack_index) {

    ObjUpvalue* prev = nullptr;
    ObjUpvalue* curr = open_upvalues_;
    while (curr != nullptr && curr->stack_index > stack_index) {
        prev = curr;
        curr = curr->next;
    }
    if (curr != nullptr && curr->stack_index == stack_index) {
        return curr;
    }
    auto uv = std::make_unique<ObjUpvalue>();
    uv->stack_index = stack_index;
    uv->is_closed = false;
    uv->next = curr;

    ObjUpvalue* raw = uv.get();
    all_upvalues_.push_back(std::move(uv));

    if (prev == nullptr) {
        open_upvalues_ = raw;
    } else {
        prev->next = raw;
    }
    return raw;
}

void VM::close_upvalues(size_t last) {
    while (open_upvalues_ != nullptr && open_upvalues_->stack_index >= last) {
        ObjUpvalue* uv = open_upvalues_;
        uv->closed = stack_[uv->stack_index];
        uv->is_closed = true;
        open_upvalues_ = uv->next;
        uv->next = nullptr;
    }
}
void VM::push(Value value) {
    if (stack_.size() >= STACK_MAX) {
        throw make_error(CrashKind::StackOverflow, "value stack overflow");
    }
    stack_.push_back(std::move(value));
}

Value VM::pop() {
    Value v = std::move(stack_.back());
    stack_.pop_back();
    return v;
}

Value& VM::peek(size_t distance) {
    return stack_[stack_.size() - 1 - distance];
}

void VM::pop_n(size_t count) {
    for (size_t i = 0; i < count; ++i) stack_.pop_back();
}
uint8_t VM::read_byte() {
    return frame().chunk->code[frame().ip++];
}

uint16_t VM::read_short() {
    uint8_t hi = read_byte();
    uint8_t lo = read_byte();
    return static_cast<uint16_t>((hi << 8) | lo);
}

const Value& VM::read_constant() {
    uint16_t idx = read_short();
    return frame().chunk->constants[idx];
}

const std::string& VM::read_string() {
    return read_constant().as_string();
}

uint32_t VM::current_line() const {
    const auto& f = frames_.back();
    if (f.ip > 0 && f.ip - 1 < f.chunk->lines.size()) {
        return f.chunk->lines[f.ip - 1];
    }
    return 0;
}

Span VM::current_span() const {
    Span s;
    s.start.file = &source_;
    s.start.line = current_line();
    s.start.column = 1;
    s.end = s.start;
    return s;
}

CrashError VM::make_error(CrashKind kind, const std::string& detail) const {
    Span loc;
    loc.start.file = &source_;
    loc.start.line = current_line();
    loc.start.column = 1;
    loc.end = loc.start;

    CrashError err(kind, loc, detail);
    for (const auto& f : frames_) {
        CallFrame cf;
        cf.function_name = f.name;
        Span cs;
        cs.start.file = &source_;
        cs.start.line = f.ip < f.chunk->lines.size() ? f.chunk->lines[f.ip] : 0;
        cs.start.column = 1;
        cs.end = cs.start;
        cf.call_site = cs;
        err.call_stack.push_back(std::move(cf));
    }
    return err;
}
void VM::register_builtins() {
    Environment env;
    crashlang::register_builtins(env);

    static const char* builtin_names[] = {
        "print", "println", "len", "type_of", "to_string", "to_int", "to_float",
        "assert", "push", "pop", "range", "input", "clock", "sqrt", "abs",
        "max", "min", "map", "filter", "forEach",
        "substr", "split", "join", "contains", "starts_with", "ends_with",
        "replace", "trim", "upper", "lower", "chars", "reverse",
        nullptr
    };

    for (int i = 0; builtin_names[i]; ++i) {
        auto v = env.get(builtin_names[i]);
        if (v) {
            globals_[builtin_names[i]] = std::move(*v);
        }
    }
}
void VM::execute(Chunk& chunk) {
    VMFrame top;
    top.chunk = &chunk;
    top.ip    = 0;
    top.base  = 0;
    top.name  = chunk.name;
    frames_.push_back(std::move(top));

    run();
}
void VM::run() {
    for (;;) {
        auto op = static_cast<OpCode>(read_byte());

        switch (op) {

        case OpCode::Halt:
            return;

        case OpCode::Constant: {
            push(read_constant());
            break;
        }

        case OpCode::Nil:   push(Value(NilType{})); break;
        case OpCode::True:  push(Value(true)); break;
        case OpCode::False: push(Value(false)); break;

        // ── Arithmetic ─────────────────────────────────────────────────────────

        case OpCode::Add: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value(a.as_int() + b.as_int()));
            } else if (a.is_numeric() && b.is_numeric()) {
                push(Value(a.to_float() + b.to_float()));
            } else if (a.is_string() && b.is_string()) {
                push(Value(a.as_string() + b.as_string()));
            } else if (a.is_string()) {
                push(Value(a.as_string() + value_to_string(b)));
            } else {
                throw make_error(CrashKind::TypeMismatch,
                    "cannot add " + std::string(value_type_name(a))
                    + " and " + std::string(value_type_name(b)));
            }
            break;
        }

        case OpCode::Subtract: {
            Value b = pop(); Value a = pop();
            if (a.is_int() && b.is_int()) push(Value(a.as_int() - b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() - b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "cannot subtract non-numeric types");
            break;
        }

        case OpCode::Multiply: {
            Value b = pop(); Value a = pop();
            if (a.is_int() && b.is_int()) push(Value(a.as_int() * b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() * b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "cannot multiply non-numeric types");
            break;
        }

        case OpCode::Divide: {
            Value b = pop(); Value a = pop();
            if (b.is_int() && b.as_int() == 0) throw make_error(CrashKind::DivisionByZero, "division by zero");
            if (b.is_float() && b.as_float() == 0.0) throw make_error(CrashKind::DivisionByZero, "division by zero");
            if (a.is_int() && b.is_int()) push(Value(a.as_int() / b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() / b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "cannot divide non-numeric types");
            break;
        }

        case OpCode::Modulo: {
            Value b = pop(); Value a = pop();
            if (b.is_int() && b.as_int() == 0) throw make_error(CrashKind::DivisionByZero, "modulo by zero");
            if (a.is_int() && b.is_int()) push(Value(a.as_int() % b.as_int()));
            else throw make_error(CrashKind::TypeMismatch, "modulo requires integer operands");
            break;
        }

        case OpCode::Negate: {
            Value a = pop();
            if (a.is_int()) push(Value(-a.as_int()));
            else if (a.is_float()) push(Value(-a.as_float()));
            else throw make_error(CrashKind::TypeMismatch, "cannot negate a non-numeric value");
            break;
        }

        // ── Comparison ─────────────────────────────────────────────────────────

        case OpCode::Equal:        { Value b = pop(); Value a = pop(); push(Value(values_equal(a, b))); break; }
        case OpCode::NotEqual:     { Value b = pop(); Value a = pop(); push(Value(!values_equal(a, b))); break; }
        case OpCode::Less:         { Value b = pop(); Value a = pop();
            if (a.is_int() && b.is_int()) push(Value(a.as_int() < b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() < b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "comparison requires numeric operands");
            break; }
        case OpCode::LessEqual:    { Value b = pop(); Value a = pop();
            if (a.is_int() && b.is_int()) push(Value(a.as_int() <= b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() <= b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "comparison requires numeric operands");
            break; }
        case OpCode::Greater:      { Value b = pop(); Value a = pop();
            if (a.is_int() && b.is_int()) push(Value(a.as_int() > b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() > b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "comparison requires numeric operands");
            break; }
        case OpCode::GreaterEqual: { Value b = pop(); Value a = pop();
            if (a.is_int() && b.is_int()) push(Value(a.as_int() >= b.as_int()));
            else if (a.is_numeric() && b.is_numeric()) push(Value(a.to_float() >= b.to_float()));
            else throw make_error(CrashKind::TypeMismatch, "comparison requires numeric operands");
            break; }

        case OpCode::Not: {
            Value a = pop();
            push(Value(!value_is_truthy(a)));
            break;
        }

        // ── Variables ──────────────────────────────────────────────────────────

        case OpCode::GetLocal: {
            uint8_t slot = read_byte();
            push(stack_[frame().base + slot]);
            break;
        }

        case OpCode::SetLocal: {
            uint8_t slot = read_byte();
            stack_[frame().base + slot] = peek();
            break;
        }

        case OpCode::GetGlobal: {
            const std::string& name = read_string();
            auto it = globals_.find(name);
            if (it == globals_.end()) {
                throw make_error(CrashKind::UndefinedVariable,
                    "undefined variable '" + name + "'");
            }
            push(it->second);
            break;
        }

        case OpCode::SetGlobal: {
            const std::string& name = read_string();
            auto it = globals_.find(name);
            if (it == globals_.end()) {
                throw make_error(CrashKind::UndefinedVariable,
                    "undefined variable '" + name + "'");
            }
            it->second = peek();
            break;
        }

        case OpCode::DefineGlobal: {
            const std::string& name = read_string();
            globals_[name] = pop();
            break;
        }

        // ── Control flow ───────────────────────────────────────────────────────

        case OpCode::Jump: {
            uint16_t offset = read_short();
            frame().ip += offset;
            break;
        }

        case OpCode::JumpIfFalse: {
            uint16_t offset = read_short();
            if (!value_is_truthy(peek())) {
                frame().ip += offset;
            }
            break;
        }

        case OpCode::Loop: {
            uint16_t offset = read_short();
            frame().ip -= offset;
            break;
        }

        // ── Functions ──────────────────────────────────────────────────────────

        case OpCode::Call: {
            uint8_t argc = read_byte();
            Value& callee = peek(argc);

            if (callee.is_builtin()) {

                auto& builtin = callee.as_builtin();
                if (builtin.arity >= 0 && argc != builtin.arity) {
                    throw make_error(CrashKind::ArityMismatch,
                        builtin.name + "() expects " + std::to_string(builtin.arity)
                        + " arguments, got " + std::to_string(argc));
                }
                std::vector<Value> args;
                for (size_t i = argc; i > 0; --i) {
                    args.insert(args.begin(), peek(i - 1));
                }
                pop_n(argc);
                Value fn_val = pop(); // Pop the callee.
                Value result = fn_val.as_builtin().fn(std::move(args), current_span());
                push(std::move(result));
            }
            else if (callee.is_function()) {
                auto& fn = callee.as_function();
                if (!fn.compiled) {
                    throw make_error(CrashKind::InvalidOperation,
                        "cannot call tree-walk function in VM mode");
                }
                if (static_cast<int>(argc) != static_cast<int>(fn.params.size())) {
                    throw make_error(CrashKind::ArityMismatch,
                        fn.name + "() expects " + std::to_string(fn.params.size())
                        + " arguments, got " + std::to_string(argc));
                }
                if (frames_.size() >= FRAMES_MAX) {
                    throw make_error(CrashKind::StackOverflow,
                        "call stack overflow (depth " + std::to_string(FRAMES_MAX) + ")");
                }

                VMFrame new_frame;
                new_frame.chunk = fn.compiled.get();
                new_frame.ip    = 0;
                new_frame.base  = stack_.size() - argc - 1;
                new_frame.name  = fn.name;

                if (fn.vm_upvalues) {
                    new_frame.upvalues = *fn.vm_upvalues;
                }
                frames_.push_back(std::move(new_frame));
            }
            else {
                throw make_error(CrashKind::NotCallable,
                    "tried to call a " + std::string(value_type_name(callee)));
            }
            break;
        }

        case OpCode::Return: {
            Value result = pop();
            size_t base = frame().base;
            close_upvalues(base);
            frames_.pop_back();

            if (frames_.empty()) {
                return;
            }

            stack_.resize(base);
            push(std::move(result));
            break;
        }

        case OpCode::Closure: {
            Value fn_val = read_constant();
            uint8_t upvalue_count = read_byte();
            std::vector<ObjUpvalue*> upvalues(upvalue_count);
            for (uint8_t i = 0; i < upvalue_count; ++i) {
                uint8_t is_local = read_byte();
                uint8_t index = read_byte();
                if (is_local) {
                    upvalues[i] = capture_upvalue(frame().base + index);
                } else {
                    upvalues[i] = frame().upvalues[index];
                }
            }

            if (fn_val.is_function()) {
                auto& fn = const_cast<FunctionValue&>(fn_val.as_function());
                fn.vm_upvalues = std::make_shared<std::vector<ObjUpvalue*>>(std::move(upvalues));
            }
            push(std::move(fn_val));
            break;
        }

        case OpCode::GetUpvalue: {
            uint8_t slot = read_byte();
            ObjUpvalue* uv = frame().upvalues[slot];
            if (uv->is_closed) {
                push(uv->closed);
            } else {
                push(stack_[uv->stack_index]);
            }
            break;
        }

        case OpCode::SetUpvalue: {
            uint8_t slot = read_byte();
            ObjUpvalue* uv = frame().upvalues[slot];
            if (uv->is_closed) {
                uv->closed = peek();
            } else {
                stack_[uv->stack_index] = peek();
            }
            break;
        }

        case OpCode::CloseUpvalue: {
            close_upvalues(stack_.size() - 1);
            pop();
            break;
        }

        // ── Collections ────────────────────────────────────────────────────────

        case OpCode::Array: {
            uint16_t count = read_short();
            auto elements = std::make_shared<std::vector<Value>>();
            elements->resize(count);
            for (size_t i = count; i > 0; --i) {
                (*elements)[i - 1] = pop();
            }
            ArrayValue arr;
            arr.elements = std::move(elements);
            push(Value(std::move(arr)));
            break;
        }

        case OpCode::Index: {
            Value index = pop();
            Value obj = pop();
            if (obj.is_array()) {
                if (!index.is_int()) throw make_error(CrashKind::TypeMismatch, "array index must be int");
                auto& elems = *obj.as_array().elements;
                int64_t idx = index.as_int();
                if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
                    throw make_error(CrashKind::OutOfBounds,
                        "index " + std::to_string(idx) + " out of bounds (length " + std::to_string(elems.size()) + ")");
                push(elems[static_cast<size_t>(idx)]);
            } else if (obj.is_string()) {
                if (!index.is_int()) throw make_error(CrashKind::TypeMismatch, "string index must be int");
                auto& str = obj.as_string();
                int64_t idx = index.as_int();
                if (idx < 0 || idx >= static_cast<int64_t>(str.size()))
                    throw make_error(CrashKind::OutOfBounds, "string index out of bounds");
                push(Value(std::string(1, str[static_cast<size_t>(idx)])));
            } else {
                throw make_error(CrashKind::TypeMismatch, "indexing requires array or string");
            }
            break;
        }

        case OpCode::SetIndex: {
            Value val = pop();
            Value index = pop();
            Value obj = pop();
            if (!obj.is_array()) throw make_error(CrashKind::TypeMismatch, "index assignment requires array");
            if (!index.is_int()) throw make_error(CrashKind::TypeMismatch, "array index must be int");
            auto& elems = *obj.as_array().elements;
            int64_t idx = index.as_int();
            if (idx < 0 || idx >= static_cast<int64_t>(elems.size()))
                throw make_error(CrashKind::OutOfBounds, "index assignment out of bounds");
            elems[static_cast<size_t>(idx)] = std::move(val);
            push(std::move(val));
            break;
        }

        // ── Heap / memory ──────────────────────────────────────────────────────

        case OpCode::NewStruct: {
            uint16_t type_idx = read_short();
            uint8_t field_count = read_byte();
            std::string type_name = value_to_string(frame().chunk->constants[type_idx]);
            std::vector<std::string> field_names(field_count);
            for (size_t i = field_count; i > 0; --i) {
                field_names[i - 1] = pop().as_string();
            }
            std::vector<Value> field_values(field_count);
            for (size_t i = field_count; i > 0; --i) {
                field_values[i - 1] = pop();
            }

            Span span = current_span();
            std::unordered_map<std::string, Value> field_map;
            for (size_t i = 0; i < field_count; ++i) {
                field_map[field_names[i]] = std::move(field_values[i]);
            }
            HeapID id = heap_.allocate(type_name, std::move(field_map), span, frame().name);
            ownership_.record_allocation(id, type_name, type_name, span, frame().name);

            push(Value(HeapRef{id, span}));
            break;
        }

        case OpCode::GetField: {
            const std::string& name = read_string();
            Value obj = pop();
            if (!obj.is_heap_ref()) {
                throw make_error(CrashKind::TypeMismatch,
                    "field access requires a heap object, got " + std::string(value_type_name(obj)));
            }
            HeapID id = obj.as_heap_ref().id;
            if (!heap_.is_alive(id)) {
                auto err = make_error(CrashKind::UseAfterFree,
                    "tried to read field '" + name + "' on a freed object");
                err.related_object = id;
                throw err;
            }
            push(heap_.read_field(id, name, current_span()));
            break;
        }

        case OpCode::SetField: {
            const std::string& name = read_string();
            Value val = pop();
            Value obj = pop();
            if (!obj.is_heap_ref()) {
                throw make_error(CrashKind::TypeMismatch, "field set requires a heap object");
            }
            HeapID id = obj.as_heap_ref().id;
            if (!heap_.is_alive(id)) {
                auto err = make_error(CrashKind::UseAfterFree,
                    "tried to write field '" + name + "' on a freed object");
                err.related_object = id;
                throw err;
            }
            heap_.write_field(id, name, val, current_span());
            push(std::move(val));
            break;
        }

        case OpCode::Ref: {
            Value obj = pop();
            if (!obj.is_heap_ref()) throw make_error(CrashKind::TypeMismatch, "ref() requires a heap object");
            push(obj);
            break;
        }

        case OpCode::Deref: {
            Value obj = pop();
            if (obj.is_nil()) throw make_error(CrashKind::NullDereference, "dereferencing nil");
            if (!obj.is_heap_ref()) throw make_error(CrashKind::TypeMismatch, "deref() requires a heap reference");
            HeapID id = obj.as_heap_ref().id;
            if (!heap_.is_alive(id)) {
                auto err = make_error(CrashKind::UseAfterFree,
                    "dereferencing a freed object");
                err.related_object = id;
                throw err;
            }
            push(obj);
            break;
        }

        case OpCode::Move: {
            Value obj = pop();
            push(obj);
            break;
        }

        case OpCode::Free: {
            Value obj = pop();
            if (!obj.is_heap_ref()) throw make_error(CrashKind::TypeMismatch, "free() requires a heap reference");
            HeapID id = obj.as_heap_ref().id;
            heap_.free(id, current_span(), frame().name);
            ownership_.record_free(id, current_span(), frame().name);
            break;
        }

        // ── Stack ──────────────────────────────────────────────────────────────

        case OpCode::Pop:
            pop();
            break;

        case OpCode::PopN: {
            uint8_t count = read_byte();
            pop_n(count);
            break;
        }

        // ── I/O ────────────────────────────────────────────────────────────────

        case OpCode::Print: {
            uint8_t argc = read_byte();

            std::vector<Value> args(argc);
            for (size_t i = argc; i > 0; --i) {
                args[i - 1] = pop();
            }
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << ' ';
                std::cout << value_to_string(args[i]);
            }
            break;
        }

        case OpCode::Println: {
            uint8_t argc = read_byte();
            std::vector<Value> args(argc);
            for (size_t i = argc; i > 0; --i) {
                args[i - 1] = pop();
            }
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << ' ';
                std::cout << value_to_string(args[i]);
            }
            std::cout << '\n';
            break;
        }

        } // switch
    } // for
}

} // namespace crashlang
