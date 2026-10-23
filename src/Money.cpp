#include "Money.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace JBank {

Money Money::fromDollars(double dollars) {
    if (std::isnan(dollars) || std::isinf(dollars)) {
        throw std::invalid_argument("Invalid dollar amount: NaN or infinity");
    }
    double scaled = std::round(dollars * 100.0);
    if (scaled >= static_cast<double>(std::numeric_limits<int64_t>::max()) ||
        scaled < static_cast<double>(std::numeric_limits<int64_t>::min())) {
        throw std::overflow_error("Dollar amount exceeds 64-bit integer range");
    }
    return Money(static_cast<int64_t>(scaled));
}

std::string Money::toString() const {
    bool negative = m_cents < 0;
    uint64_t absCents = 0;
    if (m_cents == std::numeric_limits<int64_t>::min()) {
        absCents = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL;
    } else if (negative) {
        absCents = static_cast<uint64_t>(-m_cents);
    } else {
        absCents = static_cast<uint64_t>(m_cents);
    }

    uint64_t dollars = absCents / 100;
    uint64_t cents = absCents % 100;

    std::string dollarStr = std::to_string(dollars);
    std::string formattedDollars;
    size_t len = dollarStr.length();
    for (size_t i = 0; i < len; ++i) {
        if (i > 0 && (len - i) % 3 == 0) {
            formattedDollars += ',';
        }
        formattedDollars += dollarStr[i];
    }

    std::string centStr = (cents < 10 ? "0" : "") + std::to_string(cents);

    std::string result;
    if (negative) {
        result += '-';
    }
    result += '$';
    result += formattedDollars;
    result += '.';
    result += centStr;

    return result;
}

Money Money::operator+(const Money& other) const {
    if (other.m_cents > 0 && m_cents > std::numeric_limits<int64_t>::max() - other.m_cents) {
        throw std::overflow_error("Money addition overflow");
    }
    if (other.m_cents < 0 && m_cents < std::numeric_limits<int64_t>::min() - other.m_cents) {
        throw std::underflow_error("Money addition underflow");
    }
    return Money(m_cents + other.m_cents);
}

Money Money::operator-(const Money& other) const {
    if (other.m_cents < 0 && m_cents > std::numeric_limits<int64_t>::max() + other.m_cents) {
        throw std::overflow_error("Money subtraction overflow");
    }
    if (other.m_cents > 0 && m_cents < std::numeric_limits<int64_t>::min() + other.m_cents) {
        throw std::underflow_error("Money subtraction underflow");
    }
    return Money(m_cents - other.m_cents);
}

Money Money::operator*(int64_t scalar) const {
    if (scalar > 0) {
        if (m_cents > std::numeric_limits<int64_t>::max() / scalar ||
            m_cents < std::numeric_limits<int64_t>::min() / scalar) {
            throw std::overflow_error("Money multiplication overflow");
        }
    } else if (scalar < -1) {
        if (m_cents < std::numeric_limits<int64_t>::max() / scalar ||
            m_cents > std::numeric_limits<int64_t>::min() / scalar) {
            throw std::overflow_error("Money multiplication overflow");
        }
    } else if (scalar == -1) {
        if (m_cents == std::numeric_limits<int64_t>::min()) {
            throw std::overflow_error("Money multiplication overflow");
        }
    }
    return Money(m_cents * scalar);
}

Money Money::operator*(double scalar) const {
    if (std::isnan(scalar) || std::isinf(scalar)) {
        throw std::invalid_argument("Multiplication by NaN or infinity");
    }
    double result = std::round(static_cast<double>(m_cents) * scalar);
    if (result >= static_cast<double>(std::numeric_limits<int64_t>::max()) ||
        result < static_cast<double>(std::numeric_limits<int64_t>::min())) {
        throw std::overflow_error("Money multiplication overflow");
    }
    return Money(static_cast<int64_t>(result));
}

Money Money::operator/(int64_t scalar) const {
    if (scalar == 0) {
        throw std::invalid_argument("Division by zero");
    }
    if (m_cents == std::numeric_limits<int64_t>::min() && scalar == -1) {
        throw std::overflow_error("Money division overflow");
    }
    return Money(m_cents / scalar);
}

Money Money::operator/(double divisor) const {
    if (divisor == 0.0 || std::isnan(divisor) || std::isinf(divisor)) {
        throw std::invalid_argument("Division by zero or invalid divisor");
    }
    double result = std::round(static_cast<double>(m_cents) / divisor);
    if (result >= static_cast<double>(std::numeric_limits<int64_t>::max()) ||
        result < static_cast<double>(std::numeric_limits<int64_t>::min())) {
        throw std::overflow_error("Money division overflow");
    }
    return Money(static_cast<int64_t>(result));
}

Money& Money::operator+=(const Money& other) {
    *this = *this + other;
    return *this;
}

Money& Money::operator-=(const Money& other) {
    *this = *this - other;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const Money& m) {
    return os << m.toString();
}

} // namespace JBank
