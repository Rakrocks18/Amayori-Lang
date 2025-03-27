/* Since this compiler draws its main structure and components from Rust,
we need to make the basics like the parser and lexer very similar. 
We will add our own flare to it in the future as we need to cross compile languages,
but for now, its working will be pretty similar to how Rust's lexer works on two levels

The following is the official starting comming in compiler/rustc_lexer/src/lib.rs:

//! Low-level Rust lexer.
//!
//! The idea with `rustc_lexer` is to make a reusable library,
//! by separating out pure lexing and rustc-specific concerns, like spans,
//! error reporting, and interning. So, rustc_lexer operates directly on `&str`,
//! produces simple tokens which are a pair of type-tag and a bit of original text,
//! and does not report errors, instead storing them as flags on the token.
//!
//! Tokens produced by this lexer are not yet ready for parsing the Rust syntax.
//! For that see [`rustc_parse::lexer`], which converts this basic token stream
//! into wide tokens used by actual parser.
//!
//! The purpose of this crate is to convert raw sources into a labeled sequence
//! of well-known token types, so building an actual Rust token stream will
//! be easier.
//!
//! The main entity of this crate is the [`TokenKind`] enum which represents common
//! lexeme types.
//!
//! [`rustc_parse::lexer`]: ../rustc_parse/lexer/index.html
*/
#pragma once

#include<optional>
#include<cassert>

#include "cursor.hpp"
#include "unicode_escape.hpp"

/* Parsed token.
It doesn't contain information about data that has been parsed,
only the type of the token and its size.*/
struct Token {
    public:
        TokenKind kind;
        unsigned int len;

        Token(TokenKind kind, unsigned int len)
            : kind (kind), len (len) {}
};

enum class DocStyle {
    Outer,
    Inner
};

enum class Base: uint8_t {
    // Literal starts with 0b
    Binary = 2,

    /// Literal starts with "0o".
    Octal = 8,

    /// Literal doesn't contain a prefix.
    Decimal = 10,
    
    /// Literal starts with "0x".
    Hexadecimal = 16

};

/// Enum representing the literal types supported by the lexer.
///
/// Note that the suffix is *not* considered when deciding the `LiteralKind` in
/// this type. This means that float literals like `1f32` are classified by this
/// type as `Int`. 
namespace namespace_Literal {
    
    // `12_u8`, `0o100`, `0b120i99`, `1f32`
    struct Int {
        Base base;
        bool empty_int;

        Int(Base base, bool empty_int)
            : base(base), empty_int (empty_int) {}
    };

    
    // `12.34f32`, `1e3`, but not `1f32`.
    struct Float {
        Base base;
        bool empty_exponent;

        Float(Base base, bool empty_exponent)
            : base(base), empty_exponent (empty_exponent) {}
    };

    
    // `'a'`, `'\\'`, `'''`, `';`
    struct Char {
        bool terminated;

        Char (bool terminated)
            : terminated (terminated) {}
    };

    // `b'a'`, `b'\\'`, `b'''`, `b';`
    struct Byte {
        bool terminated;

        Byte (bool terminated)
            : terminated (terminated) {}
    };

    // `"abc"`, `"abc`
    struct Str {
        bool terminated;

        Str (bool terminated)
            : terminated (terminated) {}
    };

    // `b"abc"`, `b"abc`
    struct ByteStr {
        bool terminated;

        ByteStr (bool terminated)
            : terminated (terminated) {}
    };

    // `c"abc"`, `c"abc`
    struct CStr {
        bool terminated;

        CStr (bool terminated)
            : terminated (terminated) {}
    };

    // `r"abc"`, `r#"abc"#`, `r####"ab"###"c"####`, `r#"a`. `None` indicates an invalid literal.
    struct RawStr {
        std::optional<unsigned short> n_hashes;

        RawStr (std::optional<unsigned short> n_hashes) 
            :n_hashes (n_hashes) {}
    };

    // `br"abc"`, `br#"abc"#`, `br####"ab"###"c"####`, `br#"a`. `None` indicates an invalid literal.
    struct RawByteStr {
        std::optional<unsigned short> n_hashes;

        RawByteStr (std::optional<unsigned short> n_hashes) 
            :n_hashes (n_hashes) {}
    };

    // `cr"abc"`, "cr#"abc"#", `cr#"a`. `None` indicates an invalid literal.
    struct RawCStr {
        std::optional<unsigned short> n_hashes;

        RawCStr (std::optional<unsigned short> n_hashes) 
            :n_hashes (n_hashes) {}
    };
}

// Use a variant to hold one of the literal kinds.
using LiteralKind = std::variant<
    namespace_Literal::Int,
    namespace_Literal::Float,
    namespace_Literal::Char,
    namespace_Literal::Byte,
    namespace_Literal::Str,
    namespace_Literal::ByteStr,
    namespace_Literal::CStr,
    namespace_Literal::RawStr,
    namespace_Literal::RawByteStr,
    namespace_Literal::RawCStr
>;

// Variants with associated data:
// A line comment, e.g. `// comment`.
struct LineComment {
    std::optional<DocStyle> doc_style;
};

//A block comment, e.g. `/* block comment */`.
//Block comments can be recursive, so a sequence like `/* /* */`
//will not be considered terminated and will result in a parsing error.*/
struct BlockComment {
    std::optional<DocStyle> doc_style;
    bool terminated;
};

/*A literal token carries a literal kind and the start position of its suffix.
Literals, e.g. `12u8`, `1.0e-40`, `b"123"`. Note that `_` is an invalid
suffix, but may be present here on string and float literals. Users of
this type will need to check for and reject that case.

See [LiteralKind] for more details.
*/
struct Literal {
    LiteralKind kind;
    uint32_t suffix_start;
};

// A lifetime, e.g. `'a`
struct Lifetime {
    bool starts_with_number;
};

// The following tokens carry no extra data; they are used as “tags.”
// Any whitespace character sequence.
struct Whitespace {};

// An identifier or keyword, e.g. `ident` or `continue`.
struct Ident {};

// An identifier or keyword, e.g. `ident` or `continue`.
struct InvalidIdent {};

// A raw identifier, e.g. "r#ident".
struct RawIdent {};

/*
An unknown literal prefix, like `foo#`, `foo'`, `foo"`. Excludes
literal prefixes that contain emoji, which are considered "invalid".

Note that only the
prefix (`foo`) is included in the token, not the separator (which is
lexed as its own distinct token). In Rust 2021 and later, reserved
prefixes are reported as errors; in earlier editions, they result in a
(allowed by default) lint, and are treated as regular identifier
tokens.
*/
struct UnknownPrefix {};

/*
An unknown prefix in a lifetime, like `'foo#`.

Like `UnknownPrefix`, only the `'` and prefix are included in the token
and not the separator.
*/
struct UnknownPrefixLifetime {};

/*
A raw lifetime, e.g. `'r#foo`. In edition < 2021 it will be split into
several tokens: `'r` and `#` and `foo`.
*/
struct RawLifetime {};

/*
Guarded string literal prefix: `#"` or `##`.

Used for reserving "guarded strings" (RFC 3598) in edition 2024.
Split into the component tokens on older editions.
*/
struct GuardedStrPrefix {};

struct Semi {};         // ;
struct Comma {};        // ,
struct Dot {};          // .
struct OpenParen {};    // (
struct CloseParen {};   // )
struct OpenBrace {};    // {
struct CloseBrace {};   // }
struct OpenBracket {};  // [
struct CloseBracket {}; // ]
struct At {};           // @
struct Pound {};        // #
struct Tilde {};        // ~
struct Question {};     // ?
struct Colon {};        // :
struct Dollar {};       // $
struct Eq {};           // =
struct Bang {};         // !
struct Lt {};           // <
struct Gt {};           // >
struct Minus {};        // -
struct And {};          // &
struct Or {};           // |
struct Plus {};         // +
struct Star {};         // *
struct Slash {};        // /
struct Caret {};        // ^
struct Percent {};      // %
struct Unknown {};      // unknown token (e.g. "№")
struct Eof {};          // end of input

// TokenKind is represented as a variant over all possible kinds.
using TokenKind = std::variant<
    LineComment,
    BlockComment,
    Whitespace,
    Ident,
    InvalidIdent,
    RawIdent,
    UnknownPrefix,
    UnknownPrefixLifetime,
    RawLifetime,
    GuardedStrPrefix,
    Literal,
    Lifetime,
    Semi,
    Comma,
    Dot,
    OpenParen,
    CloseParen,
    OpenBrace,
    CloseBrace,
    OpenBracket,
    CloseBracket,
    At,
    Pound,
    Tilde,
    Question,
    Colon,
    Dollar,
    Eq,
    Bang,
    Lt,
    Gt,
    Minus,
    And,
    Or,
    Plus,
    Star,
    Slash,
    Caret,
    Percent,
    Unknown,
    Eof
>;

/*`#"abc"#`, `##"a"` (fewer closing), or even `#"a` (unterminated).

Can capture fewer closing hashes than starting hashes,
for more efficient lexing and better backwards diagnostics.*/
struct GuardedStr {
    uint32_t n_hashes;
    bool terminated;
    uint32_t token_len;
};


// Non `#` characters exist between `r` and `"`, e.g. `r##~"abcde"##`
struct InvalidStarter {
    char bad_char;
};

/*
The string was not terminated, e.g. `r###"abcde"##`.
`possible_terminator_offset` is the number of characters after `r` or
`br` where they may have intended to terminate it.
*/
struct NoTerminator {
    uint32_t expected;
    uint32_t found;
    std::optional<uint32_t> possible_terminator_offset;
};

// More than 255 `#`s exist.
struct TooManyDelimiters {
    uint32_t found;
};

// Representing RawStrError as a variant.
using RawStrError = std::variant<
    InvalidStarter,
    NoTerminator,
    TooManyDelimiters
>;

/// `rustc` allows files to have a shebang, e.g. "#!/usr/bin/rustrun",
/// but shebang isn't a part of rust syntax.
std::optional<size_t> strip_shebang(const std::string &input) {
    // Shebang must start with "#!" literally, without any preceding whitespace.
    // For simplicity we consider any line starting with "#!" a shebang,
    // regardless of restrictions put on shebangs by specific platforms.
    if (input.rfind("#!", 0) == 0) { // check if input starts with "#!"
        std::string input_tail = input.substr(2);
        // Ok, this is a shebang but if the next non-whitespace token is '[',
        // then it may be valid Rust code, so consider it Rust code.
        std::vector<Token> tokens = tokenize(input_tail);

        auto is_skippable = [](const Token &token) -> bool {
            return std::visit([](auto &&arg) -> bool {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Whitespace>) {
                    return true;
                } else if constexpr (std::is_same_v<T, LineComment>) {
                    return !arg.doc_style.has_value();
                } else if constexpr (std::is_same_v<T, BlockComment>) {
                    return !arg.doc_style.has_value();
                } else {
                    return false;
                }
            }, token.kind);
        };

        auto it = std::find_if(tokens.begin(), tokens.end(), [&](const Token &tok) {
            return !is_skippable(tok);
        });

        bool nextIsOpenBracket = (it != tokens.end() && std::holds_alternative<OpenBracket>(it->kind));
        if (!nextIsOpenBracket) {
            size_t firstLineEnd = input_tail.find('\n');
            if (firstLineEnd == std::string::npos) {
                firstLineEnd = input_tail.size();
            }
            return 2 + firstLineEnd;
        }
    }
    return std::nullopt;
}

/*
Validates a raw string literal. Used for getting more information about a
problem with a `RawStr`/`RawByteStr` with a `None` field.
*/
inline Result<void, RawStrError> validate_raw_string(std::string &input, uint32_t prefix_len) {
    assert(!input.empty());
    Cursor cursor(input);

    // Move past the leading `r` or `br`.
    for(uint32_t i = 0; i < prefix_len; ++i) {
        std::optional<char> bump = cursor.bump();
        if(!bump.has_value()) {
            return Result<void, RawStrError>::Err(NoTerminator{});
        }
    }

    // Call raw_double_quoted_string and map its result to void.
    Result<uint8_t, RawStrError> res = raw_double_quoted_string(cursor, prefix_len);
    if (res.is_err()) {
        return Result<void, RawStrError>::Err(res); // Propagate the error.
    } else {
        return Result<void, RawStrError>::Ok(std::nullopt);
    }
}


std::vector<Token> tokenize(std::string &input);

// From here on out, we just extend the functionality of Cursor class
// Eats the double-quoted string and returns `n_hashes` and an error if encountered.
Result<uint8_t, RawStrError> raw_double_quoted_string(Cursor &cursor, uint32_t prefix_len) {
    // Wrap the actual function to handle the error with too many hashes.
    // This way, it eats the whole raw string.

    uint32_t n_hashes = raw_string_unvalidated(cursor, prefix_len);

    //only upto 255 `#`s are allowed in a string
    if(n_hashes > 255) {
        Result<uint8_t, RawStrError>::Ok(uint8_t(n_hashes));
    } else {
        Result<uint8_t, RawStrError>::Err(TooManyDelimiters{found: n_hashes});
    }
}

Result<uint32_t, RawStrError> raw_string_unvalidated(Cursor &cursor, uint32_t prefix_len);

