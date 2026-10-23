#pragma once

#include <compare>
#include <cstdint>
#include <ostream>
#include <string>

namespace JBank {

class Money {
public:
    constexpr explicit Money(int64_t cents = 0) noexcept : m_cents(cents) {}

    [[nodiscard]] static Money fromDollars(double dollars);

    [[nodiscard]] constexpr int64_t getCents() const noexcept {
        return m_cents;
    }

    [[nodiscard]] constexpr double toDollars() const noexcept {
        return static_cast<double>(m_cents) / 100.0;
    }

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] Money operator+(const Money& other) const;
    [[nodiscard]] Money operator-(const Money& other) const;
    [[nodiscard]] Money operator*(double scalar) const;
    [[nodiscard]] Money operator*(int64_t scalar) const;
    [[nodiscard]] Money operator/(int64_t scalar) const;
    [[nodiscard]] Money operator/(double divisor) const;

    Money& operator+=(const Money& other);
    Money& operator-=(const Money& other);

    auto operator<=>(const Money& other) const = default;
    bool operator==(const Money& other) const = default;

    friend Money operator*(double scalar, const Money& m) {
        return m * scalar;
    }

    friend Money operator*(int64_t scalar, const Money& m) {
        return m * scalar;
    }

    friend std::ostream& operator<<(std::ostream& os, const Money& m);

private:
    int64_t m_cents{0};
};

} // namespace JBank
