/*
Peekable iterator over a char sequence. 

Next characters can be peeked via the peek_first method,
and position can be shifted forward by the bump method
*/
#pragma once
#include<stddef.h>
#include<string>
#include<string_view>
#include<optional>

class Cursor {

public:
    static constexpr char EOF_CHAR = '\0';

    Cursor(const std::string& input)
            : input_(input), pos_(0), len_remaining_(input.length())
        #ifdef _DEBUG
            , prev_(EOF_CHAR)
        #endif
        {}

    
    std::string_view as_str() const {
        return std::string_view(input_).substr(pos_);
    }

    
    /*
    Returns the last eaten symbol while in Debug mode,
    else returns the last eaten character
    */
    char prev() const {
        #ifdef _DEBUG
            return prev_;
        #else
            return EOF_CHAR;
        #endif
    }


    /*
    Peeks the next character from the input stream without consuming it.
    If the requested position doesn't exist, EOF_CHAR is returned.
    However, getting `EOF_CHAR` doesn't always mean actual end of file, it should be checked with `is_eof` method.
    */
    char first_peek() const {
        return pos_ < input_.length() ? input_[pos_] : EOF_CHAR;
    }

    char second_peek() const {
        return pos_ + 1 < input_.length() ? input_[pos_ + 1] : EOF_CHAR;
    }

    char third_peek() const {
        return pos_ + 2 < input_.length() ? input_[pos_ + 2] : EOF_CHAR;
    }

    
    /* 
    Checks if there is nothing more to consume.
    */
    bool is_eof() const {
        return pos_ >= input_.length();
    }


    /*
    Returns amount of already consumed symbols
    */
   unsigned int pos_within_token() const {
        return static_cast<unsigned int> (input_.length() - len_remaining_);
    }

    
    /*
    Resets the number of bytes consumed to 0
    */
    void reset_pos_within_token() {
        len_remaining_ = input_.length() - pos_;
    }


    /*
    Moves to next char
    */
    std::optional<char> bump() {
        if (pos_ < input_.length()) {
            char c = input_[pos_++];
            
            #ifdef _DEBUG
                prev_ = c;
            #endif

            --len_remaining_;
            return c;
        }

        return EOF_CHAR;
    }


    /*
    Eats symbols while predicate returns true or until the end of file is reached.
    */
    template <typename Predicate>
    void eat_while(Predicate predicate) {
        while (!is_eof() && predicate(first_peek())) {
            bump();
        }
    }


private:
    size_t len_remaining_;
    size_t pos_;
    const std::string& input_;
    #ifdef _DEBUG
        char prev_;
    #endif
};
