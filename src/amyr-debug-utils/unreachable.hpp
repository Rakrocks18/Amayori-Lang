#pragma once


#include <cassert>
#include <stdexcept>
#include <source_location>
#include <string>

// Namespace to encapsulate our unreachable implementations
namespace debug_utils {
    // Base unreachable function - minimal implementation
    [[noreturn]]
    inline void unreachable(const char* message = "Reached supposedly unreachable code") {
        // Use standard assert to trigger in debug mode
        #ifdef _DEBUG
        assert(false && message);
        #endif

        // Throw an exception if assert is disabled or in release mode
        throw std::logic_error(message);
    }

    // More advanced version with source location (C++20)
    #if __cplusplus >= 202002L
    [[noreturn]]
    inline void unreachable_detailed(
        const char* message = "Reached supposedly unreachable code", 
        const std::source_location& location = std::source_location::current()) 
    {
        // Construct a detailed error message
        std::string detailed_message = std::string(message) + 
            "\nFile: " + location.file_name() +
            "\nLine: " + std::to_string(location.line()) +
            "\nFunction: " + location.function_name();

        // Use standard assert to trigger in debug mode
        #ifdef _DEBUG
        assert(false && detailed_message.c_str());
        #endif

        // Throw an exception with detailed location info
        throw std::logic_error(detailed_message);
    }
    #endif

    // Macro-like function template for different types of unreachability
    template<typename... Args>
    [[noreturn]]
    inline void unreachable_with_context(const char* format, Args&&... args) {
        // Format message with provided context
        char buffer[1024];
        int written = snprintf(
            buffer, 
            sizeof(buffer), 
            format, 
            std::forward<Args>(args)...
        );

        // Ensure buffer is null-terminated and not truncated
        if (written < 0 || written >= static_cast<int>(sizeof(buffer))) {
            strcpy(buffer, "Unreachable code reached with formatting error");
        }

        // Throw with formatted message
        throw std::logic_error(buffer);
    }
}

// Macro-like function to mimic Rust's unreachable!() behavior
#define UNREACHABLE(...) \
    do { \
        ::debug_utils::unreachable(__VA_ARGS__); \
        __builtin_unreachable(); /* Hint to compiler that this path is impossible */ \
    } while(0)

// C++20 detailed version
#if __cplusplus >= 202002L
    #define UNREACHABLE_DETAILED(...) \
        do { \
            ::debug_utils::unreachable_detailed(__VA_ARGS__); \
            __builtin_unreachable(); \
        } while(0)
#endif

