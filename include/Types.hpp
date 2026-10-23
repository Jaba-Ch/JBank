#pragma once

#include "Money.hpp"
#include <atomic>
#include <chrono>
#include <compare>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace JBank {

// ============================================================================
// Date: Gregorian Calendar Date Record
// ============================================================================
struct Date {
    int year{1970};
    int month{1};
    int day{1};

    constexpr Date() noexcept = default;
    constexpr Date(int y, int m, int d) noexcept : year(y), month(m), day(d) {}

    [[nodiscard]] constexpr bool isValid() const noexcept {
        if (year <= 0) {
            return false;
        }
        if (month < 1 || month > 12) {
            return false;
        }

        int maxDays = 31;
        switch (month) {
            case 4: case 6: case 9: case 11:
                maxDays = 30;
                break;
            case 2: {
                const bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
                maxDays = isLeap ? 29 : 28;
                break;
            }
            default:
                maxDays = 31;
                break;
        }

        return day >= 1 && day <= maxDays;
    }

    [[nodiscard]] std::string toString() const {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return std::string(buf);
    }

    auto operator<=>(const Date& other) const = default;
    bool operator==(const Date& other) const = default;

    friend std::ostream& operator<<(std::ostream& os, const Date& d) {
        return os << d.toString();
    }
};

// ============================================================================
// Customer: Client Profile Domain Record
// ============================================================================
struct Customer {
    std::string id;
    std::string firstName;
    std::string lastName;
    Date dob;
    std::string email;

    Customer() = default;
    Customer(std::string id, std::string firstName, std::string lastName, Date dob, std::string email)
        : id(std::move(id)),
          firstName(std::move(firstName)),
          lastName(std::move(lastName)),
          dob(dob),
          email(std::move(email)) {}

    [[nodiscard]] std::string getFullName() const {
        if (firstName.empty()) {
            return lastName;
        }
        if (lastName.empty()) {
            return firstName;
        }
        return firstName + " " + lastName;
    }

    auto operator<=>(const Customer& other) const = default;
    bool operator==(const Customer& other) const = default;

    friend std::ostream& operator<<(std::ostream& os, const Customer& c) {
        os << "Customer[ID: " << c.id
           << ", Name: " << c.getFullName()
           << ", DOB: " << c.dob.toString()
           << ", Email: " << c.email << "]";
        return os;
    }
};

// ============================================================================
// TransactionType: Audit Ledger Operation Categorization
// ============================================================================
enum class TransactionType {
    Deposit,
    Withdrawal,
    TransferIn,
    TransferOut,
    Interest,
    Fee,
    StockBuy,
    StockSell,
    BondBuy,
    BondSell
};

[[nodiscard]] constexpr std::string_view to_string(TransactionType type) noexcept {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "TransferIn";
        case TransactionType::TransferOut: return "TransferOut";
        case TransactionType::Interest:    return "Interest";
        case TransactionType::Fee:         return "Fee";
        case TransactionType::StockBuy:    return "StockBuy";
        case TransactionType::StockSell:   return "StockSell";
        case TransactionType::BondBuy:     return "BondBuy";
        case TransactionType::BondSell:    return "BondSell";
    }
    return "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, TransactionType type) {
    return os << to_string(type);
}

// ============================================================================
// Transaction: Immutable Historical Audit Record
// ============================================================================
struct Transaction {
    std::string id;
    std::chrono::system_clock::time_point timestamp;
    TransactionType type{TransactionType::Deposit};
    Money amount;
    Money resultingBalance;
    std::string description;

    static Transaction create(TransactionType type, Money amount, Money resultingBalance, std::string description = "") {
        static std::atomic<uint64_t> s_txCounter{100000};
        const auto now = std::chrono::system_clock::now();
        const uint64_t num = s_txCounter.fetch_add(1, std::memory_order_relaxed);
        std::string id = "TXN-" + std::to_string(num);
        return Transaction{
            .id = std::move(id),
            .timestamp = now,
            .type = type,
            .amount = amount,
            .resultingBalance = resultingBalance,
            .description = std::move(description)
        };
    }

    [[nodiscard]] std::string getFormattedTimestamp() const {
        const std::time_t tt = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_buf{};
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
        localtime_s(&tm_buf, &tt);
#else
        localtime_r(&tt, &tm_buf);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        return std::string(buf);
    }

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "[" << getFormattedTimestamp() << "] "
            << to_string(type) << " "
            << amount.toString()
            << " (Balance: " << resultingBalance.toString() << ")";
        if (!description.empty()) {
            oss << " - " << description;
        }
        return oss.str();
    }

    auto operator<=>(const Transaction& other) const = default;
    bool operator==(const Transaction& other) const = default;

    friend std::ostream& operator<<(std::ostream& os, const Transaction& tx) {
        return os << tx.toString();
    }
};

} // namespace JBank
