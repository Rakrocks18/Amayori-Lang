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
#include<cctype>


#include "cursor.hpp"
#include "unicode_escape.hpp"
#include "amyr-utils/iterator.hpp"
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/stringpiece.h>

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
    if (input.size() < 2 || input[0] != '#' || input[1] != '!') {
        return std::nullopt;
    }
    
    std::string input_tail = input.substr(2);
    // Ok, this is a shebang but if the next non-whitespace token is '[',
    // then it may be valid Rust code, so consider it Rust code.
    auto iter = tokenize(input_tail);
    std::optional<Token> first_significant;

    // Find first non-whitespace/non-trivial-comment token
    while (auto token = iter.next()) {
        const auto& kind = token->kind;
        
        bool is_ignorable = false;
        if (std::holds_alternative<Whitespace>(kind)) {
            is_ignorable = true;
        }
        else if (const auto* lc = std::get_if<LineComment>(&kind)) {
            is_ignorable = (lc->doc_style == std::nullopt);
        }
        else if (const auto* bc = std::get_if<BlockComment>(&kind)) {
            is_ignorable = (bc->doc_style == std::nullopt);
        }

        if (!is_ignorable) {
            first_significant = token;
            break;
        }
    }

    // Check if first significant token is OpenBracket
    if (first_significant && 
        !std::holds_alternative<OpenBracket>(first_significant->kind)) {
        // Calculate shebang length including newline
        const size_t newline_pos = input_tail.find('\n');
        const size_t line_length = (newline_pos != std::string::npos)
                                  ? newline_pos
                                  : input_tail.length();
        return 2 + line_length; // Include the original "#!" prefix
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
        RawStrError res1 = res.unwrap_err();
        return Result<void, RawStrError>::Err(res1); // Propagate the error.
    } 
    
    return Result<void, RawStrError>::Ok(std::nullopt);
    
}

// Creates an iterator that produces tokens from the input string.
class TokenIterator: Iterator<TokenIterator, Token>{
    Cursor cursor;

public:
    explicit TokenIterator(std::string &input) 
        : cursor(input) {}

    std::optional<Token> next() {
        Token token = advance_token(cursor);
        
        if (std::holds_alternative<Eof>(token.kind)) {
            return std::nullopt;
        } 
        return token;
    }
}; 

TokenIterator tokenize(std::string &input) {
    return TokenIterator(input);
}

/*
True if `c` is considered a whitespace according to Rust language definition.
See [Rust language reference](https://doc.rust-lang.org/reference/whitespace.html)
for definitions of these classes.
*/
bool is_whitespace(char c) {
    // This is Pattern_White_Space.
    //
    // Note that this set is stable (ie, it doesn't change with different
    // Unicode versions), so it's ok to just hard-code the values.

    switch (c) {
        case U'\u0009':  // \t (Horizontal tab)
        case U'\u000A':  // \n (Line feed)
        case U'\u000B':  // \v (Vertical tab)
        case U'\u000C':  // \f (Form feed)
        case U'\u000D':  // \r (Carriage return)
        case U'\u0020':  // Space
        case U'\u0085':  // Next line (NEL)
        case U'\u200E':  // Left-to-Right Mark
        case U'\u200F':  // Right-to-Left Mark
        case U'\u2028':  // Line Separator
        case U'\u2029':  // Paragraph Separator
            return true;
        default:
            return false;
    }
}

// True if `c` is valid as a first character of an identifier.
bool is_id_start(char32_t c) {
    // Check for underscore or Unicode XID_Start property
    return c == U'_' || u_isIDStart(static_cast<UChar32>(c));
}

// True if `c` is valid as a non-first character of an identifier.
bool is_id_continue(char32_t c) {
    // Check for Unicode XID_Continue property
    return u_isIDPart(static_cast<UChar32>(c));
}

struct CharIterator: Iterator<CharIterator, char> {
    std::string &input;
    size_t len;
    size_t current;

public:
    explicit CharIterator(std::string &input) 
        : input(input) {
            len = input.length();
            current=0;
        }
    
    std::optional<char> next() {
        if (current < len) {
            return input[current++];
        } else {
            return std::nullopt;
        }
    }
};

bool is_ident(std::string &str) {
    CharIterator chars = CharIterator(str);

    if (auto start = chars.next()) {
        if (start.has_value()) {
            return is_id_start(start.value()) && chars.all(is_id_continue);
        } else {
            return false;
        }
    }

}

// From here on out, we just extend the functionality of Cursor class
// Eats the double-quoted string and returns `n_hashes` and an error if encountered.
Result<uint8_t, RawStrError> raw_double_quoted_string(Cursor &cursor, uint32_t prefix_len) {
    // Wrap the actual function to handle the error with too many hashes.
    // This way, it eats the whole raw string.

    Result<uint32_t, RawStrError> n_hashes = raw_string_unvalidated(cursor, prefix_len);
    uint32_t n_hashes1 = n_hashes.unwrap();
    //only upto 255 `#`s are allowed in a string
    if(n_hashes1 > 255) {
        Result<uint8_t, RawStrError>::Ok(uint8_t(n_hashes1));
    } else {
        Result<uint8_t, RawStrError>::Err(TooManyDelimiters{found: n_hashes1});
    }
}

Token advance_token(Cursor &cursor) {
    size_t start_pos = cursor.pos();

    auto bump = cursor.bump();
    if (!bump.has_value()) {
        return Token(TokenKind(Eof{}), 0);
    }

    char32_t first_char = bump.value();

    TokenKind kind;

    switch(first_char) {
        //Slash, line comment or block comment
        case '/': {
            char next_char = cursor.first();
            if (next_char == '/') {
                kind = line_comment(cursor);
            } else if (next_char == '*') {
                kind = block_comment(cursor);
            } else {
                kind = TokenKind(Slash{});
            }
            break;
        }

        //Whitespace
        case U'\u0009':  // \t (Horizontal tab)
        case U'\u000A':  // \n (Line feed)
        case U'\u000B':  // \v (Vertical tab)
        case U'\u000C':  // \f (Form feed)
        case U'\u000D':  // \r (Carriage return)
        case U'\u0020':  // Space
        case U'\u0085':  // Next line (NEL)
        case U'\u200E':  // Left-to-Right Mark
        case U'\u200F':  // Right-to-Left Mark
        case U'\u2028':  // Line Separator
        case U'\u2029': { // Paragraph Separator
        
            kind = whitespace(cursor);
            break;
        }

        case 'r': {
            char next1 = cursor.first();
            char next2 = cursor.second();
            
            if (next1 == '#' && is_id_start(next2)) {
                kind = raw_ident(cursor);
            } else if (next1 == '#' || next1 == '"') {
                auto res = raw_double_quoted_string(cursor, 1);
                const uint32_t suffix_start = cursor.pos_within_token();

                if(res.is_ok()) {
                    eat_literal_suffix(cursor);
                }
                kind = Literal{namespace_Literal::RawStr {res.unwrap()}, suffix_start};
            } else {
                kind = ident_or_unknown_prefix(cursor);
            }
            break;
        }

        case 'b': {
            kind = c_or_byte_string(
                cursor,
                /*byte_str_handler*/ [] (bool t) {return namespace_Literal::ByteStr{t};},
                /*raw_byte_str_handler*/ [] (auto h) {return namespace_Literal::RawByteStr{h};},
                /*byte_handler*/ [] (bool t) {return namespace_Literal::Byte{t};}
            );
            break;
        }

        case 'c': {
            kind = c_or_byte_string(
                cursor,
                /*c_str_handler*/ [] (bool t) {return namespace_Literal::CStr{t};},
                /*raw_c_str_handler*/ [] (auto h) {return namespace_Literal::RawCStr{h};},
                /*no_char_handler*/ nullptr
            );
            break;
        }

        case '0'...'9': {
            auto lit_kind = number(cursor, first_char);
            const uint32_t suffix_start = cursor.pos_within_token();
            eat_literal_suffix(cursor);
            kind = Literal{lit_kind, suffix_start};
            break;
        }

        case ';': kind = Semi{}; break;
        case ',': kind = Comma{}; break;
        case '.': kind = Dot{}; break;
        case '(': kind = OpenParen{}; break;
        case ')': kind = CloseParen{}; break;
        case '{': kind = OpenBrace{}; break;
        case '}': kind = CloseBrace{}; break;
        case '[': kind = OpenBracket{}; break;
        case ']': kind = CloseBracket{}; break;
        case '@': kind = At{}; break;
        case '#': kind = Pound{}; break;
        case '~': kind = Tilde{}; break;
        case '?': kind = Question{}; break;
        case ':': kind = Colon{}; break;
        case '$': kind = Dollar{}; break;
        case '=': kind = Eq{}; break;
        case '!': kind = Bang{}; break;
        case '<': kind = Lt{}; break;
        case '>': kind = Gt{}; break;
        case '-': kind = Minus{}; break;
        case '&': kind = And{}; break;
        case '|': kind = Or{}; break;
        case '+': kind = Plus{}; break;
        case '*': kind = Star{}; break;
        case '^': kind = Caret{}; break;
        case '%': kind = Percent{}; break;

        // Lifetime or character literal
        case '\'': {
            kind = lifetime_or_char(cursor);
            break;
        }

        // String literal
        case '"': {
            const bool terminated = double_quoted_string(cursor);
            const uint32_t suffix_start = cursor.pos_within_token();
            if (terminated) {
                eat_literal_suffix(cursor);
            }
            kind = Literal{namespace_Literal::Str{terminated}, suffix_start};
            break;
        }

        default: {
            if (is_id_start(first_char)) {
                kind = ident_or_unknown_prefix(cursor);
            } else if (!is_ascii(first_char) && isemoji(first_char)) {
                kind = invalid_ident(cursor);
            } else {
                kind = Unknown{};
            }
            break;
        }
    }

    Token res = Token(kind, cursor.pos_within_token());
    cursor.reset_pos_within_token();
    return res; 
}

bool is_ascii(char32_t c) {
    return c >= 0 && c <= 127;
}

bool is_ascii_digit(char32_t c) {
    return c >= '0' && c <= '9';
}

bool isemoji(char32_t c) {
    return (c >= 0x1F600 && c <= 0x1F64F);
}

TokenKind line_comment(Cursor &cursor) {
    // Consume the line comment
    assert(cursor.prev() == '/' && cursor.first() == '/');
    cursor.bump();

    std::optional<DocStyle> doc_style = std::nullopt;
    const char32_t next = cursor.first();
    if (next == '!') {
        // `//!` is an inner line doc comment.
        doc_style = DocStyle::Inner;
    } else if (next == '/' && cursor.second() != '/') {
        // `////` (more than 3 slashes) is not considered a doc comment.
        doc_style = DocStyle::Outer;
    }

    cursor.eat_until('\n');

    return TokenKind(LineComment{doc_style});
}

TokenKind block_comment(Cursor &cursor) {
    // Consume the block comment
    assert(cursor.prev() == '/' && cursor.first() == '*');
    cursor.bump(); //Consume the '*'

    //Determine the doc style
    std::optional<DocStyle> doc_style;
    const char32_t next = cursor.first();
    if (next == '!') {
        doc_style = DocStyle::Inner;
    } else if (next == '*' && cursor.second() != '*' && cursor.second() != '/') {
        doc_style = DocStyle::Outer;
    }

    //Handle nested block comments
    size_t depth = 1;
    while (auto c = cursor.bump()) {
        if (c == '/' && cursor.first() == '*') {
            cursor.bump();
            depth++;
        } else if (c == '*' && cursor.first() == '/') {
            cursor.bump(); // Consume the '/'
            if (--depth == 0) {
                return BlockComment{doc_style, true};
            }
        }
    }
    return BlockComment{doc_style, false};
}

TokenKind whitespace(Cursor &cursor) {
    // Consume the whitespace
    assert(is_whitespace(cursor.prev()));
    cursor.eat_while(is_whitespace);

    return TokenKind(Whitespace{});
}

TokenKind raw_ident(Cursor &cursor) {
    assert(cursor.prev() == 'r' && cursor.first() == '#' && is_id_start(cursor.second()));
    //Eat the `#` symbol
    cursor.bump();
    //Eat the identifier as part of RawIdent
    eat_identifier(cursor);
    return TokenKind(RawIdent{});
}

TokenKind ident_or_unknown_prefix(Cursor &cursor) {
    assert(is_id_start(cursor.prev()));
    //Start already eaten, eat the rest of the identifier
    cursor.eat_while(is_id_continue);
    // Known prefixes must have been handled earlier. So if
    // we see a prefix here, it is definitely an unknown prefix.
    const char32_t c = cursor.first();
    if (c == '#' || c == '\'' || c == '"') {
        return TokenKind(UnknownPrefix{});
    } else if (!is_ascii(c) && isemoji(c)) {
        return invalid_ident(cursor);
    }
    return TokenKind(Ident{});
}

TokenKind invalid_ident(Cursor &cursor) {
    // Start is already eaten, eat the rest of the identifier
    cursor.eat_while([] (char32_t c) {
        const char ZERO_WIDTH_JOINER = U'\u200D';
        return is_id_continue(c) || c == ZERO_WIDTH_JOINER || !is_ascii(c) && isemoji(c);
    });
    // An invalid identifier followed by '#' or '"' or '\'' could be
    // interpreted as an invalid literal prefix. We don't bother doing that
    // because the treatment of invalid identifiers and invalid prefixes
    // would be the same.
    return TokenKind(InvalidIdent{});
}

TokenKind c_or_byte_string(
    Cursor &cursor,
    std::function<LiteralKind(bool)> mk_kind,
    std::function<LiteralKind(std::optional<unsigned short>)> mk_kind_raw,
    std::optional<std::function<LiteralKind(bool)>> single_quoted 
) {
    char32_t next1 = cursor.first();
    char32_t next2 = cursor.second();

    if (next1 == '\'' && single_quoted.has_value()) {
        cursor.bump();
        bool terminated = single_quoted_string(cursor);
        uint32_t suffix_start = cursor.pos_within_token();
        if (terminated) {
            eat_literal_suffix(cursor);
        }
        LiteralKind kind = single_quoted.value()(terminated);
        return TokenKind(Literal{kind, suffix_start});
    } else if (next1 == '"') {
        cursor.bump();
        bool terminated = double_quoted_string(cursor);
        uint32_t suffix_start = cursor.pos_within_token();

        if (terminated) {
            eat_literal_suffix(cursor);
        }

        LiteralKind kind = mk_kind(terminated);
        return TokenKind(Literal{kind, suffix_start});
    } else if (next1=='r' && (next2=='"' || next2=='#')) {
        cursor.bump();
        Result<uint8_t, RawStrError> res = raw_double_quoted_string(cursor, 2);
        uint32_t suffix_start = cursor.pos_within_token();
        if (res.is_ok()) {
            eat_literal_suffix(cursor);
        }
        LiteralKind kind = mk_kind_raw(res.ok());
        return TokenKind(Literal{kind, suffix_start});
    } else {
        ident_or_unknown_prefix(cursor);
    }
}

LiteralKind number(Cursor &cursor, char32_t first_char) {
    // Consume the number
    assert(first_char >= '0' && first_char <= '9');
    
    Base base = Base::Decimal;
    if (first_char == '0') {
        char32_t next = cursor.first();
        // Attempt to parse encoding base.
        switch (next) {
            case 'b': {
                base = Base::Binary;
                cursor.bump();
                if (!eat_decimal_digits(cursor)) {
                    return namespace_Literal::Int{base, true};
                }
                break;
            }
            
            case 'o': {
                base = Base::Octal;
                cursor.bump();
                if (!eat_decimal_digits(cursor)) {
                    return namespace_Literal::Int{base, true};
                }
                break;
            }
            case 'x': {
                base = Base::Hexadecimal;
                cursor.bump();
                if (!eat_hexadecimal_digits(cursor)) {
                    return namespace_Literal::Int{base, true};
                }
                break;
            }

            // Not a base prefix; consume additional digits.
             
            case '_': 
            case '0'...'9':{
                eat_decimal_digits(cursor);
                break;
            }

            case '.':
            case 'e':
            case 'E': {
                // Also not a base prefix; nothing more to do here.
                break;
            }



            default:
                //Just a 0
                return namespace_Literal::Int{base, false};
                break;
        }
    } else {
         // No base prefix, parse number in the usual way.
        eat_decimal_digits(cursor);
    }

    char32_t next = cursor.first();
    // Don't be greedy if this is actually an
    // integer literal followed by field/method access or a range pattern
    // (`0..2` and `12.foo()`)
    if(next == '.' && cursor.second() != '.' && !is_id_start(cursor.second())) {
        // might have stuff after the ., and if it does, it needs to start
        // with a number
        cursor.bump();
        bool empty_exponent = false;

        if (is_ascii_digit(cursor.first())) {
            eat_decimal_digits(cursor);
            switch (cursor.first()) {
                case 'e':
                case 'E': {
                    cursor.bump();
                    empty_exponent = !eat_float_exponent(cursor);
                    break;
                }
                default:
                    break;
            }
        }
        return namespace_Literal::Float{base, empty_exponent};
    } else if (next == 'e' || next == 'E') {
        cursor.bump();
        bool empty_exponent = !eat_float_exponent(cursor);
        return namespace_Literal::Float{base, empty_exponent};
    } else {
        // No exponent, just an integer literal.
        return namespace_Literal::Int{base, false};
    }
}

TokenKind lifetime_or_char(Cursor &cursor) {
    assert(cursor.prev() == '\'');

    bool can_be_lifetime;
    if (cursor.second() == '\'') {
        // It's surely not a lifetime.
        can_be_lifetime = false;
    } else {
        // If the first symbol is valid for identifier, it can be a lifetime.
        // Also check if it's a number for a better error reporting (so '0 will
        // be reported as invalid lifetime and not as unterminated char literal).
        can_be_lifetime = is_id_start(cursor.first()) || is_ascii_digit(cursor.first());
    }

    if (!can_be_lifetime) {
        bool terminated = single_quoted_string(cursor);
        uint32_t suffix_start = cursor.pos_within_token();
        if (terminated) {
            eat_literal_suffix(cursor);
        }
        LiteralKind kind = namespace_Literal::Char{terminated};
        return TokenKind(Literal{kind, suffix_start});
    }

    if (cursor.first() == 'r' && cursor.second() == '#' && is_id_start(cursor.third())) {
        // Eat "r" and `#`, and identifier start characters.
        cursor.bump();
        cursor.bump();
        cursor.bump();
        cursor.eat_while(is_id_continue);
        return RawLifetime{};
    }

    // Either a lifetime or a character literal with
    // length greater than 1.
    bool starts_with_number = is_ascii_digit(cursor.first());

    // Skip the literal contents.
    // First symbol can be a number (which isn't a valid identifier start),
    // so skip it without any checks.

    cursor.bump();
    cursor.eat_while(is_id_continue);

    char32_t c = cursor.first();
    if (c == '\'') {
        // Check if after skipping literal contents we've met a closing
        // single quote (which means that user attempted to create a
        // string with single quotes).
        cursor.bump();
        LiteralKind kind = namespace_Literal::Char{true};
        return TokenKind(Literal{kind, cursor.pos_within_token()});           
    } else if (c == '#' && !starts_with_number) {
        return UnknownPrefixLifetime{};
    } else {
        return Lifetime {starts_with_number};
    }
}

bool single_quoted_string(Cursor &cursor) {
    assert(cursor.prev() == '\'');

    //Check if it's a one-symbol literal
    if (cursor.second() == '\'' && cursor.first() != '\\') {
        cursor.bump();
        cursor.bump();
        return true;
    }

    // Literal has more than one symbol.

    // Parse until either quotes are terminated or error is detected.
    while(true) {
        char c = cursor.first();
        if (c == '\''){
            // Quotes are terminated, finish parsing.
            cursor.bump();
            return true;
        }
        
        // Probably beginning of the comment, which we don't want to include
        // to the error report.
        else if (c == '/') { break; }
        
        // Newline without following '\'' means unclosed quote, stop parsing.
        else if(c == '\n' && cursor.second() != '\'') { break; }
        
        // End of file, stop parsing.
        else if (c == EOF_CHAR && cursor.is_eof()) { break; }

        // Escaped slash is considered one character, so bump twice.
        else if (c == '\\') {
            cursor.bump();
            cursor.bump();
        }
        
        // Skip the character.
        else {
            cursor.bump();
        }
    }
    //String was not terminated
    return false;
}

// Eats double-quoted string and returns true
// if string is terminated.
bool double_quoted_string(Cursor &cursor) {
    assert(cursor.prev() == '"');

    while (true) {
        auto c_opt = cursor.bump();
        if (!c_opt.has_value()) {
            // Reached EOF before closing quote
            return false;
        }
        
        const char c = c_opt.value();
        
        if (c == '"') {
            // Found closing quote
            return true;
        }
        
        if (c == '\\') {
            // Handle escape sequences
            const char next = cursor.first();
            if (next == '\\' || next == '"') {
                // Skip escaped character
                cursor.bump();
            }
        }
    }
}

// Attempt to lex for a guarded string literal.
//
// Used by `rustc_parse::lexer` to lex for guarded strings
// conditionally based on edition.
//
// Note: this will not reset the `Cursor` when a
// guarded string is not found. It is the caller's
// responsibility to do so.
std::optional<GuardedStr> guarded_qouted_double_string(Cursor &cursor) {
    assert(cursor.prev() != '#');
    
    uint32_t n_start_hashes = 0;
    while (cursor.first() == '#') {
        cursor.bump();
        n_start_hashes++;
    }

    if (cursor.first() != '"') {
        // Not a guarded string literal
        return std::nullopt;
    }
    cursor.bump(); // Consume the '"'
    assert(cursor.prev() == '"');

    // Lex the string itself as a normal string literal
    // so we can recover that for future/older editions later.
    bool terminated = double_quoted_string(cursor);
    if (!terminated) {
        const uint32_t token_len = cursor.pos_within_token();
        cursor.reset_pos_within_token();

        return GuardedStr{n_start_hashes, false, token_len};
    }

    // Consume closing '#' symbols.
    // Note that this will not consume extra trailing `#` characters:
    // `###"abcde"####` is lexed as a `GuardedStr { n_end_hashes: 3, .. }`
    // followed by a `#` token.
    uint16_t n_end_hashes = 0;
    while(cursor.first() == '#' && n_end_hashes < n_start_hashes) {
        cursor.bump();
        n_end_hashes++;
    }

    // Reserved syntax, always an error, so it doesn't matter if
    // `n_start_hashes != n_end_hashes`.
    
    eat_literal_suffix(cursor);

    const uint32_t token_len = cursor.pos_within_token();
    cursor.reset_pos_within_token();

    return GuardedStr{n_start_hashes, true, token_len};    
}

// Eats the double-quoted string and returns `n_hashes` and an error if encountered.
Result<uint8_t, RawStrError> raw_double_quoted_string(Cursor &cursor, uint32_t prefix_len) {
    // Wrap the actual function to handle the error with too many hashes.
    // This way, it eats the whole raw string.
    
    Result<uint32_t, RawStrError> n_hashes = raw_string_unvalidated(cursor, prefix_len);
    //only upto 255 `#`s are allowed in a string
    if(n_hashes.is_ok() && n_hashes.unwrap() <= 255) {
        Result<uint8_t, RawStrError>::Ok(uint8_t(n_hashes.unwrap()));
    } else {
        Result<uint8_t, RawStrError>::Err(TooManyDelimiters{found: n_hashes.unwrap()});
    }
}

Result<uint32_t, RawStrError> raw_string_unvalidated(Cursor &cursor, uint32_t prefix_len) {
    assert(cursor.prev() == 'r');
    const uint32_t start_pos = cursor.pos_within_token();
    std::optional<uint32_t> possible_terminator_offset;
    uint32_t max_hashes = 0;
    
    // Count opening '#' symbols.
    uint32_t eaten = 0;
    while (cursor.first() == '#') {
        cursor.bump();
        eaten++;
    }
    const uint32_t n_start_hashes = eaten;

    // Check that string is started.
    std::optional<char> quote = cursor.bump();
    if (!quote.has_value() && quote.value() != '"') {
        const char bad_char = quote.has_value() ? quote.value() : EOF_CHAR;
        return Result<uint32_t, RawStrError>::Err(InvalidStarter{bad_char});
    }

    // Skip the string contents and on each '#' character met, check if this is
    // a raw string termination.
    while(true) {
        cursor.eat_until('"');

        if (cursor.is_eof()) {
            // Reached EOF before closing quote
            return Result<uint32_t, RawStrError>::Err(NoTerminator{n_start_hashes, max_hashes, possible_terminator_offset});
        }

        // Eat closing double quote.
        cursor.bump();

        // Check that amount of closing '#' symbols
        // is equal to the amount of opening ones.
        // Note that this will not consume extra trailing `#` characters:
        // `r###"abcde"####` is lexed as a `RawStr { n_hashes: 3 }`
        // followed by a `#` token.
        uint32_t n_end_hashes = 0;
        while (cursor.first() == '#' && n_end_hashes < n_start_hashes) {
            n_end_hashes++;
            cursor.bump();
        } 

        if (n_end_hashes == n_start_hashes) {
            // Found a valid raw string literal.
            return Result<uint32_t, RawStrError>::Ok(n_start_hashes);
        } else if (n_end_hashes > n_start_hashes) {
            // Found a closing quote with more hashes than opening ones.
            // Keep track of possible terminators to give a hint about
            // where there might be a missing terminator
            possible_terminator_offset = cursor.pos_within_token() - start_pos - n_end_hashes + prefix_len;
            max_hashes = n_end_hashes;
        }    
    }
}

bool eat_decimal_digits(Cursor &cursor) {
    bool has_digits = false;
    while(true) {
        char32_t c = cursor.first();
        if (is_ascii_digit(c)) {
            cursor.bump();
            has_digits = true;
        } else if (c == '_') {
            cursor.bump();
        } else {
            break;
        }
    }
    return has_digits;
}

bool eat_hexadecimal_digits(Cursor &cursor) {
    bool has_digits = false;
    while(true) {
        char32_t c = cursor.first();
        if (is_ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            cursor.bump();
            has_digits = true;
        } else if (c == '_') {
            cursor.bump();
        } else {
            break;
        }
    }
    return has_digits;
}

// Eats the float exponent. Returns true if at least one digit was met,
// and returns false otherwise.
bool eat_float_exponent(Cursor &cursor) {
    assert(cursor.prev() == 'e' || cursor.prev() == 'E');
    bool has_digits = false;
    char32_t c = cursor.first();
    if (c == '+' || c == '-') {
        cursor.bump();
    }
    return eat_decimal_digits(cursor);
}

// Eats the suffix of the literal, e.g. "u8".
bool eat_literal_suffix(Cursor &cursor) {
    eat_identifier(cursor);
}

// Eats the identifier. Note: succeeds on `_`, which isn't a valid
// identifier
void eat_identifier(Cursor &cursor) {
    if(!is_id_start(cursor.first())) {
        return;
    }
    cursor.bump();
    cursor.eat_while(is_id_continue);
}