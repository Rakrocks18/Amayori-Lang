// Utilities for validating string and char literals and turning them into values they represent.

#pragma once

#include<string>
#include<string_view>
#include<optional>
#include<variant>
#include<cctype>
#include<cassert>
#include<iostream>
#include<functional>
#include<stdexcept>
#include<ranges>

#include "amyr-debug-utils/unreachable.hpp"
#include "amyr-utils/result.hpp"


enum EscapeError {
    
    /*
    Errors and warnings that can occur during string unescaping. 
    They mostly relate to malformed escape sequences, but 
    there are a few that are about other problems
    */
    
    // Expected 1 char, but 0 were found.
    ZeroChars,
    // Expected 1 char, but more than 1 were found.
    MoreThanOneChar,

    // Escaped '\' character without continuation.
    LoneSlash,
    // Invalid escape character (e.g. '\z').
    InvalidEscape,
    // Raw '\r' encountered.
    BareCarriageReturn,
    // Raw '\r' encountered in raw string.
    BareCarriageReturnInRawString,
    // Unescaped character that was expected to be escaped (e.g. raw '\t').
    EscapeOnlyChar,

    // Numeric character escape is too short (e.g. '\x1').
    TooShortHexEscape,
    // Invalid character in numeric escape (e.g. '\xz')
    InvalidCharInHexEscape,
    // Character code in numeric escape is non-ascii (e.g. '\xFF').
    OutOfRangeHexEscape,

    // '\u' not followed by '{'.
    NoBraceInUnicodeEscape,
    // Non-hexadecimal value in '\u{..}'.
    InvalidCharInUnicodeEscape,
    // '\u{}'
    EmptyUnicodeEscape,
    // No closing brace in '\u{..}', e.g. '\u{12'.
    UnclosedUnicodeEscape,
    // '\u{_12}'
    LeadingUnderscoreUnicodeEscape,
    // More than 6 characters in '\u{..}', e.g. '\u{10FFFF_FF}'
    OverlongUnicodeEscape,
    // Invalid in-bound unicode character code, e.g. '\u{DFFF}'.
    LoneSurrogateUnicodeEscape,
    // Out of bounds unicode character code, e.g. '\u{FFFFFF}'.
    OutOfRangeUnicodeEscape,

    // Unicode escape code in byte literal.
    UnicodeEscapeInByte,
    // Non-ascii character in byte literal, byte string literal, or raw byte string literal.
    NonAsciiCharInByte,

    // `\0` in a C string literal.
    NulInCStr,

    // After a line ending with '\', the next line contains whitespace characters that are not skipped.
    UnskippedWhitespaceWarning,

    // After a line ending with '\', multiple lines are skipped.
    MultipleSkippedLinesWarning
};


bool is_fatal(EscapeError error) {
    /*
    Determines if an error is fatal
    */

    return error != EscapeError::UnskippedWhitespaceWarning ||
           error != EscapeError::MultipleSkippedLinesWarning;
}


/*
What kind of literal do we parse
*/
enum Mode {
    Char,
    
    Byte,
    
    Str,
    RawStr,

    ByteStr,
    RawByteStr,

    CStr,
    RawCStr
};

/*
Utils for Mode enum
*/
bool in_double_quotes(Mode mode) {
    switch (mode)
    {
        case Mode::Char:
        case Mode::Byte:
            return false;
        
    
    /*
    Since this is a enum and the `state` to speak is only a few possible things, 
    if it isn't Mode::Char or Mode::Byte, the others are true.
    An unusual optimization, but an optimization none the less
    */
        default:
            return true;
        }
}


/*
Are `\x80`, `\xff` allowed?
*/
bool allow_high_bytes(Mode mode) {

    switch (mode)
    {
        case Mode::Char:
        case Mode::Str:
            return false;
        
        case Mode::Byte:
        case Mode::ByteStr:
        case Mode::CStr:
            return true;

        case Mode::RawStr:
        case Mode::RawByteStr:
        case Mode::RawCStr:
            UNREACHABLE();
    }
}

// Are unicode (non-ASCII) chars allowed?
inline bool allow_unicode_chars(Mode mode) {
    switch (mode)
    {
    case Mode::Byte:
    case Mode::ByteStr:
    case Mode::RawByteStr:
        return false;
    
    
    // similar optimization as in_double_quotes()
    
    default:
        return true;
    }
}


// Are unicode escapes (`\u`) allowed?
bool allow_unicode_escapes(Mode mode) {
    switch (mode) {
        case Mode::Byte:
        case Mode::ByteStr:
            return false;

        case Mode::Char:
        case Mode::Str:
        case Mode::CStr:
            return true;

        default:
            UNREACHABLE(); 
    }
}


const std::string prefix_noraw(Mode mode) {
    switch (mode) {
        case Mode::Char:
        case Mode::Str:
        case Mode::RawStr:
            return "";

        case Mode::Byte:
        case Mode::ByteStr:
        case Mode::RawByteStr:
            return "b";

        case CStr:
        case RawCStr:
            return "c";
    }
}


Result<MixedUnit, EscapeError> scan_escape(std::string::iterator& it, std::string::iterator end, Mode mode, int xyz) {
    // Previous character was '\\', unescape what follows.
    char res;
    char c;
    if (it == end) {
        return Result<MixedUnit, EscapeError>::Err(LoneSlash);
    } else {
        c = *it++;
    }

    switch (c) {
        case '"' : res = '"'; break;
        case 'n' : res = '\n'; break;
        case 'r' : res = '\r'; break;
        case 't' : res = '\t'; break;
        case '\\' : res = '\\'; break;
        case '\'' : res = '\''; break;
        case '0' : res = '\0'; break;
        case 'x' : {
            //Parse Hexadeecimal character code
            if (std::distance(it, end) < 2) {
                return Result<MixedUnit, EscapeError>::Err(TooShortHexEscape);
            }

            char hi = *it++;
            char lo = *it++;
            if (!std::isxdigit(hi) || !std::isxdigit(lo)) {
                return Result<MixedUnit, EscapeError>::Err(InvalidCharInHexEscape);
            }

            int value = (std::stoi(std::string(1, hi), nullptr, 16) << 4) + std::stoi(std::string(1, lo), nullptr, 16);

            if (value > 127 && mode != Mode::Byte) {
                return Result<MixedUnit, EscapeError>::Err(OutOfRangeHexEscape);
            }

            return Result<MixedUnit, EscapeError>::Ok(MixedUnit(static_cast<char>(value)));
        }

        case 'u' : {
            return scan_unicode(it, end, allow_unicode_escapes(mode));
        }
        default: return Result<MixedUnit, EscapeError>::Err(InvalidEscape);

    }

    return Result<MixedUnit, EscapeError>::Ok(MixedUnit(res));
    
}


class MixedUnit {
/*
Used for mixed utf8 string literals, i.e. those that allow both unicode chars and high bytes.
*/
public:

    /*
    Used for ASCII chars (written directly or via `\x00`..`\x7f` escapes)
    and Unicode chars (written directly or via `\u` escapes).
    
    For example, if '¥' appears in a string it is represented here as
    `MixedUnit::Char('¥')`, and it will be appended to the relevant byte
    string as the two-byte UTF-8 sequence `[0xc2, 0xa5]`
    */
    explicit MixedUnit(char c) : value(c) {}
    
    
    /*
    Used for high bytes (`\x80`..`\xff`).
    
    For example, if `\xa5` appears in a string it is represented here as
    `MixedUnit::HighByte(0xa5)`, and it will be appended to the relevant
    byte string as the single byte `0xa5`.
    */
    explicit MixedUnit(uint8_t b) : value(b) {}

    bool is_char() const { return std::holds_alternative<char>(value); }
    bool is_high_byte() const { return std::holds_alternative<uint8_t>(value); }

    char as_char() const { return std::get<char>(value); }
    uint8_t as_high_byte() const { return std::get<uint8_t>(value); }

private:
    std::variant<char, uint8_t> value;
};


MixedUnit to_mixed_unit(char c) { return MixedUnit(c); }
MixedUnit to_mixed_unit(uint8_t b) { return MixedUnit(b); }


inline Result<char, EscapeError> ascii_check(char c, bool allow_unicode_characters) {
    if (allow_unicode_characters || static_cast<unsigned char>(c) < 128) {
        return Result<char, EscapeError>::Ok(c);
    } else {
        return Result<char, EscapeError>::Err(NonAsciiCharInByte);
    }
}

void unescape_unicode(std::string& src, Mode mode, std::function<void(std::pair<size_t, size_t>, Result<MixedUnit, EscapeError>)> &callback) {
    switch (mode)
    {
        case Mode::Char:
        case Mode::Byte:
            std::string::iterator it = src.begin();
            Result<MixedUnit, EscapeError> res = Result<MixedUnit, EscapeError>::Ok(MixedUnit(unescape_char_or_byte(it, src.end(), mode).unwrap()));
            callback({0, static_cast<size_t>(std::distance(src.begin(), it))}, res);
            break;

        case Mode::Str:
        case Mode::ByteStr:
            unescape_non_raw_common(src, mode, callback);
            break;

    }
}

Result<char, EscapeError> unescape_char_or_byte(std::string::iterator &it, const std::string::const_iterator &end, Mode mode) {
    if (it == end) {
        return Result<char, EscapeError>::Err(EscapeError::ZeroChars);
    }

    char c = *it++;
    Result<char, EscapeError> res;
    switch (c) {
        case '\\':
            if (it == end) {
                return Result<char, EscapeError>::Err(InvalidEscape);
            }

            c = *it++;
            switch (c) {
                case 'n': res = Result<char, EscapeError>::Ok('\n'); break;
                case 't': res = Result<char, EscapeError>::Ok('\t'); break;
                case 'r': res = Result<char, EscapeError>::Ok('\r'); break;
                case '\\': res = Result<char, EscapeError>::Ok('\\'); break;
                default: res = Result<char, EscapeError>::Err(InvalidEscape); break;
            }
            break;

        case '\n':
        case '\t':
        case '\'':
            res = Result<char, EscapeError>::Err(EscapeOnlyChar);
            break;

        case '\r':
            res = Result<char, EscapeError>::Err(BareCarriageReturn);
            break;

        default:
            res = ascii_check(c, allow_unicode_chars(mode));
            break;
    }

    

    if (it != end) {
        return Result<char, EscapeError>::Err(MoreThanOneChar);
    }

    return res;
    
}

/* 
Takes a contents of a string literal (without quotes) and produces a sequence of escaped characters or errors
*/
void unescape_non_raw_common(std::string& src, Mode mode, 
                                std::function<void(std::pair<size_t, size_t>, Result<MixedUnit, EscapeError>)> &callback) {
    
    std::string::iterator it = src.begin();
    std::string::iterator end = src.end();
    bool allow_unicode_chars_ = allow_unicode_chars(mode);

    while (it != end) {
        size_t  start = std::distance(src.begin(), it);
        Result<MixedUnit, EscapeError> res;

        char c = *it++;
        if(c = '\\') {
            if (it != end && *it == '\n') {
                ++it;
                skip_ascii_whitespace(it, end, start, callback);
                continue;
            } else {
                res = scan_escape(it, end, mode, 0);
            }   
        }

    }
}

void skip_ascii_whitespace(std::string::iterator& it, std::string::iterator& end, size_t start, std::function<void(std::pair<size_t, size_t>, Result<MixedUnit, EscapeError>)> &callback) {}