#pragma once
#include <optional>
#include <string>


namespace bittorrent::tracker {

    struct Error {
    std::string message;
    };


    template <typename T>
    struct Expected 
    {
        std::optional<T> value;
        std::optional<Error> error;


        static Expected success(T v) { 
            Expected e; e.value = std::move(v); 
            return e; 
        }
        static Expected failure(std::string msg) { 
            Expected e; e.error = Error{std::move(msg)}; 
            return e;
        }
        bool has_value() const { return value.has_value(); }
        T& get() { return *value; }
        const T& get() const { return *value; }
    };


    template <>
    struct Expected<void> 
    {
        std::optional<Error> error;
        static Expected success() { return {}; }
        static Expected failure(std::string msg) { Expected e; e.error = Error{std::move(msg)}; return e; }
        bool has_value() const { return !error.has_value(); }
    };


} // namespace bittorrent::tracker