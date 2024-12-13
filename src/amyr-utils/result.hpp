#pragma once

#include<variant>
#include<optional>
#include<stdexcept>
#include<functional>
#include<type_traits>
#include<functional>
#include<string>

/*
This struct imitates Rust's Result enum.
Since the compiler is based a lot on rustc, this will come in handy.
Using just variant normally may cause a lot of overhead
Making this and using inline reduces the overhead making the entire thing speedy.
*/
template <typename T, typename E>
class Result {

public:

    //Empty initializer
    Result() : value_(std::monostate{}) {}

    //Constructors for Ok() and Err()
    static Result<T, E> Ok(T value) {
        return Result(std::move(value));
    }

    static Result<T, E> Err(E error) {
        return Result(std::move(error));
    }

    
    //Check if Result is Ok
    inline bool is_ok() const {
        return std::holds_alternative<T>(value_);
    }

    
    //Check if Result is Ok
    inline bool is_err() const {
        return std::holds_alternative<E>(value_);
    }

    //check if Reuslt is empty
    inline bool is_empty() const {
        return std::holds_alternative<std::monostate>(value_);
    }


    //Access the value with safety checks
    inline T unwrap() const {
        if(!is_ok) {
            throw std::runtime_error("Called unwrap on an Err value");
        }

        return std::get<T>(value_);
    }

    //Access the error with safety checks
    inline E unwrap_err() const {
        if(!is_err()) {
            throw std::runtime_error("Called unwrap_err on an Ok value");
        }

        return std::get<E>(value_);
    }
    

    //Access the value or provide a default value
    inline T unwrap_or(const T& default_value) const {
        return is_ok() ? std::get<T>(value_) : default_value;
    }


    //Access the value or provide a default value
    inline std::variant<T, std::string&> unwrap_or(const std::string& str) const {
        return is_ok() ? std::get<T>(value_) : str;
    }


    // Transform the Result using a function
    template <typename Func>
    auto map(Func func) const -> Result<std::invoke_result_t<Func, T>, E> {
        using U = std::invoke_result_t<Func, T>;
        if (is_ok()) {
            return Result<U, E>::Ok(func(std::get<T>(value_)));
        } else {
            return Result<U, E>::Err(std::get<E>(value_));
        }
    }


    // Chain operations with a function that returns a Result
    template <typename Func>
    auto and_then(Func func) const -> std::invoke_result_t<Func, T> {
        static_assert(std::is_same_v<std::invoke_result_t<Func, T>::value_type, T>, 
                      "and_then must return a Result type.");
        if (is_ok()) {
            return func(std::get<T>(value_));
        } else {
            return std::invoke_result_t<Func, T>::Err(std::get<E>(value_));
        }
    }


    //Equality operation overload
    bool operator==(const Result<T, E>& other) {
        return value_ == other.value_;
    }

private:

    //Private constructor to enforce uses of Ok and Err
    explicit Result(T value): value_(std::move(value)) {}
    explicit Result(E error): value_(std::move(error)) {}

    std::variant<T,E> value_;

};