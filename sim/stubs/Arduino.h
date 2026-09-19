#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

extern std::uint32_t gSimMillis;

inline std::uint32_t millis() { return gSimMillis; }
inline void delay(std::uint32_t milliseconds) { gSimMillis += milliseconds; }

template <typename T>
constexpr T constrain(T value, T low, T high)
{
    return value < low ? low : value > high ? high : value;
}

class String {
public:
    String() = default;
    String(const char* value) : value_(value ? value : "") {}
    String(const std::string& value) : value_(value) {}

    template <typename T,
              typename = std::enable_if_t<std::is_integral_v<T>>>
    String(T value) : value_(std::to_string(value)) {}

    std::size_t length() const { return value_.size(); }
    const char* c_str() const { return value_.c_str(); }
    operator const char*() const { return value_.c_str(); }

    String substring(std::size_t start) const
    {
        return start < value_.size() ? value_.substr(start) : std::string();
    }

    String substring(std::size_t start, std::size_t end) const
    {
        if (start >= value_.size() || end <= start) return {};
        return value_.substr(start, end - start);
    }

    int indexOf(const String& needle, std::size_t start = 0) const
    {
        const auto index = value_.find(needle.value_, start);
        return index == std::string::npos ? -1 : static_cast<int>(index);
    }

    int indexOf(char needle, std::size_t start = 0) const
    {
        const auto index = value_.find(needle, start);
        return index == std::string::npos ? -1 : static_cast<int>(index);
    }

    void remove(std::size_t start)
    {
        if (start < value_.size()) value_.erase(start);
    }

    String& operator+=(char value)
    {
        value_ += value;
        return *this;
    }

    String& operator+=(const String& value)
    {
        value_ += value.value_;
        return *this;
    }

    friend String operator+(const String& left, const String& right)
    {
        return left.value_ + right.value_;
    }

    template <typename T,
              typename = std::enable_if_t<std::is_integral_v<T>>>
    friend String operator+(const String& left, T right)
    {
        return left.value_ + std::to_string(right);
    }

    friend bool operator==(const String& left, const String& right)
    {
        return left.value_ == right.value_;
    }

    friend bool operator==(const String& left, const char* right)
    {
        return left.value_ == (right ? right : "");
    }

    friend bool operator==(const char* left, const String& right)
    {
        return right == left;
    }

    friend bool operator!=(const String& left, const String& right)
    {
        return !(left == right);
    }

private:
    std::string value_;
};

class IPAddress {
public:
    IPAddress() = default;
    IPAddress(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d)
        : bytes_{a, b, c, d} {}

    String toString() const
    {
        return std::to_string(bytes_[0]) + "." + std::to_string(bytes_[1]) +
               "." + std::to_string(bytes_[2]) + "." +
               std::to_string(bytes_[3]);
    }

    friend bool operator==(const IPAddress& left, const IPAddress& right)
    {
        return std::equal(std::begin(left.bytes_), std::end(left.bytes_),
                          std::begin(right.bytes_));
    }

    friend bool operator!=(const IPAddress& left, const IPAddress& right)
    {
        return !(left == right);
    }

private:
    std::uint8_t bytes_[4]{};
};

struct SimSerial {
    void begin(unsigned long) {}

    template <typename... Args>
    void printf(const char*, Args...) {}

    void println(const char*) {}
};

struct SimEsp {
    unsigned getFreeHeap() const { return 512000; }
};

inline SimSerial Serial;
inline SimEsp ESP;
