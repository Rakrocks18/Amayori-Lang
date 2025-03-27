#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <iostream>
#include <cassert>
#include <functional>
#include <variant>

namespace amyr {
namespace ast {

// Represents a label, e.g., `'outer` in Rust.
class Label {
public:
    explicit Label(const std::string& ident) : ident(ident) {}

    const std::string& getIdent() const { return ident; }

    friend std::ostream& operator<<(std::ostream& os, const Label& label) {
        os << "label(" << label.ident << ")";
        return os;
    }

private:
    std::string ident;
};

// Represents a lifetime, e.g., `'a` in `&'a i32`.
class Lifetime {
public:
    Lifetime(int id, const std::string& ident) : id(id), ident(ident) {}

    int getId() const { return id; }
    const std::string& getIdent() const { return ident; }

    friend std::ostream& operator<<(std::ostream& os, const Lifetime& lifetime) {
        os << "lifetime(" << lifetime.id << ": " << lifetime.ident << ")";
        return os;
    }

private:
    int id;
    std::string ident;
};

// Represents a segment of a path, e.g., `std`, `String`, or `Box<T>`.
class PathSegment {
public:
    explicit PathSegment(const std::string& ident) : ident(ident) {}

    const std::string& getIdent() const { return ident; }

private:
    std::string ident;
};

// Represents a path, e.g., `std::cmp::PartialEq`.
class Path {
public:
    explicit Path(const std::vector<std::shared_ptr<PathSegment>>& segments)
        : segments(segments) {}

    const std::vector<std::shared_ptr<PathSegment>>& getSegments() const { return segments; }

    bool isGlobal() const {
        return !segments.empty() && segments.front()->getIdent() == "PathRoot";
    }

    friend std::ostream& operator<<(std::ostream& os, const Path& path) {
        os << "Path(";
        for (const auto& segment : path.segments) {
            os << segment->getIdent() << "::";
        }
        os << ")";
        return os;
    }

private:
    std::vector<std::shared_ptr<PathSegment>> segments;
};

// Represents generic arguments, e.g., `<A, B>` or `(A, B) -> C`.
class GenericArgs {
public:
    enum class Kind { AngleBracketed, Parenthesized };

    explicit GenericArgs(Kind kind) : kind(kind) {}

    Kind getKind() const { return kind; }

private:
    Kind kind;
};

// Represents a generic parameter, e.g., `'a`, `T`, or `const N: usize`.
class GenericParam {
public:
    enum class Kind { Lifetime, Type, Const };

    GenericParam(Kind kind, const std::string& ident) : kind(kind), ident(ident) {}

    Kind getKind() const { return kind; }
    const std::string& getIdent() const { return ident; }

private:
    Kind kind;
    std::string ident;
};

// Represents a collection of generic parameters and where clauses.
class Generics {
public:
    void addParam(const std::shared_ptr<GenericParam>& param) {
        params.push_back(param);
    }

    const std::vector<std::shared_ptr<GenericParam>>& getParams() const { return params; }

private:
    std::vector<std::shared_ptr<GenericParam>> params;
};

// Represents a crate, which is the root of the AST.
class Crate {
public:
    explicit Crate(const std::vector<std::shared_ptr<Path>>& items) : items(items) {}

    const std::vector<std::shared_ptr<Path>>& getItems() const { return items; }

private:
    std::vector<std::shared_ptr<Path>> items;
};

// Represents a meta item, e.g., `#[test]`, `#[derive(..)]`, or `#[feature = "foo"]`.
class MetaItem {
public:
    enum class Kind {
        Word,       // E.g., `#[test]`
        List,       // E.g., `#[derive(..)]`
        NameValue   // E.g., `#[feature = "foo"]`
    };

    MetaItem(const std::string& path, Kind kind, const std::string& span)
        : path(path), kind(kind), span(span) {}

    const std::string& getPath() const { return path; }
    Kind getKind() const { return kind; }
    const std::string& getSpan() const { return span; }

private:
    std::string path;
    Kind kind;
    std::string span;
};

// Represents a block, e.g., `{ .. }` in `fn foo() { .. }`.
class Block {
public:
    Block(const std::vector<std::string>& stmts, int id, const std::string& rules, const std::string& span)
        : stmts(stmts), id(id), rules(rules), span(span) {}

    const std::vector<std::string>& getStatements() const { return stmts; }
    int getId() const { return id; }
    const std::string& getRules() const { return rules; }
    const std::string& getSpan() const { return span; }

private:
    std::vector<std::string> stmts;
    int id;
    std::string rules;
    std::string span;
};

// Represents a pattern, e.g., `let x = 42;` or `if let Some(x) = y`.
class Pattern {
public:
    enum class Kind {
        Wild,       // `_`
        Ident,      // `x`
        Path,       // `std::cmp::PartialEq`
        Ref,        // `&x`
        Tuple,      // `(x, y)`
        Slice,      // `[x, y]`
        Or,         // `x | y`
        Box,        // `box x`
        Deref,      // `*x`
        Paren,      // `(x)`
        Guard,      // `x if y`
        Rest,       // `..`
        Never,      // `!`
        Expr,       // `42`
        Range,      // `1..10`
        Err         // Error pattern
    };

    Pattern(int id, Kind kind, const std::string& span)
        : id(id), kind(kind), span(span) {}

    int getId() const { return id; }
    Kind getKind() const { return kind; }
    const std::string& getSpan() const { return span; }

    // Walk through the pattern and apply a function to each sub-pattern.
    void walk(const std::function<bool(const Pattern&)>& visitor) const {
        if (!visitor(*this)) {
            return;
        }

        for (const auto& subpattern : subpatterns) {
            subpattern->walk(visitor);
        }
    }

    void addSubpattern(std::shared_ptr<Pattern> subpattern) {
        subpatterns.push_back(std::move(subpattern));
    }

private:
    int id;
    Kind kind;
    std::string span;
    std::vector<std::shared_ptr<Pattern>> subpatterns;
};

// Represents a single field in a struct pattern, e.g., `x: x` or `y: ref y`.
class PatternField {
public:
    PatternField(const std::string& ident, std::shared_ptr<Pattern> pattern, bool is_shorthand, const std::string& span)
        : ident(ident), pattern(std::move(pattern)), is_shorthand(is_shorthand), span(span) {}

    const std::string& getIdent() const { return ident; }
    const std::shared_ptr<Pattern>& getPattern() const { return pattern; }
    bool isShorthand() const { return is_shorthand; }
    const std::string& getSpan() const { return span; }

private:
    std::string ident;
    std::shared_ptr<Pattern> pattern;
    bool is_shorthand;
    std::string span;
};

// Represents a reference type, e.g., `&x` or `&mut x`.
class ByRef {
public:
    enum class Mutability {
        Mutable,
        Immutable
    };

    ByRef(Mutability mutability) : mutability(mutability) {}

    Mutability getMutability() const { return mutability; }

private:
    Mutability mutability;
};

// Represents the mode of a binding (e.g., `mut`, `ref mut`, etc.).
class BindingMode {
public:
    enum class ByRef { No, Yes };
    enum class Mutability { Not, Mut };

    BindingMode(ByRef by_ref, Mutability mutability)
        : by_ref(by_ref), mutability(mutability) {}

    static const BindingMode NONE;
    static const BindingMode REF;
    static const BindingMode MUT;
    static const BindingMode REF_MUT;
    static const BindingMode MUT_REF;
    static const BindingMode MUT_REF_MUT;

    std::string prefixStr() const {
        if (by_ref == ByRef::No && mutability == Mutability::Not) return "";
        if (by_ref == ByRef::Yes && mutability == Mutability::Not) return "ref ";
        if (by_ref == ByRef::No && mutability == Mutability::Mut) return "mut ";
        if (by_ref == ByRef::Yes && mutability == Mutability::Mut) return "ref mut ";
        return "";
    }

private:
    ByRef by_ref;
    Mutability mutability;
};

const BindingMode BindingMode::NONE = BindingMode(BindingMode::ByRef::No, BindingMode::Mutability::Not);
const BindingMode BindingMode::REF = BindingMode(BindingMode::ByRef::Yes, BindingMode::Mutability::Not);
const BindingMode BindingMode::MUT = BindingMode(BindingMode::ByRef::No, BindingMode::Mutability::Mut);
const BindingMode BindingMode::REF_MUT = BindingMode(BindingMode::ByRef::Yes, BindingMode::Mutability::Mut);
const BindingMode BindingMode::MUT_REF = BindingMode(BindingMode::ByRef::Yes, BindingMode::Mutability::Not);
const BindingMode BindingMode::MUT_REF_MUT = BindingMode(BindingMode::ByRef::Yes, BindingMode::Mutability::Mut);

// Represents the end of a range (e.g., `..`, `..=`, `...`).
class RangeEnd {
public:
    enum class Kind { Included, Excluded };
    enum class Syntax { DotDotDot, DotDotEq };

    RangeEnd(Kind kind, Syntax syntax = Syntax::DotDotEq)
        : kind(kind), syntax(syntax) {}

    Kind getKind() const { return kind; }
    Syntax getSyntax() const { return syntax; }

private:
    Kind kind;
    Syntax syntax;
};

// Represents binary operators (e.g., `+`, `-`, `*`, etc.).
class BinOpKind {
public:
    enum class Kind {
        Add, Sub, Mul, Div, Rem, And, Or, BitXor, BitAnd, BitOr,
        Shl, Shr, Eq, Lt, Le, Ne, Ge, Gt
    };

    explicit BinOpKind(Kind kind) : kind(kind) {}

    std::string asStr() const {
        switch (kind) {
            case Kind::Add: return "+";
            case Kind::Sub: return "-";
            case Kind::Mul: return "*";
            case Kind::Div: return "/";
            case Kind::Rem: return "%";
            case Kind::And: return "&&";
            case Kind::Or: return "||";
            case Kind::BitXor: return "^";
            case Kind::BitAnd: return "&";
            case Kind::BitOr: return "|";
            case Kind::Shl: return "<<";
            case Kind::Shr: return ">>";
            case Kind::Eq: return "==";
            case Kind::Lt: return "<";
            case Kind::Le: return "<=";
            case Kind::Ne: return "!=";
            case Kind::Ge: return ">=";
            case Kind::Gt: return ">";
        }
        return "";
    }

    bool isLazy() const {
        return kind == Kind::And || kind == Kind::Or;
    }

    bool isComparison() const {
        switch (kind) {
            case Kind::Eq:
            case Kind::Ne:
            case Kind::Lt:
            case Kind::Le:
            case Kind::Gt:
            case Kind::Ge:
                return true;
            default:
                return false;
        }
    }

    bool isByValue() const {
        return !isComparison();
    }

private:
    Kind kind;
};

// Represents unary operators (e.g., `*`, `!`, `-`).
class UnOp {
public:
    enum class Kind { Deref, Not, Neg };

    explicit UnOp(Kind kind) : kind(kind) {}

    Kind getKind() const { return kind; }

    std::string asStr() const {
        switch (kind) {
            case Kind::Deref: return "*";
            case Kind::Not: return "!";
            case Kind::Neg: return "-";
        }
        return "";
    }

    bool isByValue() const {
        return kind == Kind::Neg || kind == Kind::Not;
    }

private:
    Kind kind;
};

// Represents a statement in the AST.
class Stmt {
public:
    enum class Kind { Let, Item, Expr, Semi, Empty, MacCall };

    Stmt(int id, Kind kind, const std::string& span)
        : id(id), kind(kind), span(span) {}

    int getId() const { return id; }
    Kind getKind() const { return kind; }
    const std::string& getSpan() const { return span; }

    bool hasTrailingSemicolon() const {
        return kind == Kind::Semi || kind == Kind::MacCall;
    }

    bool isItem() const { return kind == Kind::Item; }
    bool isExpr() const { return kind == Kind::Expr; }

private:
    int id;
    Kind kind;
    std::string span;
};

// Represents a local variable declaration (e.g., `let x = 42;`).
class Local {
public:
    enum class Kind { Decl, Init, InitElse };

    Local(int id, const std::string& pattern, Kind kind, const std::string& span)
        : id(id), pattern(pattern), kind(kind), span(span) {}

    int getId() const { return id; }
    const std::string& getPattern() const { return pattern; }
    Kind getKind() const { return kind; }
    const std::string& getSpan() const { return span; }

private:
    int id;
    std::string pattern;
    Kind kind;
    std::string span;
};

// Represents a match arm in a `match` expression.
class MatchArm {
public:
    MatchArm(const std::string& pattern, const std::string& guard, const std::string& body, const std::string& span)
        : pattern(pattern), guard(guard), body(body), span(span) {}

    const std::string& getPattern() const { return pattern; }
    const std::string& getGuard() const { return guard; }
    const std::string& getBody() const { return body; }
    const std::string& getSpan() const { return span; }

private:
    std::string pattern;
    std::string guard;
    std::string body;
    std::string span;
};

// Represents a block check mode (e.g., `unsafe`).
class BlockCheckMode {
public:
    enum class Kind { Default, Unsafe };

    BlockCheckMode(Kind kind) : kind(kind) {}

    Kind getKind() const { return kind; }

private:
    Kind kind;
};

// Represents an anonymous constant (e.g., `const` in array lengths).
class AnonConst {
public:
    AnonConst(int id, const std::string& value)
        : id(id), value(value) {}

    int getId() const { return id; }
    const std::string& getValue() const { return value; }

private:
    int id;
    std::string value;
};

// Represents an expression in the AST.
class Expr {
public:
    enum class Kind {
        Path, Block, Binary, Unary, Call, Lit, Match, Array, Tuple, Struct,
        Range, Paren, AddrOf, Repeat, Try, Err
    };

    Expr(int id, Kind kind, const std::string& span)
        : id(id), kind(kind), span(span) {}

    int getId() const { return id; }
    Kind getKind() const { return kind; }
    const std::string& getSpan() const { return span; }

    bool isPotentialTrivialConstArg(bool allow_mgca_arg) const {
        if (allow_mgca_arg) {
            return kind == Kind::Path;
        } else {
            return kind == Kind::Path; // Simplified for now
        }
    }

    const Expr* maybeUnwrapBlock() const {
        if (kind == Kind::Block) {
            return this; // Simplified for now
        }
        return this;
    }

    bool isApproximatelyPattern() const {
        return kind == Kind::Array || kind == Kind::Path || kind == Kind::Struct;
    }

private:
    int id;
    Kind kind;
    std::string span;
};

// Represents the kind of borrow in an `AddrOf` expression (e.g., `&place` or `&raw const place`).
class BorrowKind {
public:
    enum class Kind { Ref, Raw };

    explicit BorrowKind(Kind kind) : kind(kind) {}

    Kind getKind() const { return kind; }

private:
    Kind kind;
};

// Represents the kind of pattern in Rust (e.g., `_`, `x`, `&x`, etc.).
class PatKind {
public:
    enum class Kind {
        Wild, Ident, Struct, TupleStruct, Or, Path, Tuple, Box, Deref, Ref,
        Expr, Range, Slice, Rest, Never, Guard, Paren, MacCall, Err
    };

    explicit PatKind(Kind kind) : kind(kind) {}

    Kind getKind() const { return kind; }

private:
    Kind kind;
};

// Represents whether the `..` is present in a struct fields pattern.
class PatFieldsRest {
public:
    enum class Kind { Rest, Recovered, None };

    explicit PatFieldsRest(Kind kind) : kind(kind) {}

    Kind getKind() const { return kind; }

private:
    Kind kind;
};

// Represents the limits of a range (inclusive or exclusive).
enum class RangeLimits {
    HalfOpen, // ".."
    Closed    // "..="
};

inline std::string rangeLimitsAsStr(RangeLimits limits) {
    switch (limits) {
        case RangeLimits::HalfOpen: return "..";
        case RangeLimits::Closed: return "..=";
    }
    return "";
}

// Represents a method call (e.g., `x.foo::<Bar, Baz>(a, b, c)`).
class MethodCall {
public:
    std::string method_name;
    std::vector<std::shared_ptr<class Expr>> args;
    std::shared_ptr<class Expr> receiver;

    MethodCall(std::string name, std::shared_ptr<class Expr> recv, std::vector<std::shared_ptr<class Expr>> arguments)
        : method_name(std::move(name)), receiver(std::move(recv)), args(std::move(arguments)) {}
};

// Represents the kind of a match expression.
enum class MatchKind {
    Prefix,  // `match expr { ... }`
    Postfix  // `expr.match { ... }`
};

// Represents the kind of a yield expression.
class YieldKind {
public:
    enum class Kind { Prefix, Postfix };

    YieldKind(Kind kind, std::shared_ptr<class Expr> expr = nullptr)
        : kind(kind), expr(std::move(expr)) {}

    Kind getKind() const { return kind; }
    const std::shared_ptr<class Expr>& getExpr() const { return expr; }

private:
    Kind kind;
    std::shared_ptr<class Expr> expr;
};

// Represents the type of a for loop (e.g., `for` or `for await`).
enum class ForLoopKind {
    For,
    ForAwait
};

// Represents the type of a generator block (e.g., `async`, `gen`, or `async gen`).
enum class GenBlockKind {
    Async,
    Gen,
    AsyncGen
};

inline std::string genBlockKindModifier(GenBlockKind kind) {
    switch (kind) {
        case GenBlockKind::Async: return "async";
        case GenBlockKind::Gen: return "gen";
        case GenBlockKind::AsyncGen: return "async gen";
    }
    return "";
}

// Represents a closure.
class Closure {
public:
    std::string capture_clause;
    std::shared_ptr<class Expr> body;
    std::vector<std::string> params;

    Closure(std::string capture, std::vector<std::string> parameters, std::shared_ptr<class Expr> closure_body)
        : capture_clause(std::move(capture)), params(std::move(parameters)), body(std::move(closure_body)) {}
};

// Represents a range expression.
class RangeExpr {
public:
    std::shared_ptr<class Expr> start;
    std::shared_ptr<class Expr> end;
    RangeLimits limits;

    RangeExpr(std::shared_ptr<class Expr> start_expr, std::shared_ptr<class Expr> end_expr, RangeLimits range_limits)
        : start(std::move(start_expr)), end(std::move(end_expr)), limits(range_limits) {}
};

// Represents a struct expression (e.g., `Foo { x: 1, y: 2 }`).
class StructExpr {
public:
    std::string struct_name;
    std::vector<std::pair<std::string, std::shared_ptr<class Expr>>> fields;
    bool has_rest;

    StructExpr(std::string name, std::vector<std::pair<std::string, std::shared_ptr<class Expr>>> field_list, bool rest)
        : struct_name(std::move(name)), fields(std::move(field_list)), has_rest(rest) {}
};

// Represents the kind of an expression.
class Expr {
public:
    enum class Kind {
        Array,
        Call,
        MethodCall,
        Binary,
        Unary,
        Literal,
        Cast,
        If,
        While,
        ForLoop,
        Match,
        Closure,
        Block,
        Range,
        Struct,
        Yield,
        Err
    };

    Kind kind;

    explicit Expr(Kind kind) : kind(kind) {}
    virtual ~Expr() = default;
};

// Represents a binary operation expression (e.g., `a + b`).
class BinaryExpr : public Expr {
public:
    std::shared_ptr<Expr> lhs;
    std::shared_ptr<Expr> rhs;
    std::string op;

    BinaryExpr(std::shared_ptr<Expr> left, std::shared_ptr<Expr> right, std::string operation)
        : Expr(Kind::Binary), lhs(std::move(left)), rhs(std::move(right)), op(std::move(operation)) {}
};

// Represents a unary operation expression (e.g., `!x`).
class UnaryExpr : public Expr {
public:
    std::shared_ptr<Expr> operand;
    std::string op;

    UnaryExpr(std::shared_ptr<Expr> expr, std::string operation)
        : Expr(Kind::Unary), operand(std::move(expr)), op(std::move(operation)) {}
};

// Represents a literal expression (e.g., `42`, `"hello"`).
class LiteralExpr : public Expr {
public:
    std::variant<int, double, std::string> value;

    explicit LiteralExpr(std::variant<int, double, std::string> val)
        : Expr(Kind::Literal), value(std::move(val)) {}
};

// Represents a block expression (e.g., `{ ... }`).
class BlockExpr : public Expr {
public:
    std::vector<std::shared_ptr<Expr>> statements;

    explicit BlockExpr(std::vector<std::shared_ptr<Expr>> stmts)
        : Expr(Kind::Block), statements(std::move(stmts)) {}
};

// Represents a function call expression (e.g., `foo(a, b)`).
class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<std::shared_ptr<Expr>> args;

    CallExpr(std::string function_name, std::vector<std::shared_ptr<Expr>> arguments)
        : Expr(Kind::Call), callee(std::move(function_name)), args(std::move(arguments)) {}
};

// Represents a match expression.
class MatchExpr : public Expr {
public:
    std::shared_ptr<Expr> condition;
    std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Expr>>> arms;

    MatchExpr(std::shared_ptr<Expr> cond, std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Expr>>> match_arms)
        : Expr(Kind::Match), condition(std::move(cond)), arms(std::move(match_arms)) {}
};

// Represents the kind of a literal.
class LitKind {
public:
    enum class Kind {
        Str,       // String literal
        ByteStr,   // Byte string literal
        CStr,      // C string literal
        Byte,      // Byte char
        Char,      // Character literal
        Int,       // Integer literal
        Float,     // Float literal
        Bool,      // Boolean literal
        Err        // Error placeholder
    };

    LitKind(Kind kind, std::variant<std::string, std::vector<uint8_t>, char, int, double, bool> value)
        : kind(kind), value(std::move(value)) {}

    Kind getKind() const { return kind; }

    bool isStr() const { return kind == Kind::Str; }
    bool isByteStr() const { return kind == Kind::ByteStr; }
    bool isNumeric() const { return kind == Kind::Int || kind == Kind::Float; }
    bool isSuffixed() const { return kind == Kind::Int || kind == Kind::Float; }

    const std::variant<std::string, std::vector<uint8_t>, char, int, double, bool>& getValue() const {
        return value;
    }

private:
    Kind kind;
    std::variant<std::string, std::vector<uint8_t>, char, int, double, bool> value;
};

// Represents a mutable type (e.g., `&mut T`).
class MutTy {
public:
    std::shared_ptr<class Ty> ty;
    bool isMutable;

    MutTy(std::shared_ptr<class Ty> ty, bool isMutable)
        : ty(std::move(ty)), isMutable(isMutable) {}
};

// Represents a function signature.
class FnSig {
public:
    std::string header;
    std::shared_ptr<class FnDecl> decl;
    std::string span;

    FnSig(std::string header, std::shared_ptr<class FnDecl> decl, std::string span)
        : header(std::move(header)), decl(std::move(decl)), span(std::move(span)) {}
};

// Represents floating-point types.
enum class FloatTy {
    F16,
    F32,
    F64,
    F128
};

inline std::string floatTyName(FloatTy ty) {
    switch (ty) {
        case FloatTy::F16: return "f16";
        case FloatTy::F32: return "f32";
        case FloatTy::F64: return "f64";
        case FloatTy::F128: return "f128";
    }
    return "";
}

// Represents integer types.
enum class IntTy {
    Isize,
    I8,
    I16,
    I32,
    I64,
    I128
};

inline std::string intTyName(IntTy ty) {
    switch (ty) {
        case IntTy::Isize: return "isize";
        case IntTy::I8: return "i8";
        case IntTy::I16: return "i16";
        case IntTy::I32: return "i32";
        case IntTy::I64: return "i64";
        case IntTy::I128: return "i128";
    }
    return "";
}

// Represents unsigned integer types.
enum class UintTy {
    Usize,
    U8,
    U16,
    U32,
    U64,
    U128
};

inline std::string uintTyName(UintTy ty) {
    switch (ty) {
        case UintTy::Usize: return "usize";
        case UintTy::U8: return "u8";
        case UintTy::U16: return "u16";
        case UintTy::U32: return "u32";
        case UintTy::U64: return "u64";
        case UintTy::U128: return "u128";
    }
    return "";
}

// Represents a type in the AST.
class Ty {
public:
    enum class Kind {
        Slice,
        Array,
        Ptr,
        Ref,
        BareFn,
        Never,
        Tuple,
        Path,
        TraitObject,
        ImplTrait,
        Paren,
        Infer,
        ImplicitSelf,
        Err
    };

    Ty(Kind kind, std::string span)
        : kind(kind), span(std::move(span)) {}

    Kind getKind() const { return kind; }
    const std::string& getSpan() const { return span; }

private:
    Kind kind;
    std::string span;
};

// Represents a bare function type (e.g., `fn(usize) -> bool`).
class BareFnTy {
public:
    std::string safety;
    std::string ext;
    std::vector<std::string> genericParams;
    std::shared_ptr<class FnDecl> decl;
    std::string declSpan;

    BareFnTy(std::string safety, std::string ext, std::vector<std::string> genericParams,
             std::shared_ptr<class FnDecl> decl, std::string declSpan)
        : safety(std::move(safety)), ext(std::move(ext)), genericParams(std::move(genericParams)),
          decl(std::move(decl)), declSpan(std::move(declSpan)) {}
};

// Represents a trait object syntax.
enum class TraitObjectSyntax {
    Dyn,
    DynStar,
    None
};

// Represents inline assembly options.
class InlineAsmOptions {
public:
    enum class Option {
        PURE,
        NOMEM,
        READONLY,
        PRESERVES_FLAGS,
        NORETURN,
        NOSTACK,
        ATT_SYNTAX,
        RAW,
        MAY_UNWIND
    };

    void addOption(Option option) { options.push_back(option); }

    std::vector<std::string> humanReadableNames() const {
        std::vector<std::string> names;
        for (const auto& option : options) {
            switch (option) {
                case Option::PURE: names.push_back("pure"); break;
                case Option::NOMEM: names.push_back("nomem"); break;
                case Option::READONLY: names.push_back("readonly"); break;
                case Option::PRESERVES_FLAGS: names.push_back("preserves_flags"); break;
                case Option::NORETURN: names.push_back("noreturn"); break;
                case Option::NOSTACK: names.push_back("nostack"); break;
                case Option::ATT_SYNTAX: names.push_back("att_syntax"); break;
                case Option::RAW: names.push_back("raw"); break;
                case Option::MAY_UNWIND: names.push_back("may_unwind"); break;
            }
        }
        return names;
    }

private:
    std::vector<Option> options;
};

// Represents a piece of an inline assembly template.
class InlineAsmTemplatePiece {
public:
    enum class Kind { String, Placeholder };

    InlineAsmTemplatePiece(const std::string& str) : kind(Kind::String), str(str) {}
    InlineAsmTemplatePiece(size_t operandIdx, std::optional<char> modifier)
        : kind(Kind::Placeholder), operandIdx(operandIdx), modifier(modifier) {}

    std::string toString() const {
        if (kind == Kind::String) {
            return str;
        } else {
            return "{" + std::to_string(operandIdx) + (modifier ? ":" + std::string(1, *modifier) : "") + "}";
        }
    }

private:
    Kind kind;
    std::string str;
    size_t operandIdx;
    std::optional<char> modifier;
};

// Represents an inline assembly symbol.
class InlineAsmSym {
public:
    InlineAsmSym(int id, const std::string& path) : id(id), path(path) {}

private:
    int id;
    std::string path;
};

// Represents an inline assembly operand.
class InlineAsmOperand {
public:
    enum class Kind { In, Out, InOut, SplitInOut, Const, Sym, Label };

    InlineAsmOperand(Kind kind) : kind(kind) {}

private:
    Kind kind;
};

// Represents the type of an inline assembly macro.
enum class AsmMacro { Asm, GlobalAsm, NakedAsm };

// Represents inline assembly.
class InlineAsm {
public:
    InlineAsm(AsmMacro macro, const std::vector<InlineAsmTemplatePiece>& templatePieces)
        : macro(macro), templatePieces(templatePieces) {}

private:
    AsmMacro macro;
    std::vector<InlineAsmTemplatePiece> templatePieces;
};

// Represents a function parameter.
class Param {
public:
    Param(const std::string& name, const std::string& type) : name(name), type(type) {}

private:
    std::string name;
    std::string type;
};

// Represents a function declaration.
class FnDecl {
public:
    FnDecl(const std::vector<Param>& params, const std::string& returnType)
        : params(params), returnType(returnType) {}

private:
    std::vector<Param> params;
    std::string returnType;
};

// Represents the kind of a module.
class ModKind {
public:
    enum class Kind { Loaded, Unloaded };

    ModKind(Kind kind) : kind(kind) {}

private:
    Kind kind;
};

// Represents spans for a module.
class ModSpans {
public:
    ModSpans(const std::string& innerSpan, const std::string& injectUseSpan)
        : innerSpan(innerSpan), injectUseSpan(injectUseSpan) {}

private:
    std::string innerSpan;
    std::string injectUseSpan;
};

// Represents a foreign module declaration.
class ForeignMod {
public:
    ForeignMod(const std::string& externSpan, const std::string& safety, const std::optional<std::string>& abi)
        : externSpan(externSpan), safety(safety), abi(abi) {}

private:
    std::string externSpan;
    std::string safety;
    std::optional<std::string> abi;
};

// Represents an enum definition.
class EnumDef {
public:
    void addVariant(const std::string& name) { variants.push_back(name); }

private:
    std::vector<std::string> variants;
};

// Represents a tree of paths sharing common prefixes.
class UseTree {
public:
    enum class Kind { Simple, Nested, Glob };

    UseTree(const std::string& prefix, Kind kind) : prefix(prefix), kind(kind) {}

private:
    std::string prefix;
    Kind kind;
};

// Represents the style of an attribute.
class AttrStyle {
public:
    enum class Kind { Outer, Inner };

    AttrStyle(Kind kind) : kind(kind) {}

private:
    Kind kind;
};

// Represents an attribute.
class Attribute {
public:
    Attribute(const std::string& kind, const std::string& span) : kind(kind), span(span) {}

private:
    std::string kind;
    std::string span;
};

// Represents a trait reference.
class TraitRef {
public:
    TraitRef(const std::string& path, int refId) : path(path), refId(refId) {}

private:
    std::string path;
    int refId;
};

// Represents a polymorphic trait reference.
class PolyTraitRef {
public:
    PolyTraitRef(const std::string& path, const std::string& span)
        : traitRef(path, 0), span(span) {}

private:
    TraitRef traitRef;
    std::string span;
};

// Represents visibility of an item.
class Visibility {
public:
    enum class Kind { Public, Restricted, Inherited };

    Visibility(Kind kind) : kind(kind) {}

private:
    Kind kind;
};

// Represents an item definition.
class Item {
public:
    Item(const std::string& name, const std::string& kind) : name(name), kind(kind) {}

private:
    std::string name;
    std::string kind;
};

// Represents a function header.
class FnHeader {
public:
    FnHeader(const std::string& safety, const std::string& coroutineKind, const std::string& constness)
        : safety(safety), coroutineKind(coroutineKind), constness(constness) {}

private:
    std::string safety;
    std::string coroutineKind;
    std::string constness;
};

// Represents a trait.
class Trait {
public:
    Trait(const std::string& safety, const std::string& isAuto) : safety(safety), isAuto(isAuto) {}

private:
    std::string safety;
    std::string isAuto;
};

// Represents a type alias.
class TyAlias {
public:
    TyAlias(const std::string& defaultness, const std::string& bounds)
        : defaultness(defaultness), bounds(bounds) {}

private:
    std::string defaultness;
    std::string bounds;
};

// Represents an implementation block.
class Impl {
public:
    Impl(const std::string& defaultness, const std::string& safety, const std::string& constness)
        : defaultness(defaultness), safety(safety), constness(constness) {}

private:
    std::string defaultness;
    std::string safety;
    std::string constness;
};

// Represents a function.
class Fn {
public:
    Fn(const std::string& defaultness, const std::string& sig) : defaultness(defaultness), sig(sig) {}

private:
    std::string defaultness;
    std::string sig;
};

// Represents a delegation.
class Delegation {
public:
    Delegation(int id, const std::string& path, const std::optional<std::string>& rename, bool fromGlob)
        : id(id), path(path), rename(rename), fromGlob(fromGlob) {}

private:
    int id;
    std::string path;
    std::optional<std::string> rename;
    bool fromGlob;
};

// Represents a delegation macro.
class DelegationMac {
public:
    DelegationMac(const std::string& prefix, const std::optional<std::vector<std::pair<std::string, std::optional<std::string>>>>& suffixes)
        : prefix(prefix), suffixes(suffixes) {}

private:
    std::string prefix;
    std::optional<std::vector<std::pair<std::string, std::optional<std::string>>>> suffixes;
};

// Represents a static item.
class StaticItem {
public:
    StaticItem(const std::string& type, const std::string& safety, const std::string& mutability)
        : type(type), safety(safety), mutability(mutability) {}

private:
    std::string type;
    std::string safety;
    std::string mutability;
};

// Represents a constant item.
class ConstItem {
public:
    ConstItem(const std::string& defaultness, const std::string& type)
        : defaultness(defaultness), type(type) {}

private:
    std::string defaultness;
    std::string type;
};

// Represents the kind of an item.
class ItemKind {
public:
    enum class Kind {
        ExternCrate,
        Use,
        Static,
        Const,
        Fn,
        Mod,
        ForeignMod,
        GlobalAsm,
        TyAlias,
        Enum,
        Struct,
        Union,
        Trait,
        TraitAlias,
        Impl,
        MacCall,
        MacroDef,
        Delegation,
        DelegationMac
    };

    ItemKind(Kind kind) : kind(kind) {}

    std::string article() const {
        switch (kind) {
            case Kind::Use:
            case Kind::Static:
            case Kind::Const:
            case Kind::Fn:
            case Kind::Mod:
            case Kind::GlobalAsm:
            case Kind::TyAlias:
            case Kind::Struct:
            case Kind::Union:
            case Kind::Trait:
            case Kind::TraitAlias:
            case Kind::MacroDef:
            case Kind::Delegation:
            case Kind::DelegationMac:
                return "a";
            case Kind::ExternCrate:
            case Kind::ForeignMod:
            case Kind::Enum:
            case Kind::Impl:
                return "an";
        }
        return "";
    }

    std::string descr() const {
        switch (kind) {
            case Kind::ExternCrate: return "extern crate";
            case Kind::Use: return "`use` import";
            case Kind::Static: return "static item";
            case Kind::Const: return "constant item";
            case Kind::Fn: return "function";
            case Kind::Mod: return "module";
            case Kind::ForeignMod: return "extern block";
            case Kind::GlobalAsm: return "global asm item";
            case Kind::TyAlias: return "type alias";
            case Kind::Enum: return "enum";
            case Kind::Struct: return "struct";
            case Kind::Union: return "union";
            case Kind::Trait: return "trait";
            case Kind::TraitAlias: return "trait alias";
            case Kind::MacCall: return "item macro invocation";
            case Kind::MacroDef: return "macro definition";
            case Kind::Impl: return "implementation";
            case Kind::Delegation: return "delegated function";
            case Kind::DelegationMac: return "delegation";
        }
        return "";
    }

private:
    Kind kind;
};

// Represents an associated item.
class AssocItem {
public:
    AssocItem(const std::string& name, const std::string& kind) : name(name), kind(kind) {}

private:
    std::string name;
    std::string kind;
};

// Represents the kind of an associated item.
class AssocItemKind {
public:
    enum class Kind { Const, Fn, Type, MacCall, Delegation, DelegationMac };

    AssocItemKind(Kind kind) : kind(kind) {}

private:
    Kind kind;
};

// Represents a foreign item.
class ForeignItem {
public:
    ForeignItem(const std::string& name, const std::string& kind) : name(name), kind(kind) {}

private:
    std::string name;
    std::string kind;
};

// Represents the kind of a foreign item.
class ForeignItemKind {
public:
    enum class Kind { Static, Fn, TyAlias, MacCall };

    ForeignItemKind(Kind kind) : kind(kind) {}

private:
    Kind kind;
};

} // namespace ast
} // namespace amyr
