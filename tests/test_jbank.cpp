#include "Accounts.hpp"
#include "Investment.hpp"
#include "JBank.hpp"
#include "Money.hpp"
#include "Types.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using JBank::BankEngine;
using JBank::BaseAccount;
using JBank::BondHolding;
using JBank::CDAccount;
using JBank::CheckingAccount;
using JBank::Customer;
using JBank::Date;
using JBank::IAccount;
using JBank::InvestmentAccount;
using JBank::Money;
using JBank::SavingsAccount;
using JBank::StockHolding;
using JBank::Transaction;
using JBank::TransactionType;
using JBank::to_string;

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg)                                            \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "Assertion FAILED: " << (msg)                    \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";    \
            std::abort();                                                  \
        }                                                                  \
    } while (0)

#define RUN_TEST(fn)                                                      \
    do {                                                                   \
        ++g_tests_run;                                                     \
        std::cout << "[RUN] " << #fn << "..." << std::endl;                \
        fn();                                                              \
        ++g_tests_passed;                                                  \
        std::cout << "[PASS] " << #fn << std::endl;                        \
    } while (0)

// Test 1: Default construction and explicit cents construction
void test_default_construction() {
    constexpr Money m_default;
    static_assert(m_default.getCents() == 0, "Default Money must be 0 cents");
    TEST_ASSERT(m_default.getCents() == 0, "Default Money should have 0 cents");
    TEST_ASSERT(m_default.toDollars() == 0.0, "Default Money toDollars should be 0.0");

    constexpr Money m_explicit(10050);
    static_assert(m_explicit.getCents() == 10050, "Explicit Money must retain cents");
    TEST_ASSERT(m_explicit.getCents() == 10050, "Explicit cents constructor failed");
    TEST_ASSERT(m_explicit.toDollars() == 100.50, "Explicit cents toDollars failed");
}

// Test 2: Dollar conversion and zero floating-point drift
void test_from_dollars_and_zero_drift() {
    Money m1 = Money::fromDollars(100.50);
    TEST_ASSERT(m1.getCents() == 10050, "fromDollars(100.50) must equal 10050 cents");
    TEST_ASSERT(m1.toDollars() == 100.50, "toDollars() must return 100.50");

    Money m2 = Money::fromDollars(0.01);
    TEST_ASSERT(m2.getCents() == 1, "fromDollars(0.01) must equal 1 cent");

    Money m3 = Money::fromDollars(0.00);
    TEST_ASSERT(m3.getCents() == 0, "fromDollars(0.00) must equal 0 cents");

    Money m4 = Money::fromDollars(-50.20);
    TEST_ASSERT(m4.getCents() == -5020, "fromDollars(-50.20) must equal -5020 cents");
    TEST_ASSERT(m4.toDollars() == -50.20, "toDollars() must return -50.20");

    // Zero-drift test: common IEEE-754 precision issues (0.1 + 0.2 != 0.3)
    Money drift1 = Money::fromDollars(0.10 + 0.20);
    TEST_ASSERT(drift1.getCents() == 30, "0.10 + 0.20 dollars must round cleanly to 30 cents");

    Money drift2 = Money::fromDollars(19.99);
    TEST_ASSERT(drift2.getCents() == 1999, "19.99 dollars must convert to 1999 cents");

    Money drift3 = Money::fromDollars(0.07);
    TEST_ASSERT(drift3.getCents() == 7, "0.07 dollars must convert to 7 cents");

    // Invalid arguments (NaN / Infinity)
    bool threw_nan = false;
    try {
        (void)Money::fromDollars(NAN);
    } catch (const std::invalid_argument&) {
        threw_nan = true;
    }
    TEST_ASSERT(threw_nan, "fromDollars(NAN) must throw std::invalid_argument");

    bool threw_inf = false;
    try {
        (void)Money::fromDollars(INFINITY);
    } catch (const std::invalid_argument&) {
        threw_inf = true;
    }
    TEST_ASSERT(threw_inf, "fromDollars(INFINITY) must throw std::invalid_argument");

    // Float-to-int cast boundary (2^63 cents overflow check)
    bool threw_bound = false;
    try {
        // 92233720368547758.08 * 100 == 9223372036854775808.0 (2^63)
        (void)Money::fromDollars(92233720368547758.08);
    } catch (const std::overflow_error&) {
        threw_bound = true;
    }
    TEST_ASSERT(threw_bound, "fromDollars(2^63 / 100) must throw std::overflow_error");
}

// Test 3: Addition and Subtraction
void test_addition_and_subtraction() {
    Money m1 = Money::fromDollars(100.50);
    Money m2 = Money::fromDollars(50.25);

    Money sum = m1 + m2;
    TEST_ASSERT(sum.getCents() == 15075, "100.50 + 50.25 must be 150.75 (15075 cents)");

    Money diff = m1 - m2;
    TEST_ASSERT(diff.getCents() == 5025, "100.50 - 50.25 must be 50.25 (5025 cents)");

    // Negative addition
    Money neg = Money::fromDollars(-30.00);
    Money net = m1 + neg;
    TEST_ASSERT(net.getCents() == 7050, "100.50 + (-30.00) must be 70.50 (7050 cents)");

    // Compound assignments
    Money acc(1000);
    acc += Money(500);
    TEST_ASSERT(acc.getCents() == 1500, "+= operator failed");
    acc -= Money(300);
    TEST_ASSERT(acc.getCents() == 1200, "-= operator failed");

    // Identity
    Money zero;
    TEST_ASSERT((m1 + zero).getCents() == m1.getCents(), "Adding zero must preserve value");
    TEST_ASSERT((m1 - zero).getCents() == m1.getCents(), "Subtracting zero must preserve value");
}

// Test 4: Multiplication and Division
void test_multiplication_and_division() {
    Money m(1000); // $10.00

    // Multiplication by int64_t scalar
    Money m_int = m * static_cast<int64_t>(3);
    TEST_ASSERT(m_int.getCents() == 3000, "1000 * 3 must be 3000");

    Money m_neg = m * static_cast<int64_t>(-2);
    TEST_ASSERT(m_neg.getCents() == -2000, "1000 * -2 must be -2000");

    // Commutative multiplication
    Money m_comm = static_cast<int64_t>(4) * m;
    TEST_ASSERT(m_comm.getCents() == 4000, "4 * 1000 must be 4000");

    // Multiplication by double scalar
    Money m_dbl = m * 2.5;
    TEST_ASSERT(m_dbl.getCents() == 2500, "1000 * 2.5 must be 2500");

    Money m_dbl_comm = 1.25 * m;
    TEST_ASSERT(m_dbl_comm.getCents() == 1250, "1.25 * 1000 must be 1250");

    // Division by int64_t scalar
    Money div1 = Money(1500) / static_cast<int64_t>(3);
    TEST_ASSERT(div1.getCents() == 500, "1500 / 3 must be 500");

    Money div2 = Money(-1500) / static_cast<int64_t>(3);
    TEST_ASSERT(div2.getCents() == -500, "-1500 / 3 must be -500");

    Money div3 = Money(1500) / static_cast<int64_t>(-3);
    TEST_ASSERT(div3.getCents() == -500, "1500 / -3 must be -500");
}

// Test 5: Division by zero throws
void test_division_by_zero_throws() {
    Money m(5000);
    bool threw = false;
    try {
        Money result = m / static_cast<int64_t>(0);
        (void)result;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Division by 0 must throw std::invalid_argument");
}

// Test 6: Overflow and underflow validation
void test_overflow_and_underflow() {
    Money maxMoney(INT64_MAX);
    Money minMoney(INT64_MIN);
    Money one(1);
    Money minusOne(-1);

    // Addition overflow
    bool threw_add_overflow = false;
    try {
        Money res = maxMoney + one;
        (void)res;
    } catch (const std::overflow_error&) {
        threw_add_overflow = true;
    }
    TEST_ASSERT(threw_add_overflow, "INT64_MAX + 1 must throw std::overflow_error");

    // Addition underflow
    bool threw_add_underflow = false;
    try {
        Money res = minMoney + minusOne;
        (void)res;
    } catch (const std::underflow_error&) {
        threw_add_underflow = true;
    }
    TEST_ASSERT(threw_add_underflow, "INT64_MIN + (-1) must throw std::underflow_error");

    // Subtraction overflow
    bool threw_sub_overflow = false;
    try {
        Money res = maxMoney - minusOne;
        (void)res;
    } catch (const std::overflow_error&) {
        threw_sub_overflow = true;
    }
    TEST_ASSERT(threw_sub_overflow, "INT64_MAX - (-1) must throw std::overflow_error");

    // Subtraction underflow
    bool threw_sub_underflow = false;
    try {
        Money res = minMoney - one;
        (void)res;
    } catch (const std::underflow_error&) {
        threw_sub_underflow = true;
    }
    TEST_ASSERT(threw_sub_underflow, "INT64_MIN - 1 must throw std::underflow_error");

    // Multiplication overflow
    bool threw_mul_overflow = false;
    try {
        Money res = maxMoney * static_cast<int64_t>(2);
        (void)res;
    } catch (const std::overflow_error&) {
        threw_mul_overflow = true;
    }
    TEST_ASSERT(threw_mul_overflow, "INT64_MAX * 2 must throw std::overflow_error");

    // Division overflow (INT64_MIN / -1)
    bool threw_div_overflow = false;
    try {
        Money res = minMoney / static_cast<int64_t>(-1);
        (void)res;
    } catch (const std::overflow_error&) {
        threw_div_overflow = true;
    }
    TEST_ASSERT(threw_div_overflow, "INT64_MIN / -1 must throw std::overflow_error");

    // Double scalar multiplication overflow (2^63 boundary)
    bool threw_mul_dbl_overflow = false;
    try {
        Money res = Money(1000000) * (9223372036854775808.0 / 1000000.0);
        (void)res;
    } catch (const std::overflow_error&) {
        threw_mul_dbl_overflow = true;
    }
    TEST_ASSERT(threw_mul_dbl_overflow, "Money * double at 2^63 boundary must throw std::overflow_error");

    // Double divisor overflow (2^63 boundary)
    bool threw_div_dbl_overflow = false;
    try {
        Money res = Money(1000000) / (1000000.0 / 9223372036854775808.0);
        (void)res;
    } catch (const std::overflow_error&) {
        threw_div_dbl_overflow = true;
    }
    TEST_ASSERT(threw_div_dbl_overflow, "Money / double at 2^63 boundary must throw std::overflow_error");

    // Double division by zero
    bool threw_div_dbl_zero = false;
    try {
        Money res = Money(100) / 0.0;
        (void)res;
    } catch (const std::invalid_argument&) {
        threw_div_dbl_zero = true;
    }
    TEST_ASSERT(threw_div_dbl_zero, "Money / 0.0 must throw std::invalid_argument");
}

// Test 7: Comparison operators
void test_comparison_operators() {
    Money m100(100);
    Money m200(200);
    Money m100_copy(100);

    TEST_ASSERT(m100 == m100_copy, "m100 must equal m100_copy");
    TEST_ASSERT(m100 != m200, "m100 must not equal m200");
    TEST_ASSERT(m100 < m200, "m100 must be less than m200");
    TEST_ASSERT(m200 > m100, "m200 must be greater than m100");
    TEST_ASSERT(m100 <= m200, "m100 <= m200 check failed");
    TEST_ASSERT(m100 <= m100_copy, "m100 <= m100_copy check failed");
    TEST_ASSERT(m200 >= m100, "m200 >= m100 check failed");
    TEST_ASSERT(m100 >= m100_copy, "m100 >= m100_copy check failed");

    // Three-way comparison
    TEST_ASSERT((m100 <=> m200) < 0, "m100 <=> m200 must be < 0");
    TEST_ASSERT((m200 <=> m100) > 0, "m200 <=> m100 must be > 0");
    TEST_ASSERT((m100 <=> m100_copy) == 0, "m100 <=> m100_copy must be == 0");
}

// Test 8: String formatting and stream insertion
void test_string_formatting_and_stream() {
    TEST_ASSERT(Money(0).toString() == "$0.00", "0 cents must format as $0.00");
    TEST_ASSERT(Money(10050).toString() == "$100.50", "10050 cents must format as $100.50");
    TEST_ASSERT(Money(125075).toString() == "$1,250.75", "125075 cents must format as $1,250.75");
    TEST_ASSERT(Money(-5020).toString() == "-$50.20", "-5020 cents must format as -$50.20");
    TEST_ASSERT(Money(100000).toString() == "$1,000.00", "100000 cents must format as $1,000.00");
    TEST_ASSERT(Money(-100000).toString() == "-$1,000.00", "-100000 cents must format as -$1,000.00");
    TEST_ASSERT(Money(5).toString() == "$0.05", "5 cents must format as $0.05");
    TEST_ASSERT(Money(-5).toString() == "-$0.05", "-5 cents must format as -$0.05");
    TEST_ASSERT(Money(50).toString() == "$0.50", "50 cents must format as $0.50");
    TEST_ASSERT(Money(-50).toString() == "-$0.50", "-50 cents must format as -$0.50");
    TEST_ASSERT(Money(999).toString() == "$9.99", "999 cents must format as $9.99");
    TEST_ASSERT(Money(100000000).toString() == "$1,000,000.00", "1,000,000.00 format failed");

    // Stream operator
    std::ostringstream oss;
    oss << Money(125075);
    TEST_ASSERT(oss.str() == "$1,250.75", "Stream insertion must match toString()");
}

// Test 9: Date construction, default values, and comprehensive validation
void test_date_construction_and_validation() {
    // Default construction
    constexpr Date d0;
    static_assert(d0.year == 1970 && d0.month == 1 && d0.day == 1, "Default date must be 1970-01-01");
    TEST_ASSERT(d0.year == 1970, "Default year must be 1970");
    TEST_ASSERT(d0.month == 1, "Default month must be 1");
    TEST_ASSERT(d0.day == 1, "Default day must be 1");
    TEST_ASSERT(d0.isValid(), "Default date 1970-01-01 must be valid");
    TEST_ASSERT(d0.toString() == "1970-01-01", "Default date toString failed");

    // Parameterized constructor
    Date d1(2026, 10, 17);
    TEST_ASSERT(d1.year == 2026 && d1.month == 10 && d1.day == 17, "Parameterized constructor fields failed");
    TEST_ASSERT(d1.isValid(), "2026-10-17 must be valid");

    // Leap year rules:
    // 1. Divisible by 400 -> Leap year
    TEST_ASSERT(Date(2000, 2, 29).isValid(), "2000-02-29 (divisible by 400) must be valid");
    TEST_ASSERT(Date(2400, 2, 29).isValid(), "2400-02-29 (divisible by 400) must be valid");
    TEST_ASSERT(!Date(2000, 2, 30).isValid(), "2000-02-30 must be invalid");

    // 2. Divisible by 100 but not 400 -> Non-leap year
    TEST_ASSERT(!Date(1900, 2, 29).isValid(), "1900-02-29 (divisible by 100, not 400) must be invalid");
    TEST_ASSERT(Date(1900, 2, 28).isValid(), "1900-02-28 must be valid");
    TEST_ASSERT(!Date(2100, 2, 29).isValid(), "2100-02-29 (divisible by 100, not 400) must be invalid");
    TEST_ASSERT(Date(2100, 2, 28).isValid(), "2100-02-28 must be valid");

    // 3. Divisible by 4, not 100 -> Leap year
    TEST_ASSERT(Date(2024, 2, 29).isValid(), "2024-02-29 (leap year) must be valid");
    TEST_ASSERT(!Date(2024, 2, 30).isValid(), "2024-02-30 must be invalid");

    // 4. Non-leap common year
    TEST_ASSERT(!Date(2023, 2, 29).isValid(), "2023-02-29 (non-leap year) must be invalid");
    TEST_ASSERT(Date(2023, 2, 28).isValid(), "2023-02-28 must be valid");

    // Days per month: 31-day months (Jan, Mar, May, Jul, Aug, Oct, Dec)
    TEST_ASSERT(Date(2023, 1, 31).isValid(), "Jan 31 valid");
    TEST_ASSERT(!Date(2023, 1, 32).isValid(), "Jan 32 invalid");
    TEST_ASSERT(Date(2023, 3, 31).isValid(), "Mar 31 valid");
    TEST_ASSERT(!Date(2023, 3, 32).isValid(), "Mar 32 invalid");
    TEST_ASSERT(Date(2023, 5, 31).isValid(), "May 31 valid");
    TEST_ASSERT(!Date(2023, 5, 32).isValid(), "May 32 invalid");
    TEST_ASSERT(Date(2023, 7, 31).isValid(), "Jul 31 valid");
    TEST_ASSERT(!Date(2023, 7, 32).isValid(), "Jul 32 invalid");
    TEST_ASSERT(Date(2023, 8, 31).isValid(), "Aug 31 valid");
    TEST_ASSERT(!Date(2023, 8, 32).isValid(), "Aug 32 invalid");
    TEST_ASSERT(Date(2023, 10, 31).isValid(), "Oct 31 valid");
    TEST_ASSERT(!Date(2023, 10, 32).isValid(), "Oct 32 invalid");
    TEST_ASSERT(Date(2023, 12, 31).isValid(), "Dec 31 valid");
    TEST_ASSERT(!Date(2023, 12, 32).isValid(), "Dec 32 invalid");

    // Days per month: 30-day months (Apr, Jun, Sep, Nov)
    TEST_ASSERT(Date(2023, 4, 30).isValid(), "Apr 30 valid");
    TEST_ASSERT(!Date(2023, 4, 31).isValid(), "Apr 31 invalid");
    TEST_ASSERT(Date(2023, 6, 30).isValid(), "Jun 30 valid");
    TEST_ASSERT(!Date(2023, 6, 31).isValid(), "Jun 31 invalid");
    TEST_ASSERT(Date(2023, 9, 30).isValid(), "Sep 30 valid");
    TEST_ASSERT(!Date(2023, 9, 31).isValid(), "Sep 31 invalid");
    TEST_ASSERT(Date(2023, 11, 30).isValid(), "Nov 30 valid");
    TEST_ASSERT(!Date(2023, 11, 31).isValid(), "Nov 31 invalid");

    // Month boundary validation
    TEST_ASSERT(!Date(2023, 0, 15).isValid(), "Month 0 invalid");
    TEST_ASSERT(!Date(2023, 13, 1).isValid(), "Month 13 invalid");
    TEST_ASSERT(!Date(2023, -1, 10).isValid(), "Negative month invalid");

    // Day boundary validation
    TEST_ASSERT(!Date(2023, 5, 0).isValid(), "Day 0 invalid");
    TEST_ASSERT(!Date(2023, 5, -10).isValid(), "Negative day invalid");

    // Year boundary validation
    TEST_ASSERT(!Date(0, 1, 1).isValid(), "Year 0 invalid");
    TEST_ASSERT(!Date(-100, 1, 1).isValid(), "Negative year invalid");
}

// Test 10: Date string formatting and comparison operators
void test_date_formatting_and_comparisons() {
    // Zero-padding formatting
    TEST_ASSERT(Date(2026, 5, 4).toString() == "2026-05-04", "Single digit month and day must zero-pad");
    TEST_ASSERT(Date(999, 12, 1).toString() == "0999-12-01", "Year < 1000 must zero-pad to 4 digits");
    TEST_ASSERT(Date(7, 3, 9).toString() == "0007-03-09", "Single digit year must zero-pad to 4 digits");

    // Stream operator
    std::ostringstream oss;
    oss << Date(2026, 10, 17);
    TEST_ASSERT(oss.str() == "2026-10-17", "Date stream << operator must match toString()");

    // Relational operators & three-way comparison <=>
    Date d_base(2024, 6, 15);
    Date d_same(2024, 6, 15);
    Date d_next_day(2024, 6, 16);
    Date d_next_month(2024, 7, 1);
    Date d_next_year(2025, 1, 1);

    TEST_ASSERT(d_base == d_same, "Equal dates must be ==");
    TEST_ASSERT(!(d_base != d_same), "Equal dates must not be !=");
    TEST_ASSERT(d_base <= d_same && d_base >= d_same, "<= and >= on equal dates");
    TEST_ASSERT((d_base <=> d_same) == 0, "<=> on equal dates must be 0");

    TEST_ASSERT(d_base < d_next_day, "Earlier day must be <");
    TEST_ASSERT(d_next_day > d_base, "Later day must be >");
    TEST_ASSERT((d_base <=> d_next_day) < 0, "<=> on earlier day must be < 0");

    TEST_ASSERT(d_base < d_next_month, "Earlier month must be <");
    TEST_ASSERT((d_base <=> d_next_month) < 0, "<=> on earlier month must be < 0");

    TEST_ASSERT(d_base < d_next_year, "Earlier year must be <");
    TEST_ASSERT((d_base <=> d_next_year) < 0, "<=> on earlier year must be < 0");
    TEST_ASSERT(d_next_year > d_base, "Later year must be >");
}

// Test 11: Customer domain record
void test_customer_domain_record() {
    // Default constructor
    Customer c_def;
    TEST_ASSERT(c_def.id.empty(), "Default customer id should be empty");
    TEST_ASSERT(c_def.firstName.empty(), "Default customer firstName should be empty");
    TEST_ASSERT(c_def.lastName.empty(), "Default customer lastName should be empty");
    TEST_ASSERT(c_def.email.empty(), "Default customer email should be empty");
    TEST_ASSERT(c_def.getFullName().empty(), "Default customer getFullName() should be empty");

    // Parameterized constructor
    Customer c1("CUST-001", "John", "Doe", Date(1985, 3, 20), "john.doe@bank.com");
    TEST_ASSERT(c1.id == "CUST-001", "Customer id must match");
    TEST_ASSERT(c1.firstName == "John", "Customer firstName must match");
    TEST_ASSERT(c1.lastName == "Doe", "Customer lastName must match");
    TEST_ASSERT(c1.dob == Date(1985, 3, 20), "Customer dob must match");
    TEST_ASSERT(c1.email == "john.doe@bank.com", "Customer email must match");
    TEST_ASSERT(c1.getFullName() == "John Doe", "getFullName() must return 'John Doe'");

    // getFullName() edge cases
    Customer c_first("CUST-002", "Madonna", "", Date(1958, 8, 16), "madonna@music.com");
    TEST_ASSERT(c_first.getFullName() == "Madonna", "getFullName() with empty lastName must return firstName");

    Customer c_last("CUST-003", "", "Cher", Date(1946, 5, 20), "cher@music.com");
    TEST_ASSERT(c_last.getFullName() == "Cher", "getFullName() with empty firstName must return lastName");

    // Equality and comparison
    Customer c1_twin("CUST-001", "John", "Doe", Date(1985, 3, 20), "john.doe@bank.com");
    Customer c2("CUST-004", "Jane", "Smith", Date(1992, 11, 5), "jane.smith@bank.com");

    TEST_ASSERT(c1 == c1_twin, "Identical customers must compare equal");
    TEST_ASSERT(!(c1 != c1_twin), "Identical customers must not compare unequal");
    TEST_ASSERT(c1 != c2, "Distinct customers must compare unequal");
    TEST_ASSERT((c1 <=> c1_twin) == 0, "<=> on identical customers must equal 0");

    // Stream operator
    std::ostringstream oss;
    oss << c1;
    std::string out = oss.str();
    TEST_ASSERT(out.find("CUST-001") != std::string::npos, "Customer stream output must contain ID");
    TEST_ASSERT(out.find("John Doe") != std::string::npos, "Customer stream output must contain full name");
    TEST_ASSERT(out.find("1985-03-20") != std::string::npos, "Customer stream output must contain DOB");
    TEST_ASSERT(out.find("john.doe@bank.com") != std::string::npos, "Customer stream output must contain email");
}

// Test 12: TransactionType enumeration and string conversion
void test_transaction_type_enumeration() {
    TEST_ASSERT(to_string(TransactionType::Deposit) == "Deposit", "Deposit to_string failed");
    TEST_ASSERT(to_string(TransactionType::Withdrawal) == "Withdrawal", "Withdrawal to_string failed");
    TEST_ASSERT(to_string(TransactionType::TransferIn) == "TransferIn", "TransferIn to_string failed");
    TEST_ASSERT(to_string(TransactionType::TransferOut) == "TransferOut", "TransferOut to_string failed");
    TEST_ASSERT(to_string(TransactionType::Interest) == "Interest", "Interest to_string failed");
    TEST_ASSERT(to_string(TransactionType::Fee) == "Fee", "Fee to_string failed");
    TEST_ASSERT(to_string(TransactionType::StockBuy) == "StockBuy", "StockBuy to_string failed");
    TEST_ASSERT(to_string(TransactionType::StockSell) == "StockSell", "StockSell to_string failed");
    TEST_ASSERT(to_string(TransactionType::BondBuy) == "BondBuy", "BondBuy to_string failed");
    TEST_ASSERT(to_string(TransactionType::BondSell) == "BondSell", "BondSell to_string failed");

    // Stream operator
    std::ostringstream oss;
    oss << TransactionType::Deposit << " " << TransactionType::StockBuy;
    TEST_ASSERT(oss.str() == "Deposit StockBuy", "TransactionType stream insertion failed");
}

// Test 13: Transaction domain record and factory creation
void test_transaction_factory_and_records() {
    auto before = std::chrono::system_clock::now();
    Transaction tx1 = Transaction::create(
        TransactionType::Deposit,
        Money::fromDollars(1000.50),
        Money::fromDollars(5000.50),
        "Payroll Direct Deposit"
    );
    auto after = std::chrono::system_clock::now();

    // Field assertions
    TEST_ASSERT(!tx1.id.empty(), "Generated transaction ID must not be empty");
    TEST_ASSERT(tx1.type == TransactionType::Deposit, "Transaction type must be Deposit");
    TEST_ASSERT(tx1.amount == Money(100050), "Amount must match $1000.50 (100050 cents)");
    TEST_ASSERT(tx1.resultingBalance == Money(500050), "Resulting balance must match $5000.50 (500050 cents)");
    TEST_ASSERT(tx1.description == "Payroll Direct Deposit", "Description must match");
    TEST_ASSERT(tx1.timestamp >= before && tx1.timestamp <= after, "Timestamp must be within execution window");

    // Unique ID generation across multiple creations
    Transaction tx2 = Transaction::create(
        TransactionType::Withdrawal,
        Money::fromDollars(200.0),
        Money::fromDollars(4800.50),
        "ATM Cash"
    );
    Transaction tx3 = Transaction::create(
        TransactionType::Fee,
        Money::fromDollars(2.50),
        Money::fromDollars(4798.00)
    );

    TEST_ASSERT(tx1.id != tx2.id, "Consecutive transactions must produce unique IDs");
    TEST_ASSERT(tx2.id != tx3.id, "Consecutive transactions must produce unique IDs");
    TEST_ASSERT(tx3.description.empty(), "Default description parameter must be empty string");

    // Formatted timestamp check
    std::string ts = tx1.getFormattedTimestamp();
    TEST_ASSERT(!ts.empty(), "Formatted timestamp must not be empty");
    TEST_ASSERT(ts.length() >= 19, "Formatted timestamp must be at least 19 chars (YYYY-MM-DD HH:MM:SS)");
    TEST_ASSERT(ts[4] == '-' && ts[7] == '-', "Timestamp date delimiter verification failed");

    // toString formatting
    std::string str1 = tx1.toString();
    TEST_ASSERT(str1.find("Deposit") != std::string::npos, "toString must include type name");
    TEST_ASSERT(str1.find("$1,000.50") != std::string::npos, "toString must include formatted amount");
    TEST_ASSERT(str1.find("$5,000.50") != std::string::npos, "toString must include formatted resulting balance");
    TEST_ASSERT(str1.find("Payroll Direct Deposit") != std::string::npos, "toString must include description");

    // Stream operator
    std::ostringstream oss;
    oss << tx1;
    TEST_ASSERT(oss.str() == tx1.toString(), "Stream insertion operator must match toString()");

    // Equality and comparison
    Transaction tx1_copy = tx1;
    TEST_ASSERT(tx1 == tx1_copy, "Copied transaction must be equal");
    TEST_ASSERT(tx1 != tx2, "Distinct transactions must not be equal");
}

// ============================================================================
// Task 4: Account Hierarchy & Banking Accounts Tests
// ============================================================================

// Concrete wrapper to test BaseAccount directly
class ConcreteBaseAccount : public BaseAccount {
public:
    using BaseAccount::BaseAccount;
};

// Test 14: BaseAccount operations, deposit/withdrawal validation, and thread safety
void test_base_account_operations_and_thread_safety() {
    ConcreteBaseAccount acc("ACC-1001", Money::fromDollars(500.0), true);

    TEST_ASSERT(acc.getAccountNumber() == "ACC-1001", "Account number must match ACC-1001");
    TEST_ASSERT(acc.getBalance() == Money::fromDollars(500.0), "Initial balance must be $500.00");
    TEST_ASSERT(acc.isFDICInsured(), "Account must be FDIC insured");

    // History contains initial deposit
    auto history = acc.getTransactionHistory();
    TEST_ASSERT(history.size() == 1, "History must have 1 transaction for initial balance");
    TEST_ASSERT(history[0].type == TransactionType::Deposit, "Initial transaction must be Deposit");
    TEST_ASSERT(history[0].amount == Money::fromDollars(500.0), "Initial transaction amount must be $500.00");

    // Deposit positive
    bool dep_ok = acc.deposit(Money::fromDollars(200.0), "Direct Deposit");
    TEST_ASSERT(dep_ok, "Valid deposit must return true");
    TEST_ASSERT(acc.getBalance() == Money::fromDollars(700.0), "Balance after deposit must be $700.00");

    // Deposit zero or negative
    TEST_ASSERT(!acc.deposit(Money(0)), "Deposit of $0 must fail");
    TEST_ASSERT(!acc.deposit(Money(-100)), "Deposit of negative amount must fail");
    TEST_ASSERT(acc.getBalance() == Money::fromDollars(700.0), "Balance must not change on rejected deposit");

    // Withdraw positive
    bool wd_ok = acc.withdraw(Money::fromDollars(150.0), "ATM Cash");
    TEST_ASSERT(wd_ok, "Valid withdrawal must return true");
    TEST_ASSERT(acc.getBalance() == Money::fromDollars(550.0), "Balance after withdrawal must be $550.00");

    // Withdraw non-positive or exceeding balance
    TEST_ASSERT(!acc.withdraw(Money(0)), "Withdraw of $0 must fail");
    TEST_ASSERT(!acc.withdraw(Money(-50)), "Withdraw of negative amount must fail");
    TEST_ASSERT(!acc.withdraw(Money::fromDollars(600.0)), "Withdraw exceeding balance must fail");
    TEST_ASSERT(acc.getBalance() == Money::fromDollars(550.0), "Balance must remain unchanged after failed withdrawal");

    // Multithreaded concurrency test
    // 4 threads deposit $10.00 50 times each (+ $2000.00)
    // 2 threads withdraw $5.00 50 times each (- $500.00)
    // Net change: + $1500.00 -> expected balance: $550 + $1500 = $2050.00
    std::vector<std::thread> threads;
    threads.reserve(6);
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&acc]() {
            for (int i = 0; i < 50; ++i) {
                acc.deposit(Money::fromDollars(10.0), "Concurrent Deposit");
            }
        });
    }
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&acc]() {
            for (int i = 0; i < 50; ++i) {
                acc.withdraw(Money::fromDollars(5.0), "Concurrent Withdraw");
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    TEST_ASSERT(acc.getBalance() == Money::fromDollars(2050.0), "Concurrent balance must strictly match $2050.00");
    auto fullHistory = acc.getTransactionHistory();
    // 1 (init) + 1 (dep) + 1 (wd) + 200 (concurrent dep) + 100 (concurrent wd) = 303 transactions
    TEST_ASSERT(fullHistory.size() == 303, "All concurrent transactions must be recorded without data race");
}

// Test 15: CheckingAccount and overdraft protection
void test_checking_account_and_overdraft() {
    CheckingAccount chk("CHK-2001", Money::fromDollars(100.0), Money::fromDollars(200.0));

    TEST_ASSERT(chk.getAccountNumber() == "CHK-2001", "Checking account number check");
    TEST_ASSERT(chk.getBalance() == Money::fromDollars(100.0), "Initial balance $100.00");
    TEST_ASSERT(chk.getOverdraftLimit() == Money::fromDollars(200.0), "Overdraft limit $200.00");
    TEST_ASSERT(chk.isFDICInsured(), "CheckingAccount must be FDIC insured");

    // Normal withdrawal within balance
    TEST_ASSERT(chk.withdraw(Money::fromDollars(60.0), "Coffee & Lunch"), "Withdraw within balance succeeds");
    TEST_ASSERT(chk.getBalance() == Money::fromDollars(40.0), "Balance should be $40.00");

    // Withdrawal exceeding balance into overdraft
    TEST_ASSERT(chk.withdraw(Money::fromDollars(100.0), "Utility Bill"), "Withdrawal into overdraft succeeds");
    TEST_ASSERT(chk.getBalance() == Money::fromDollars(-60.0), "Balance should be -$60.00");

    // Withdrawal up to exact overdraft limit: balance -$60, limit $200 -> remaining available $140
    TEST_ASSERT(chk.withdraw(Money::fromDollars(140.0), "Emergency"), "Withdrawal to exact overdraft limit succeeds");
    TEST_ASSERT(chk.getBalance() == Money::fromDollars(-200.0), "Balance should be exactly -$200.00");

    // Withdrawal exceeding overdraft limit by 1 cent
    TEST_ASSERT(!chk.withdraw(Money(1), "Overdraft violation"), "Withdrawal beyond overdraft limit must fail");
    TEST_ASSERT(chk.getBalance() == Money::fromDollars(-200.0), "Balance must remain unchanged after rejection");

    // Non-positive withdrawal
    TEST_ASSERT(!chk.withdraw(Money(0)), "Withdraw $0 must fail");
    TEST_ASSERT(!chk.withdraw(Money(-500)), "Withdraw negative must fail");

    // Deposit to recover from overdraft
    TEST_ASSERT(chk.deposit(Money::fromDollars(350.0), "Deposit paycheck"), "Deposit into overdrawn account succeeds");
    TEST_ASSERT(chk.getBalance() == Money::fromDollars(150.0), "Balance should recover to +$150.00");

    // CheckingAccount with zero overdraft limit
    CheckingAccount chk_zero("CHK-2002", Money::fromDollars(50.0), Money(0));
    TEST_ASSERT(!chk_zero.withdraw(Money::fromDollars(50.01)), "Zero-overdraft account rejects any overdraft");
    TEST_ASSERT(chk_zero.withdraw(Money::fromDollars(50.0)), "Zero-overdraft account withdraws to exact zero");
    TEST_ASSERT(chk_zero.getBalance() == Money(0), "Zero-overdraft balance is $0.00");
}

// Test 16: SavingsAccount and interest accrual calculation
void test_savings_account_and_interest_accrual() {
    SavingsAccount sav("SAV-3001", Money::fromDollars(12000.0), 0.05); // 5% APY

    TEST_ASSERT(sav.getAccountNumber() == "SAV-3001", "Savings account number check");
    TEST_ASSERT(sav.getBalance() == Money::fromDollars(12000.0), "Initial balance $12,000.00");
    TEST_ASSERT(sav.getAnnualRate() == 0.05, "Annual rate check");
    TEST_ASSERT(sav.isFDICInsured(), "SavingsAccount must be FDIC insured");

    // Rejection of overdraft
    TEST_ASSERT(!sav.withdraw(Money::fromDollars(12000.01)), "SavingsAccount must reject overdraft");
    TEST_ASSERT(sav.getBalance() == Money::fromDollars(12000.0), "Balance unchanged after overdraft rejection");

    // First month interest accrual:
    // 12,000.00 * (0.05 / 12) = 50.00 (5,000 cents)
    Money earned1 = sav.applyMonthlyInterest();
    TEST_ASSERT(earned1 == Money::fromDollars(50.0), "Month 1 interest must be $50.00");
    TEST_ASSERT(sav.getBalance() == Money::fromDollars(12050.0), "Balance after month 1 must be $12,050.00");

    auto hist = sav.getTransactionHistory();
    TEST_ASSERT(!hist.empty(), "History not empty");
    const auto& lastTx = hist.back();
    TEST_ASSERT(lastTx.type == TransactionType::Interest, "Transaction type must be Interest");
    TEST_ASSERT(lastTx.amount == Money::fromDollars(50.0), "Transaction amount must be $50.00");
    TEST_ASSERT(lastTx.resultingBalance == Money::fromDollars(12050.0), "Resulting balance must be $12,050.00");

    // Second month interest accrual (compounding):
    // 1,205,000 * (0.05 / 12) = 5020.8333... -> round -> 5021 cents ($50.21)
    Money earned2 = sav.applyMonthlyInterest();
    TEST_ASSERT(earned2 == Money(5021), "Month 2 interest must be $50.21 (5021 cents)");
    TEST_ASSERT(sav.getBalance() == Money(1210021), "Balance after month 2 must be $12,100.21");

    // Zero balance savings account
    SavingsAccount sav_zero("SAV-3002", Money(0), 0.04);
    Money earned_zero = sav_zero.applyMonthlyInterest();
    TEST_ASSERT(earned_zero == Money(0), "Zero balance earns 0 interest");
    TEST_ASSERT(sav_zero.getBalance() == Money(0), "Zero balance unchanged");
}

// Test 17: CDAccount maturity calculation, penalty enforcement, and fees
void test_cd_account_maturity_and_penalties() {
    CDAccount cd("CD-4001", Money::fromDollars(1000.0), 6, 0.045, Date(2026, 1, 15), Money::fromDollars(50.0));

    TEST_ASSERT(cd.getAccountNumber() == "CD-4001", "CD account number check");
    TEST_ASSERT(cd.getBalance() == Money::fromDollars(1000.0), "Initial balance $1,000.00");
    TEST_ASSERT(cd.getStartDate() == Date(2026, 1, 15), "Start date check");
    TEST_ASSERT(cd.getTermMonths() == 6, "Term months check");
    TEST_ASSERT(cd.getMaturityDate() == Date(2026, 7, 15), "Maturity date 6 months from 2026-01-15 must be 2026-07-15");
    TEST_ASSERT(cd.getEarlyWithdrawalPenalty() == Money::fromDollars(50.0), "Penalty check");
    TEST_ASSERT(cd.isFDICInsured(), "CDAccount must be FDIC insured");

    // Calendar maturity calculations:
    // 1. Month rollover across year boundary: 2026-10-15 + 6 months -> 2027-04-15
    CDAccount cd_roll("CD-4002", Money::fromDollars(500.0), 6, 0.03, Date(2026, 10, 15));
    TEST_ASSERT(cd_roll.getMaturityDate() == Date(2027, 4, 15), "2026-10-15 + 6 months must be 2027-04-15");

    // 2. Month-end clamping in leap year: 2024-01-31 + 1 month -> 2024-02-29
    CDAccount cd_leap("CD-4003", Money::fromDollars(500.0), 1, 0.03, Date(2024, 1, 31));
    TEST_ASSERT(cd_leap.getMaturityDate() == Date(2024, 2, 29), "2024-01-31 + 1 month leap must clamp to 2024-02-29");

    // 3. Month-end clamping in non-leap year: 2023-01-31 + 1 month -> 2023-02-28
    CDAccount cd_nonleap("CD-4004", Money::fromDollars(500.0), 1, 0.03, Date(2023, 1, 31));
    TEST_ASSERT(cd_nonleap.getMaturityDate() == Date(2023, 2, 28), "2023-01-31 + 1 month non-leap must clamp to 2023-02-28");

    // isMatured(checkDate)
    TEST_ASSERT(!cd.isMatured(Date(2026, 7, 14)), "One day before maturity date is not matured");
    TEST_ASSERT(cd.isMatured(Date(2026, 7, 15)), "Exact maturity date is matured");
    TEST_ASSERT(cd.isMatured(Date(2026, 7, 16)), "Day after maturity date is matured");

    // Early withdrawal before maturity:
    cd.setSimulatedDate(Date(2026, 4, 1));
    TEST_ASSERT(!cd.isMatured(), "Simulated date 2026-04-01 is not matured");

    // Insufficient balance to cover withdrawal + penalty:
    // Balance: $1,000.00. Withdrawal: $980.00. Penalty: $50.00. Total required: $1,030.00 > $1,000.00.
    TEST_ASSERT(!cd.withdraw(Money::fromDollars(980.0), "Early large withdrawal"), "Must reject when balance cannot cover amount + penalty");
    TEST_ASSERT(cd.getBalance() == Money::fromDollars(1000.0), "Balance must remain unchanged after rejection");

    // Sufficient balance early withdrawal:
    // Withdraw $200.00. Penalty: $50.00.
    TEST_ASSERT(cd.withdraw(Money::fromDollars(200.0), "Early partial withdrawal"), "Early withdrawal with sufficient funds succeeds");
    TEST_ASSERT(cd.getBalance() == Money::fromDollars(750.0), "Balance must be reduced by $200 + $50 = $250 -> $750.00");

    // Verify transactions in history:
    auto cdHist = cd.getTransactionHistory();
    TEST_ASSERT(cdHist.size() >= 3, "History must have initial deposit, withdrawal, and fee");
    const auto& feeTx = cdHist.back();
    TEST_ASSERT(feeTx.type == TransactionType::Fee, "Penalty transaction must be Fee");
    TEST_ASSERT(feeTx.amount == Money::fromDollars(50.0), "Fee amount must match penalty");
    TEST_ASSERT(feeTx.resultingBalance == Money::fromDollars(750.0), "Resulting balance after fee must be $750.00");

    const auto& wdTx = cdHist[cdHist.size() - 2];
    TEST_ASSERT(wdTx.type == TransactionType::Withdrawal, "Withdrawal transaction must precede fee");
    TEST_ASSERT(wdTx.amount == Money::fromDollars(200.0), "Withdrawal amount must be $200.00");
    TEST_ASSERT(wdTx.resultingBalance == Money::fromDollars(800.0), "Resulting balance after withdrawal must be $800.00");

    // Matured withdrawal after maturity date:
    cd.setSimulatedDate(Date(2026, 8, 1));
    TEST_ASSERT(cd.isMatured(), "Simulated date 2026-08-01 is matured");

    // Withdraw $300.00 without any penalty
    TEST_ASSERT(cd.withdraw(Money::fromDollars(300.0), "Matured withdrawal"), "Matured withdrawal succeeds");
    TEST_ASSERT(cd.getBalance() == Money::fromDollars(450.0), "Balance must be reduced by exactly $300.00 without fee");

    auto cdHistPost = cd.getTransactionHistory();
    const auto& maturedTx = cdHistPost.back();
    TEST_ASSERT(maturedTx.type == TransactionType::Withdrawal, "Matured withdrawal logs Withdrawal only");
    TEST_ASSERT(maturedTx.amount == Money::fromDollars(300.0), "Amount is $300.00");
    TEST_ASSERT(maturedTx.resultingBalance == Money::fromDollars(450.0), "Resulting balance is $450.00");
}

// Test 18: Clean Multi-Owner Composition and Statement Generation
void test_multi_owner_joint_account_composition_and_statement() {
    CheckingAccount joint("CHK-5001", Money::fromDollars(2500.0), Money::fromDollars(500.0));

    Customer c1("CUST-101", "Alice", "Smith", Date(1990, 5, 12), "alice@example.com");
    Customer c2("CUST-102", "Bob", "Smith", Date(1988, 11, 23), "bob@example.com");

    TEST_ASSERT(joint.getOwners().empty(), "Initially no owners");
    joint.addOwner(c1);
    TEST_ASSERT(joint.getOwners().size() == 1, "1 owner added");
    joint.addOwner(c2);
    TEST_ASSERT(joint.getOwners().size() == 2, "2 owners added");

    // Duplicate owner prevention
    joint.addOwner(c1);
    TEST_ASSERT(joint.getOwners().size() == 2, "Duplicate owner must not increase count");

    TEST_ASSERT(joint.getOwners()[0].getFullName() == "Alice Smith", "Owner 1 name check");
    TEST_ASSERT(joint.getOwners()[1].getFullName() == "Bob Smith", "Owner 2 name check");

    // Perform account operations
    joint.deposit(Money::fromDollars(1000.0), "Joint Paycheck");
    joint.withdraw(Money::fromDollars(500.0), "Mortgage Payment");

    TEST_ASSERT(joint.getBalance() == Money::fromDollars(3000.0), "Balance must be $3,000.00");

    // Generate statement
    std::ostringstream oss;
    joint.generateStatement(oss);
    std::string stmt = oss.str();

    TEST_ASSERT(stmt.find("CHK-5001") != std::string::npos, "Statement must contain account number");
    TEST_ASSERT(stmt.find("Checking Account") != std::string::npos, "Statement must contain Checking Account");
    TEST_ASSERT(stmt.find("FDIC Insured: Yes") != std::string::npos, "Statement must contain FDIC Insured: Yes");
    TEST_ASSERT(stmt.find("$3,000.00") != std::string::npos, "Statement must contain balance $3,000.00");
    TEST_ASSERT(stmt.find("Alice Smith") != std::string::npos, "Statement must contain Alice Smith");
    TEST_ASSERT(stmt.find("Bob Smith") != std::string::npos, "Statement must contain Bob Smith");
    TEST_ASSERT(stmt.find("CUST-101") != std::string::npos, "Statement must contain CUST-101");
    TEST_ASSERT(stmt.find("CUST-102") != std::string::npos, "Statement must contain CUST-102");
    TEST_ASSERT(stmt.find("Joint Paycheck") != std::string::npos, "Statement must contain transaction description Joint Paycheck");
    TEST_ASSERT(stmt.find("Mortgage Payment") != std::string::npos, "Statement must contain transaction description Mortgage Payment");
}

// Test 19: Polymorphic IAccount usage and RAII interface compliance
void test_polymorphic_account_hierarchy() {
    std::vector<std::shared_ptr<IAccount>> accounts;
    accounts.push_back(std::make_shared<CheckingAccount>("CHK-6001", Money::fromDollars(1000.0), Money::fromDollars(300.0)));
    accounts.push_back(std::make_shared<SavingsAccount>("SAV-6002", Money::fromDollars(2000.0), 0.04));
    accounts.push_back(std::make_shared<CDAccount>("CD-6003", Money::fromDollars(5000.0), 12, 0.05, Date(2026, 1, 1)));

    Money totalBalance;
    for (const auto& acc : accounts) {
        TEST_ASSERT(!acc->getAccountNumber().empty(), "Account number must not be empty");
        TEST_ASSERT(acc->isFDICInsured(), "All standard accounts must be FDIC insured");

        // Deposit via IAccount interface
        bool dep = acc->deposit(Money::fromDollars(100.0), "Bonus Deposit");
        TEST_ASSERT(dep, "Polymorphic deposit must succeed");

        // Mutex access check: verify getMutex() returns a valid lockable reference
        {
            std::unique_lock<std::mutex> lock(acc->getMutex());
            // Mutex acquired and released without deadlock
        }
        totalBalance += acc->getBalance();

        std::ostringstream oss;
        acc->generateStatement(oss);
        TEST_ASSERT(!oss.str().empty(), "Polymorphic generateStatement output not empty");
    }

    // Checking: 1000 + 100 = 1100. Savings: 2000 + 100 = 2100. CD: 5000 + 100 = 5100. Total = 8300.
    TEST_ASSERT(totalBalance == Money::fromDollars(8300.0), "Total balance sum across polymorphic accounts must equal $8,300.00");
}

// Test 20: StockHolding valuation, cost basis, unrealized P&L, and ROI
void test_stock_holding_calculations_and_roi() {
    StockHolding defaultHolding;
    TEST_ASSERT(defaultHolding.symbol.empty(), "Default symbol empty");
    TEST_ASSERT(defaultHolding.shares == 0, "Default shares is 0");
    TEST_ASSERT(defaultHolding.avgCostPerShare == Money{0}, "Default avg cost is 0");
    TEST_ASSERT(defaultHolding.currentMarketPrice == Money{0}, "Default current market price is 0");
    TEST_ASSERT(defaultHolding.getTotalCostBasis() == Money{0}, "Default total cost basis is 0");
    TEST_ASSERT(defaultHolding.getCurrentMarketValue() == Money{0}, "Default current market value is 0");
    TEST_ASSERT(defaultHolding.getUnrealizedGainLoss() == Money{0}, "Default unrealized P&L is 0");
    TEST_ASSERT(defaultHolding.getReturnOnInvestment() == 0.0, "Default ROI is 0.0");

    // Positive gain scenario: 100 shares @ $50.00 cost, market price $65.00
    StockHolding posHolding("AAPL", 100, Money::fromDollars(50.0), Money::fromDollars(65.0));
    TEST_ASSERT(posHolding.getTotalCostBasis() == Money::fromDollars(5000.0), "Total cost basis $5,000.00");
    TEST_ASSERT(posHolding.getCurrentMarketValue() == Money::fromDollars(6500.0), "Market value $6,500.00");
    TEST_ASSERT(posHolding.getUnrealizedGainLoss() == Money::fromDollars(1500.0), "Unrealized gain $1,500.00");
    TEST_ASSERT(std::abs(posHolding.getReturnOnInvestment() - 30.0) < 1e-6, "ROI must be +30.0%");

    // Negative loss scenario: 100 shares @ $50.00 cost, market price $40.00
    StockHolding negHolding("AAPL", 100, Money::fromDollars(50.0), Money::fromDollars(40.0));
    TEST_ASSERT(negHolding.getTotalCostBasis() == Money::fromDollars(5000.0), "Total cost basis $5,000.00");
    TEST_ASSERT(negHolding.getCurrentMarketValue() == Money::fromDollars(4000.0), "Market value $4,000.00");
    TEST_ASSERT(negHolding.getUnrealizedGainLoss() == Money::fromDollars(-1000.0), "Unrealized loss -$1,000.00");
    TEST_ASSERT(std::abs(negHolding.getReturnOnInvestment() - (-20.0)) < 1e-6, "ROI must be -20.0%");

    // Breakeven scenario
    StockHolding evenHolding("AAPL", 100, Money::fromDollars(50.0), Money::fromDollars(50.0));
    TEST_ASSERT(evenHolding.getUnrealizedGainLoss() == Money{0}, "Unrealized P&L is 0");
    TEST_ASSERT(std::abs(evenHolding.getReturnOnInvestment() - 0.0) < 1e-6, "ROI must be 0.0%");

    // Zero shares with non-zero price
    StockHolding zeroHolding("AAPL", 0, Money::fromDollars(50.0), Money::fromDollars(100.0));
    TEST_ASSERT(zeroHolding.getTotalCostBasis() == Money{0}, "Zero shares basis is 0");
    TEST_ASSERT(zeroHolding.getCurrentMarketValue() == Money{0}, "Zero shares market value is 0");
    TEST_ASSERT(zeroHolding.getReturnOnInvestment() == 0.0, "Zero shares ROI must return 0.0 without division by zero");
}

// Test 21: BondHolding yield and market valuation
void test_bond_holding_calculations_and_yield() {
    BondHolding b1("US912828Z229", "US Treasury", Money::fromDollars(10000.0), 0.045, Date(2034, 5, 15), Money::fromDollars(9800.0));
    TEST_ASSERT(b1.cusip == "US912828Z229", "CUSIP check");
    TEST_ASSERT(b1.issuer == "US Treasury", "Issuer check");
    TEST_ASSERT(b1.faceValue == Money::fromDollars(10000.0), "Face value check");
    TEST_ASSERT(b1.couponRate == 0.045, "Coupon rate check");
    TEST_ASSERT(b1.maturityDate == Date(2034, 5, 15), "Maturity date check");
    TEST_ASSERT(b1.currentMarketPrice == Money::fromDollars(9800.0), "Market price check");

    // Annual yield = faceValue * couponRate = 10,000 * 0.045 = 450.00
    TEST_ASSERT(b1.getAnnualYield() == Money::fromDollars(450.0), "Annual yield must be $450.00");
    TEST_ASSERT(b1.getCurrentMarketValue() == Money::fromDollars(9800.0), "Current market value is market price");

    // Zero coupon bond
    BondHolding bZero("ZERO123", "Corp Zero", Money::fromDollars(5000.0), 0.0, Date(2030, 1, 1), Money::fromDollars(4000.0));
    TEST_ASSERT(bZero.getAnnualYield() == Money{0}, "Zero coupon bond yield is $0.00");
}

// Test 22: InvestmentAccount stock purchase and weighted average cost basis
void test_investment_account_stock_buys_and_cost_averaging() {
    InvestmentAccount inv("INV-1001", Money::fromDollars(10000.0));

    // Verify baseline properties
    TEST_ASSERT(!inv.isFDICInsured(), "Investment account must NOT be FDIC insured");
    TEST_ASSERT(inv.getAccountType() == "Investment Account", "Account type is Investment Account");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(10000.0), "Initial cash is $10,000.00");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(10000.0), "Total portfolio value initially equals cash");
    TEST_ASSERT(inv.getEquitiesValue() == Money{0}, "Initial equities value is 0");
    TEST_ASSERT(inv.getBondsValue() == Money{0}, "Initial bonds value is 0");
    TEST_ASSERT(inv.getStockHoldings().empty(), "No stock holdings initially");

    // Buy 20 shares of AAPL @ $150.00 -> $3,000.00
    bool buy1 = inv.buyStock("AAPL", 20, Money::fromDollars(150.0));
    TEST_ASSERT(buy1, "First stock buy must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(7000.0), "Cash after buy1 is $7,000.00");
    TEST_ASSERT(inv.getEquitiesValue() == Money::fromDollars(3000.0), "Equities value after buy1 is $3,000.00");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(10000.0), "Total portfolio value invariant holds");

    auto holdings1 = inv.getStockHoldings();
    TEST_ASSERT(holdings1.size() == 1, "1 stock position");
    TEST_ASSERT(holdings1[0].symbol == "AAPL", "Symbol is AAPL");
    TEST_ASSERT(holdings1[0].shares == 20, "20 shares owned");
    TEST_ASSERT(holdings1[0].avgCostPerShare == Money::fromDollars(150.0), "Avg cost is $150.00");
    TEST_ASSERT(holdings1[0].currentMarketPrice == Money::fromDollars(150.0), "Current price is $150.00");
    TEST_ASSERT(holdings1[0].getTotalCostBasis() == Money::fromDollars(3000.0), "Cost basis is $3,000.00");

    // Subsequent buy: 10 shares of AAPL @ $180.00 -> $1,800.00
    // Total shares = 30. Total cost = $3,000 + $1,800 = $4,800. Avg cost = 4800 / 30 = $160.00
    bool buy2 = inv.buyStock("AAPL", 10, Money::fromDollars(180.0));
    TEST_ASSERT(buy2, "Second stock buy must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(5200.0), "Cash after buy2 is $5,200.00");

    auto holdings2 = inv.getStockHoldings();
    TEST_ASSERT(holdings2.size() == 1, "Still 1 consolidated position for AAPL");
    TEST_ASSERT(holdings2[0].shares == 30, "30 total shares");
    TEST_ASSERT(holdings2[0].avgCostPerShare == Money::fromDollars(160.0), "Weighted avg cost is $160.00");
    TEST_ASSERT(holdings2[0].currentMarketPrice == Money::fromDollars(180.0), "Current price is $180.00");
    TEST_ASSERT(holdings2[0].getTotalCostBasis() == Money::fromDollars(4800.0), "Cost basis is $4,800.00");
    TEST_ASSERT(holdings2[0].getCurrentMarketValue() == Money::fromDollars(5400.0), "Market value is 30 * $180 = $5,400.00");
    TEST_ASSERT(holdings2[0].getUnrealizedGainLoss() == Money::fromDollars(600.0), "Unrealized gain is $600.00");
    TEST_ASSERT(std::abs(holdings2[0].getReturnOnInvestment() - 12.5) < 1e-6, "ROI is 600/4800 = +12.5%");

    // Verify portfolio total: Cash ($5,200) + Equities ($5,400) = $10,600
    TEST_ASSERT(inv.getEquitiesValue() == Money::fromDollars(5400.0), "Equities value matches market value");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(10600.0), "Total portfolio value reflects market gains");

    // Rejection on insufficient cash: attempt to buy 50 shares @ $200.00 = $10,000 > $5,200 cash
    bool buyFailCash = inv.buyStock("MSFT", 50, Money::fromDollars(200.0));
    TEST_ASSERT(!buyFailCash, "Buy exceeding cash balance must fail");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(5200.0), "Cash must be unchanged");
    TEST_ASSERT(inv.getStockHoldings().size() == 1, "Holdings must be unchanged");

    // Invalid parameters rejection
    TEST_ASSERT(!inv.buyStock("AAPL", 0, Money::fromDollars(100.0)), "Zero shares buy rejected");
    TEST_ASSERT(!inv.buyStock("AAPL", 5, Money::fromDollars(0.0)), "Zero price buy rejected");
    TEST_ASSERT(!inv.buyStock("AAPL", 5, Money::fromDollars(-50.0)), "Negative price buy rejected");
    TEST_ASSERT(!inv.buyStock("", 5, Money::fromDollars(100.0)), "Empty symbol buy rejected");

    // Transaction history verification
    auto history = inv.getTransactionHistory();
    // 1 initial deposit + 2 stock buys = 3 transactions
    TEST_ASSERT(history.size() == 3, "History must have 3 transactions");
    TEST_ASSERT(history[1].type == TransactionType::StockBuy, "Tx 1 is StockBuy");
    TEST_ASSERT(history[1].amount == Money::fromDollars(3000.0), "Tx 1 amount is $3,000.00");
    TEST_ASSERT(history[2].type == TransactionType::StockBuy, "Tx 2 is StockBuy");
    TEST_ASSERT(history[2].amount == Money::fromDollars(1800.0), "Tx 2 amount is $1,800.00");
}

// Test 23: InvestmentAccount partial sales, full sales, and underflow rejection
void test_investment_account_stock_sells_realization_and_underflow_protection() {
    InvestmentAccount inv("INV-1002", Money::fromDollars(10000.0));
    TEST_ASSERT(inv.buyStock("AAPL", 30, Money::fromDollars(160.0)), "Setup: buy 30 AAPL @ $160");
    // Cash: 10000 - 4800 = 5200. AAPL: 30 shares @ avg $160.

    // Partial sell: sell 10 shares @ $200.00 -> $2,000.00 proceeds
    bool sell1 = inv.sellStock("AAPL", 10, Money::fromDollars(200.0));
    TEST_ASSERT(sell1, "Partial stock sell must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(7200.0), "Cash after partial sell is $7,200.00");

    auto holdings = inv.getStockHoldings();
    TEST_ASSERT(holdings.size() == 1, "1 holding remains");
    TEST_ASSERT(holdings[0].shares == 20, "20 shares remain");
    TEST_ASSERT(holdings[0].avgCostPerShare == Money::fromDollars(160.0), "Avg cost basis per share unchanged at $160.00");
    TEST_ASSERT(holdings[0].currentMarketPrice == Money::fromDollars(200.0), "Market price updated to sell price $200.00");
    TEST_ASSERT(holdings[0].getTotalCostBasis() == Money::fromDollars(3200.0), "Remaining cost basis is 20 * $160 = $3,200.00");
    TEST_ASSERT(holdings[0].getCurrentMarketValue() == Money::fromDollars(4000.0), "Market value is 20 * $200 = $4,000.00");
    TEST_ASSERT(inv.getEquitiesValue() == Money::fromDollars(4000.0), "Equities value is $4,000.00");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(11200.0), "Total portfolio value is $7,200 + $4,000 = $11,200.00");

    // Underflow rejection: try to sell 25 shares when only 20 owned
    bool underflow = inv.sellStock("AAPL", 25, Money::fromDollars(200.0));
    TEST_ASSERT(!underflow, "Selling more shares than owned must fail");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(7200.0), "Cash unchanged after failed sell");
    TEST_ASSERT(inv.getStockHoldings()[0].shares == 20, "Shares count unchanged after failed sell");

    // Non-owned ticker rejection
    TEST_ASSERT(!inv.sellStock("GOOG", 5, Money::fromDollars(100.0)), "Selling non-owned stock must fail");

    // Invalid parameters rejection
    TEST_ASSERT(!inv.sellStock("AAPL", 0, Money::fromDollars(200.0)), "Zero shares sell rejected");
    TEST_ASSERT(!inv.sellStock("AAPL", 5, Money::fromDollars(0.0)), "Zero price sell rejected");
    TEST_ASSERT(!inv.sellStock("AAPL", 5, Money::fromDollars(-10.0)), "Negative price sell rejected");
    TEST_ASSERT(!inv.sellStock("", 5, Money::fromDollars(200.0)), "Empty symbol sell rejected");

    // Full sell: sell remaining 20 shares @ $210.00 -> $4,200.00 proceeds
    bool sell2 = inv.sellStock("AAPL", 20, Money::fromDollars(210.0));
    TEST_ASSERT(sell2, "Full stock sell must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(11400.0), "Cash after full liquidation is $11,400.00");
    TEST_ASSERT(inv.getStockHoldings().empty(), "Stock holding completely removed after full sale");
    TEST_ASSERT(inv.getEquitiesValue() == Money{0}, "Equities value is 0");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(11400.0), "Total portfolio value equals cash");

    // Selling again after removal must fail
    TEST_ASSERT(!inv.sellStock("AAPL", 1, Money::fromDollars(210.0)), "Selling after full liquidation must fail");

    // Check transaction history
    auto history = inv.getTransactionHistory();
    // 1 initial deposit + 1 buy + 2 sells = 4 transactions
    TEST_ASSERT(history.size() == 4, "History has 4 transactions");
    TEST_ASSERT(history[2].type == TransactionType::StockSell, "Tx 2 is StockSell");
    TEST_ASSERT(history[2].amount == Money::fromDollars(2000.0), "Tx 2 amount is $2,000.00");
    TEST_ASSERT(history[3].type == TransactionType::StockSell, "Tx 3 is StockSell");
    TEST_ASSERT(history[3].amount == Money::fromDollars(4200.0), "Tx 3 amount is $4,200.00");
}

// Test 24: InvestmentAccount bond acquisition, liquidation, and yield tracking
void test_investment_account_bonds_buy_sell_and_valuation() {
    InvestmentAccount inv("INV-2001", Money::fromDollars(50000.0));

    BondHolding b1("US-T-10Y", "US Treasury", Money::fromDollars(10000.0), 0.045, Date(2036, 1, 1), Money::fromDollars(9500.0));
    BondHolding b2("CORP-5Y", "Apple Inc", Money::fromDollars(5000.0), 0.05, Date(2031, 6, 1), Money::fromDollars(5100.0));

    // Buy bond 1
    bool buyB1 = inv.buyBond(b1);
    TEST_ASSERT(buyB1, "Buy bond 1 must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(40500.0), "Cash deducted for bond 1 ($40,500.00)");
    TEST_ASSERT(inv.getBondsValue() == Money::fromDollars(9500.0), "Bonds value is $9,500.00");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(50000.0), "Portfolio total is $50,000.00");

    // Buy bond 2
    bool buyB2 = inv.buyBond(b2);
    TEST_ASSERT(buyB2, "Buy bond 2 must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(35400.0), "Cash deducted for bond 2 ($35,400.00)");
    TEST_ASSERT(inv.getBondsValue() == Money::fromDollars(14600.0), "Bonds value is 9500 + 5100 = $14,600.00");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(50000.0), "Portfolio total is $50,000.00");

    auto bonds = inv.getBondHoldings();
    TEST_ASSERT(bonds.size() == 2, "2 bonds held");

    // Rejection on insufficient cash: attempt bond costing $40,000 > $35,400 cash
    BondHolding expensive("EXP-1", "Mega Corp", Money::fromDollars(50000.0), 0.07, Date(2040, 1, 1), Money::fromDollars(40000.0));
    TEST_ASSERT(!inv.buyBond(expensive), "Expensive bond rejected on insufficient cash");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(35400.0), "Cash unchanged");

    // Invalid bond rejections
    BondHolding emptyCusip("", "Issuer", Money::fromDollars(1000.0), 0.05, Date(2030, 1, 1), Money::fromDollars(1000.0));
    TEST_ASSERT(!inv.buyBond(emptyCusip), "Empty CUSIP bond rejected");
    BondHolding zeroPrice("ZERO-P", "Issuer", Money::fromDollars(1000.0), 0.05, Date(2030, 1, 1), Money{0});
    TEST_ASSERT(!inv.buyBond(zeroPrice), "Zero price bond rejected");

    // Liquidate bond 1 at profit: sold for $9,800.00
    bool sellB1 = inv.sellBond("US-T-10Y", Money::fromDollars(9800.0));
    TEST_ASSERT(sellB1, "Sell bond 1 must succeed");
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(45200.0), "Cash credited with $9,800.00 ($45,200.00)");
    TEST_ASSERT(inv.getBondsValue() == Money::fromDollars(5100.0), "Bonds value reflects only bond 2 ($5,100.00)");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(50300.0), "Portfolio total reflects $300 profit ($50,300.00)");

    auto bondsAfterSell = inv.getBondHoldings();
    TEST_ASSERT(bondsAfterSell.size() == 1, "Only 1 bond remaining");
    TEST_ASSERT(bondsAfterSell[0].cusip == "CORP-5Y", "Remaining bond is CORP-5Y");

    // Non-existent bond liquidation rejection
    TEST_ASSERT(!inv.sellBond("NON-EXISTENT", Money::fromDollars(1000.0)), "Selling non-existent bond rejected");
    TEST_ASSERT(!inv.sellBond("US-T-10Y", Money::fromDollars(9800.0)), "Selling already liquidated bond rejected");

    // Transaction ledger check
    auto txs = inv.getTransactionHistory();
    // 1 initial deposit + 2 bond buys + 1 bond sell = 4 transactions
    TEST_ASSERT(txs.size() == 4, "History has 4 transactions");
    TEST_ASSERT(txs[1].type == TransactionType::BondBuy, "Tx 1 is BondBuy");
    TEST_ASSERT(txs[2].type == TransactionType::BondBuy, "Tx 2 is BondBuy");
    TEST_ASSERT(txs[3].type == TransactionType::BondSell, "Tx 3 is BondSell");
    TEST_ASSERT(txs[3].amount == Money::fromDollars(9800.0), "Tx 3 amount is $9,800.00");
}

// Test 25: Mark-to-market revaluation, portfolio invariants, and statement generation
void test_investment_account_mark_to_market_and_portfolio_invariants() {
    InvestmentAccount inv("INV-3001", Money::fromDollars(25000.0));

    // Buy equities and bonds
    TEST_ASSERT(inv.buyStock("MSFT", 25, Money::fromDollars(400.0)), "Buy MSFT"); // $10,000
    TEST_ASSERT(inv.buyStock("NVDA", 100, Money::fromDollars(50.0)), "Buy NVDA"); // $5,000
    BondHolding b1("MUNI-101", "City of Austin", Money::fromDollars(5000.0), 0.04, Date(2035, 1, 1), Money::fromDollars(4800.0));
    TEST_ASSERT(inv.buyBond(b1), "Buy Muni Bond"); // $4,800

    // Cash: 25000 - 10000 - 5000 - 4800 = 5200.
    TEST_ASSERT(inv.getBalance() == Money::fromDollars(5200.0), "Cash is $5,200.00");
    TEST_ASSERT(inv.getEquitiesValue() == Money::fromDollars(15000.0), "Equities is $15,000.00");
    TEST_ASSERT(inv.getBondsValue() == Money::fromDollars(4800.0), "Bonds is $4,800.00");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(25000.0), "Total portfolio invariant matches $25,000.00");

    // Mark-to-Market revaluation:
    // MSFT jumps from $400.00 to $450.00 -> 25 * $50 = +$1,250
    // NVDA drops from $50.00 to $45.00 -> 100 * -$5 = -$500
    // Net equities change = +$750
    inv.updateStockPrice("MSFT", Money::fromDollars(450.0));
    inv.updateStockPrice("NVDA", Money::fromDollars(45.0));
    // Updating non-existent stock does nothing and doesn't crash
    inv.updateStockPrice("UNKNOWN", Money::fromDollars(100.0));

    TEST_ASSERT(inv.getBalance() == Money::fromDollars(5200.0), "Cash unchanged by price updates");
    TEST_ASSERT(inv.getEquitiesValue() == Money::fromDollars(15750.0), "Equities value updated to $15,750.00");
    TEST_ASSERT(inv.getBondsValue() == Money::fromDollars(4800.0), "Bonds value unchanged");
    TEST_ASSERT(inv.getTotalPortfolioValue() == Money::fromDollars(25750.0), "Total portfolio value updated to $25,750.00");

    // Check individual holding metrics after price update
    auto holdings = inv.getStockHoldings();
    for (const auto& h : holdings) {
        if (h.symbol == "MSFT") {
            TEST_ASSERT(h.currentMarketPrice == Money::fromDollars(450.0), "MSFT price is $450.00");
            TEST_ASSERT(h.getUnrealizedGainLoss() == Money::fromDollars(1250.0), "MSFT unrealized gain $1,250.00");
            TEST_ASSERT(std::abs(h.getReturnOnInvestment() - 12.5) < 1e-6, "MSFT ROI is +12.5%");
        } else if (h.symbol == "NVDA") {
            TEST_ASSERT(h.currentMarketPrice == Money::fromDollars(45.0), "NVDA price is $45.00");
            TEST_ASSERT(h.getUnrealizedGainLoss() == Money::fromDollars(-500.0), "NVDA unrealized loss -$500.00");
            TEST_ASSERT(std::abs(h.getReturnOnInvestment() - (-10.0)) < 1e-6, "NVDA ROI is -10.0%");
        }
    }

    // Statement output verification
    std::ostringstream oss;
    inv.generateStatement(oss);
    std::string stmt = oss.str();

    TEST_ASSERT(stmt.find("INV-3001") != std::string::npos, "Statement contains account number");
    TEST_ASSERT(stmt.find("Investment Account") != std::string::npos, "Statement contains Investment Account");
    TEST_ASSERT(stmt.find("FDIC Insured: No") != std::string::npos, "Statement contains FDIC Insured: No");
    TEST_ASSERT(stmt.find("$5,200.00") != std::string::npos, "Statement contains cash balance");
    TEST_ASSERT(stmt.find("$15,750.00") != std::string::npos, "Statement contains equities value");
    TEST_ASSERT(stmt.find("$4,800.00") != std::string::npos, "Statement contains bonds value");
    TEST_ASSERT(stmt.find("$25,750.00") != std::string::npos, "Statement contains total portfolio value");
    TEST_ASSERT(stmt.find("MSFT") != std::string::npos, "Statement contains MSFT holding");
    TEST_ASSERT(stmt.find("NVDA") != std::string::npos, "Statement contains NVDA holding");
    TEST_ASSERT(stmt.find("MUNI-101") != std::string::npos, "Statement contains MUNI-101 bond");
}

// Test 26: Concurrent trading and thread safety
void test_investment_account_concurrent_trading_thread_safety() {
    InvestmentAccount inv("INV-CONCUR", Money::fromDollars(500000.0));
    // Pre-populate with base holdings
    TEST_ASSERT(inv.buyStock("TECH", 100, Money::fromDollars(100.0)), "Initial buy TECH");
    TEST_ASSERT(inv.buyStock("BIO", 100, Money::fromDollars(50.0)), "Initial buy BIO");

    constexpr int kIterations = 50;
    std::vector<std::thread> workers;

    // Thread 1: Repeatedly buys TECH
    workers.emplace_back([&]() {
        for (int i = 0; i < kIterations; ++i) {
            inv.buyStock("TECH", 1, Money::fromDollars(100.0));
        }
    });

    // Thread 2: Repeatedly sells TECH
    workers.emplace_back([&]() {
        for (int i = 0; i < kIterations; ++i) {
            inv.sellStock("TECH", 1, Money::fromDollars(100.0));
        }
    });

    // Thread 3: Repeatedly buys BIO
    workers.emplace_back([&]() {
        for (int i = 0; i < kIterations; ++i) {
            inv.buyStock("BIO", 1, Money::fromDollars(50.0));
        }
    });

    // Thread 4: Repeatedly sells BIO
    workers.emplace_back([&]() {
        for (int i = 0; i < kIterations; ++i) {
            inv.sellStock("BIO", 1, Money::fromDollars(50.0));
        }
    });

    // Thread 5: Repeatedly updates prices
    workers.emplace_back([&]() {
        for (int i = 0; i < kIterations; ++i) {
            inv.updateStockPrice("TECH", Money::fromDollars(95.0 + (i % 10)));
            inv.updateStockPrice("BIO", Money::fromDollars(48.0 + (i % 5)));
        }
    });

    // Thread 6: Repeatedly queries portfolio valuations
    workers.emplace_back([&]() {
        for (int i = 0; i < kIterations; ++i) {
            Money total = inv.getTotalPortfolioValue();
            Money cash = inv.getBalance();
            Money eq = inv.getEquitiesValue();
            Money bd = inv.getBondsValue();
            (void)total;
            (void)cash;
            (void)eq;
            (void)bd;
            auto stocks = inv.getStockHoldings();
            auto bonds = inv.getBondHoldings();
            (void)stocks;
            (void)bonds;
        }
    });

    for (auto& t : workers) {
        t.join();
    }

    // Invariant check after all concurrent threads finish:
    // TotalPortfolioValue == Cash + Equities + Bonds
    Money finalTotal = inv.getTotalPortfolioValue();
    Money finalCash = inv.getBalance();
    Money finalEq = inv.getEquitiesValue();
    Money finalBonds = inv.getBondsValue();
    TEST_ASSERT(finalTotal == finalCash + finalEq + finalBonds, "Portfolio valuation invariant must hold after concurrent operations");
}

// ============================================================================
// Bank Engine, Transfers & Concurrency Tests (Task 6)
// ============================================================================

void test_bank_engine_registry_and_factories() {
    BankEngine bank;
    Customer alice("CUST-001", "Alice", "Smith", Date(1985, 4, 12), "alice@example.com");
    Customer bob("CUST-002", "Bob", "Jones", Date(1990, 8, 23), "bob@example.com");
    Customer charlie("CUST-003", "Charlie", "Brown", Date(1978, 11, 5), "charlie@example.com");

    auto chk = bank.createCheckingAccount("CHK-001", alice, Money::fromDollars(500.0), Money::fromDollars(200.0));
    TEST_ASSERT(chk != nullptr, "createCheckingAccount must return a valid pointer");
    TEST_ASSERT(chk->getAccountNumber() == "CHK-001", "Account number mismatch");
    TEST_ASSERT(chk->getBalance() == Money::fromDollars(500.0), "Initial balance mismatch");
    TEST_ASSERT(chk->getOverdraftLimit() == Money::fromDollars(200.0), "Overdraft limit mismatch");
    TEST_ASSERT(chk->getOwners().size() == 1 && chk->getOwners()[0] == alice, "Owner mismatch");

    auto svg = bank.createSavingsAccount("SVG-001", alice, Money::fromDollars(1000.0), 0.05);
    TEST_ASSERT(svg != nullptr, "createSavingsAccount must return a valid pointer");
    TEST_ASSERT(svg->getAccountNumber() == "SVG-001", "Account number mismatch");
    TEST_ASSERT(svg->getBalance() == Money::fromDollars(1000.0), "Initial balance mismatch");
    TEST_ASSERT(svg->getAnnualRate() == 0.05, "Annual rate mismatch");

    auto cd = bank.createCDAccount("CD-001", bob, Money::fromDollars(2000.0), 12, 0.045, Date(2026, 1, 1));
    TEST_ASSERT(cd != nullptr, "createCDAccount must return a valid pointer");
    TEST_ASSERT(cd->getAccountNumber() == "CD-001", "Account number mismatch");
    TEST_ASSERT(cd->getBalance() == Money::fromDollars(2000.0), "Initial balance mismatch");
    TEST_ASSERT(cd->getTermMonths() == 12, "Term months mismatch");
    TEST_ASSERT(cd->getInterestRate() == 0.045, "Interest rate mismatch");
    TEST_ASSERT(cd->getStartDate() == Date(2026, 1, 1), "Start date mismatch");

    auto inv = bank.createInvestmentAccount("INV-001", charlie, Money::fromDollars(5000.0));
    TEST_ASSERT(inv != nullptr, "createInvestmentAccount must return a valid pointer");
    TEST_ASSERT(inv->getAccountNumber() == "INV-001", "Account number mismatch");
    TEST_ASSERT(inv->getBalance() == Money::fromDollars(5000.0), "Initial cash balance mismatch");

    // Registry inspection
    TEST_ASSERT(bank.getAccountCount() == 4, "Account count should be 4");
    TEST_ASSERT(bank.getAccount("CHK-001") == chk, "getAccount must return matching account");
    TEST_ASSERT(bank.getAccount("SVG-001") == svg, "getAccount must return matching account");
    TEST_ASSERT(bank.getAccount("CD-001") == cd, "getAccount must return matching account");
    TEST_ASSERT(bank.getAccount("INV-001") == inv, "getAccount must return matching account");
    TEST_ASSERT(bank.getAccount("NONEXISTENT") == nullptr, "getAccount for non-existent must return nullptr");

    // Customer accounts query
    auto aliceAccs = bank.getAccountsForCustomer("CUST-001");
    TEST_ASSERT(aliceAccs.size() == 2, "Alice should have 2 accounts");

    auto bobAccs = bank.getAccountsForCustomer("CUST-002");
    TEST_ASSERT(bobAccs.size() == 1, "Bob should have 1 account");

    auto charlieAccs = bank.getAccountsForCustomer("CUST-003");
    TEST_ASSERT(charlieAccs.size() == 1, "Charlie should have 1 account");

    auto unknownAccs = bank.getAccountsForCustomer("CUST-999");
    TEST_ASSERT(unknownAccs.empty(), "Unknown customer should have 0 accounts");

    // Joint ownership reflected in getAccountsForCustomer
    chk->addOwner(bob);
    bobAccs = bank.getAccountsForCustomer("CUST-002");
    TEST_ASSERT(bobAccs.size() == 2, "Bob should now have 2 accounts after being added as joint owner");

    // Duplicate account rejection
    auto dupChk = bank.createCheckingAccount("CHK-001", alice, Money::fromDollars(100.0));
    TEST_ASSERT(dupChk == nullptr, "Duplicate account number in createCheckingAccount must return nullptr");

    auto dupSvg = bank.createSavingsAccount("CHK-001", bob, Money::fromDollars(100.0));
    TEST_ASSERT(dupSvg == nullptr, "Duplicate account number in createSavingsAccount must return nullptr");

    auto customAcc = std::make_shared<CheckingAccount>("CHK-001", Money::fromDollars(50.0));
    TEST_ASSERT(!bank.registerAccount(customAcc), "registerAccount with duplicate account number must return false");
    TEST_ASSERT(!bank.registerAccount(nullptr), "registerAccount(nullptr) must return false");

    auto emptyNumAcc = bank.createCheckingAccount("", alice, Money::fromDollars(10.0));
    TEST_ASSERT(emptyNumAcc == nullptr, "createCheckingAccount with empty account number must return nullptr");

    TEST_ASSERT(bank.getAccountCount() == 4, "Account count should remain 4 after duplicate rejections");
}

void test_bank_engine_transfer_success_and_ledger() {
    BankEngine bank;
    Customer alice("CUST-001", "Alice", "Smith", Date(1985, 4, 12), "alice@example.com");
    Customer bob("CUST-002", "Bob", "Jones", Date(1990, 8, 23), "bob@example.com");

    auto chk = bank.createCheckingAccount("CHK-001", alice, Money::fromDollars(1000.0));
    auto svg = bank.createSavingsAccount("SVG-001", bob, Money::fromDollars(500.0));

    // Transfer with memo
    bool ok = bank.transfer("CHK-001", "SVG-001", Money::fromDollars(350.0), "Rent share");
    TEST_ASSERT(ok, "Transfer must succeed");
    TEST_ASSERT(chk->getBalance() == Money::fromDollars(650.0), "Sender balance should be deducted");
    TEST_ASSERT(svg->getBalance() == Money::fromDollars(850.0), "Receiver balance should be credited");

    // Verify transaction logs on sender
    auto chkTx = chk->getTransactionHistory();
    TEST_ASSERT(!chkTx.empty(), "Sender history should not be empty");
    const auto& lastSenderTx = chkTx.back();
    TEST_ASSERT(lastSenderTx.type == TransactionType::TransferOut, "Sender transaction must be TransferOut");
    TEST_ASSERT(lastSenderTx.amount == Money::fromDollars(350.0), "Sender transaction amount mismatch");
    TEST_ASSERT(lastSenderTx.resultingBalance == Money::fromDollars(650.0), "Sender resulting balance mismatch");
    TEST_ASSERT(lastSenderTx.description == "Rent share", "Sender description must match memo");

    // Verify transaction logs on receiver
    auto svgTx = svg->getTransactionHistory();
    TEST_ASSERT(!svgTx.empty(), "Receiver history should not be empty");
    const auto& lastReceiverTx = svgTx.back();
    TEST_ASSERT(lastReceiverTx.type == TransactionType::TransferIn, "Receiver transaction must be TransferIn");
    TEST_ASSERT(lastReceiverTx.amount == Money::fromDollars(350.0), "Receiver transaction amount mismatch");
    TEST_ASSERT(lastReceiverTx.resultingBalance == Money::fromDollars(850.0), "Receiver resulting balance mismatch");
    TEST_ASSERT(lastReceiverTx.description == "Rent share", "Receiver description must match memo");

    // Transfer with default (empty) memo
    bool ok2 = bank.transfer("SVG-001", "CHK-001", Money::fromDollars(50.0));
    TEST_ASSERT(ok2, "Second transfer must succeed");
    TEST_ASSERT(svg->getBalance() == Money::fromDollars(800.0), "Bob balance after reverse transfer");
    TEST_ASSERT(chk->getBalance() == Money::fromDollars(700.0), "Alice balance after reverse transfer");

    chkTx = chk->getTransactionHistory();
    svgTx = svg->getTransactionHistory();
    TEST_ASSERT(svgTx.back().type == TransactionType::TransferOut, "Last Bob tx must be TransferOut");
    TEST_ASSERT(chkTx.back().type == TransactionType::TransferIn, "Last Alice tx must be TransferIn");
}

void test_bank_engine_transfer_insufficient_funds_and_conservation() {
    BankEngine bank;
    Customer alice("CUST-001", "Alice", "Smith", Date(1985, 4, 12), "alice@example.com");
    Customer bob("CUST-002", "Bob", "Jones", Date(1990, 8, 23), "bob@example.com");

    auto svg = bank.createSavingsAccount("SVG-001", alice, Money::fromDollars(100.0));
    auto chk = bank.createCheckingAccount("CHK-001", bob, Money::fromDollars(50.0));

    const size_t svgTxCountBefore = svg->getTransactionHistory().size();
    const size_t chkTxCountBefore = chk->getTransactionHistory().size();

    // Attempt to transfer more than balance from savings account
    bool ok = bank.transfer("SVG-001", "CHK-001", Money::fromDollars(100.01));
    TEST_ASSERT(!ok, "Transfer exceeding balance must fail");
    TEST_ASSERT(svg->getBalance() == Money::fromDollars(100.0), "Sender balance must remain unchanged");
    TEST_ASSERT(chk->getBalance() == Money::fromDollars(50.0), "Receiver balance must remain unchanged");
    TEST_ASSERT(svg->getTransactionHistory().size() == svgTxCountBefore, "No transaction logged on sender upon failure");
    TEST_ASSERT(chk->getTransactionHistory().size() == chkTxCountBefore, "No transaction logged on receiver upon failure");

    // Now test with Checking account that has overdraft protection
    Customer charlie("CUST-003", "Charlie", "Brown", Date(1978, 11, 5), "charlie@example.com");
    auto chkOverdraft = bank.createCheckingAccount("CHK-OD", charlie, Money::fromDollars(50.0), Money::fromDollars(100.0));

    // Transfer $120 ($50 cash + $70 overdraft)
    bool okOd = bank.transfer("CHK-OD", "CHK-001", Money::fromDollars(120.0), "Overdraft transfer");
    TEST_ASSERT(okOd, "Overdraft transfer within limits must succeed");
    TEST_ASSERT(chkOverdraft->getBalance() == Money::fromDollars(-70.0), "Sender balance should be negative $70");
    TEST_ASSERT(chk->getBalance() == Money::fromDollars(170.0), "Receiver balance should be $170");

    // Attempt to transfer $40 more from chkOverdraft (would reach -$110, exceeding -$100 overdraft limit)
    bool okExceed = bank.transfer("CHK-OD", "CHK-001", Money::fromDollars(40.0));
    TEST_ASSERT(!okExceed, "Transfer exceeding overdraft limit must fail");
    TEST_ASSERT(chkOverdraft->getBalance() == Money::fromDollars(-70.0), "Sender balance must not change on failure");
    TEST_ASSERT(chk->getBalance() == Money::fromDollars(170.0), "Receiver balance must not change on failure");
}

void test_bank_engine_transfer_validation_rules() {
    BankEngine bank;
    Customer alice("CUST-001", "Alice", "Smith", Date(1985, 4, 12), "alice@example.com");
    Customer bob("CUST-002", "Bob", "Jones", Date(1990, 8, 23), "bob@example.com");

    auto chk1 = bank.createCheckingAccount("CHK-001", alice, Money::fromDollars(500.0));
    auto chk2 = bank.createCheckingAccount("CHK-002", bob, Money::fromDollars(500.0));

    // Same-account transfer rejection
    TEST_ASSERT(!bank.transfer("CHK-001", "CHK-001", Money::fromDollars(50.0)), "Same account transfer must fail");
    TEST_ASSERT(chk1->getBalance() == Money::fromDollars(500.0), "Balance must remain unchanged");

    // Zero or negative amounts
    TEST_ASSERT(!bank.transfer("CHK-001", "CHK-002", Money{0}), "Transfer of zero amount must fail");
    TEST_ASSERT(!bank.transfer("CHK-001", "CHK-002", Money::fromDollars(-10.0)), "Transfer of negative amount must fail");
    TEST_ASSERT(chk1->getBalance() == Money::fromDollars(500.0), "Sender balance must remain unchanged");
    TEST_ASSERT(chk2->getBalance() == Money::fromDollars(500.0), "Receiver balance must remain unchanged");

    // Unknown account numbers
    TEST_ASSERT(!bank.transfer("CHK-001", "UNKNOWN", Money::fromDollars(10.0)), "Transfer to unknown target must fail");
    TEST_ASSERT(!bank.transfer("UNKNOWN", "CHK-001", Money::fromDollars(10.0)), "Transfer from unknown source must fail");
    TEST_ASSERT(!bank.transfer("GHOST-1", "GHOST-2", Money::fromDollars(10.0)), "Transfer between unknown accounts must fail");
    TEST_ASSERT(chk1->getBalance() == Money::fromDollars(500.0), "Account balance must remain unchanged");
}

void test_bank_engine_analytics_and_statements() {
    BankEngine bank;
    Customer alice("CUST-001", "Alice", "Smith", Date(1985, 4, 12), "alice@example.com");
    Customer bob("CUST-002", "Bob", "Jones", Date(1990, 8, 23), "bob@example.com");

    auto chk = bank.createCheckingAccount("CHK-001", alice, Money::fromDollars(500.0));
    auto svg = bank.createSavingsAccount("SVG-001", alice, Money::fromDollars(1250.0));
    auto inv = bank.createInvestmentAccount("INV-001", bob, Money::fromDollars(2000.0));

    // Total deposits: cash balances aggregate (500 + 1250 + 2000 = 3750)
    TEST_ASSERT(bank.getTotalDeposits() == Money::fromDollars(3750.0), "Initial total deposits mismatch");

    // Investment account buys stock with cash: cash drops, stock increases
    inv->buyStock("AAPL", 5, Money::fromDollars(100.0)); // uses $500 cash
    TEST_ASSERT(inv->getBalance() == Money::fromDollars(1500.0), "Investment cash balance after stock purchase");
    // Total deposits aggregates cash across accounts: 500 + 1250 + 1500 = 3250
    TEST_ASSERT(bank.getTotalDeposits() == Money::fromDollars(3250.0), "Total deposits must reflect cash after stock purchase");

    // Export single statement
    std::ostringstream ossSingle;
    bank.exportStatement("CHK-001", ossSingle);
    const std::string singleStr = ossSingle.str();
    TEST_ASSERT(singleStr.find("CHK-001") != std::string::npos, "Exported statement must contain account number");
    TEST_ASSERT(singleStr.find("Current Balance: $500.00") != std::string::npos, "Exported statement must contain balance");

    // Export single statement for non-existent account must throw
    bool threwNotFound = false;
    try {
        std::ostringstream ossErr;
        bank.exportStatement("NONEXISTENT", ossErr);
    } catch (const std::invalid_argument&) {
        threwNotFound = true;
    }
    TEST_ASSERT(threwNotFound, "exportStatement for non-existent account must throw std::invalid_argument");

    // Export all statements
    std::ostringstream ossAll;
    bank.exportAllStatements(ossAll);
    const std::string allStr = ossAll.str();
    TEST_ASSERT(allStr.find("CHK-001") != std::string::npos, "All statements must contain CHK-001");
    TEST_ASSERT(allStr.find("SVG-001") != std::string::npos, "All statements must contain SVG-001");
    TEST_ASSERT(allStr.find("INV-001") != std::string::npos, "All statements must contain INV-001");
}

void test_bank_engine_concurrent_bidirectional_transfers_stress() {
    BankEngine bank;
    Customer alice("CUST-001", "Alice", "Smith", Date(1985, 4, 12), "alice@example.com");
    Customer bob("CUST-002", "Bob", "Jones", Date(1990, 8, 23), "bob@example.com");

    const Money initialBalA = Money::fromDollars(10000.0);
    const Money initialBalB = Money::fromDollars(10000.0);
    const Money expectedTotal = initialBalA + initialBalB;

    auto accA = bank.createCheckingAccount("ACC-A", alice, initialBalA);
    auto accB = bank.createCheckingAccount("ACC-B", bob, initialBalB);

    constexpr int kNumThreads = 10;
    constexpr int kTransfersPerThread = 1000;

    std::vector<std::thread> workers;
    workers.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < kTransfersPerThread; ++i) {
                // Deterministic pseudorandom amounts between $1.00 and $25.00
                const int dollarAmount = 1 + ((t * 13 + i * 7) % 25);
                const Money amt = Money::fromDollars(static_cast<double>(dollarAmount));

                if ((t + i) % 2 == 0) {
                    bank.transfer("ACC-A", "ACC-B", amt);
                } else {
                    bank.transfer("ACC-B", "ACC-A", amt);
                }

                // Interleaved total deposits query
                if (i % 200 == 0) {
                    Money currentTotal = bank.getTotalDeposits();
                    (void)currentTotal;
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    // Invariant: bal(A) + bal(B) before == bal(A) + bal(B) after
    Money finalBalA = accA->getBalance();
    Money finalBalB = accB->getBalance();
    TEST_ASSERT(finalBalA + finalBalB == expectedTotal, "Total combined balance must be conserved after concurrent transfers");
    TEST_ASSERT(bank.getTotalDeposits() == expectedTotal, "Total bank deposits must equal sum of balances");
}

void test_bank_engine_concurrent_multi_account_network_stress() {
    BankEngine bank;
    Customer cust("CUST-MULTI", "Multi", "User", Date(1990, 1, 1), "multi@example.com");

    constexpr int kNumAccounts = 5;
    const Money perAccount = Money::fromDollars(5000.0);
    const Money expectedTotal = perAccount * static_cast<int64_t>(kNumAccounts);

    std::vector<std::string> accNums;
    for (int i = 0; i < kNumAccounts; ++i) {
        std::string num = "NET-ACC-" + std::to_string(i);
        bank.createCheckingAccount(num, cust, perAccount);
        accNums.push_back(num);
    }

    constexpr int kNumThreads = 10;
    constexpr int kIterations = 500;
    std::vector<std::thread> workers;
    workers.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < kIterations; ++i) {
                int srcIdx = (t + i) % kNumAccounts;
                int dstIdx = (srcIdx + 1 + ((t * 3 + i) % (kNumAccounts - 1))) % kNumAccounts;
                int dollars = 1 + ((t * 17 + i * 11) % 20);
                bank.transfer(accNums[srcIdx], accNums[dstIdx], Money::fromDollars(dollars));
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    TEST_ASSERT(bank.getTotalDeposits() == expectedTotal, "Total bank deposits must be conserved in multi-account network transfers");
}

int main() {
    std::cout << "================================================================================\n";
    std::cout << "                         JBank Core Engine Test Suite                           \n";
    std::cout << "================================================================================\n";

    // Category 1: Fixed-Point Money & Value Object Invariants
    std::cout << "\n================================================================================\n";
    std::cout << "  Category 1: Fixed-Point Money & Value Object Invariants                       \n";
    std::cout << "================================================================================\n";
    RUN_TEST(test_default_construction);
    RUN_TEST(test_from_dollars_and_zero_drift);
    RUN_TEST(test_addition_and_subtraction);
    RUN_TEST(test_multiplication_and_division);
    RUN_TEST(test_division_by_zero_throws);
    RUN_TEST(test_overflow_and_underflow);
    RUN_TEST(test_comparison_operators);
    RUN_TEST(test_string_formatting_and_stream);

    // Category 2: Domain Types & Calendar Date Validations
    std::cout << "\n================================================================================\n";
    std::cout << "  Category 2: Domain Types & Calendar Date Validations                          \n";
    std::cout << "================================================================================\n";
    RUN_TEST(test_date_construction_and_validation);
    RUN_TEST(test_date_formatting_and_comparisons);
    RUN_TEST(test_customer_domain_record);
    RUN_TEST(test_transaction_type_enumeration);
    RUN_TEST(test_transaction_factory_and_records);

    // Category 3: Account Hierarchy, Overdraft, APY Compounding & CD Penalties
    std::cout << "\n================================================================================\n";
    std::cout << "  Category 3: Account Hierarchy, Overdraft, APY Compounding & CD Penalties      \n";
    std::cout << "================================================================================\n";
    RUN_TEST(test_base_account_operations_and_thread_safety);
    RUN_TEST(test_checking_account_and_overdraft);
    RUN_TEST(test_savings_account_and_interest_accrual);
    RUN_TEST(test_cd_account_maturity_and_penalties);
    RUN_TEST(test_multi_owner_joint_account_composition_and_statement);
    RUN_TEST(test_polymorphic_account_hierarchy);

    // Category 4: Wealth Management, Equities & Bonds Portfolio Valuations
    std::cout << "\n================================================================================\n";
    std::cout << "  Category 4: Wealth Management, Equities & Bonds Portfolio Valuations          \n";
    std::cout << "================================================================================\n";
    RUN_TEST(test_stock_holding_calculations_and_roi);
    RUN_TEST(test_bond_holding_calculations_and_yield);
    RUN_TEST(test_investment_account_stock_buys_and_cost_averaging);
    RUN_TEST(test_investment_account_stock_sells_realization_and_underflow_protection);
    RUN_TEST(test_investment_account_bonds_buy_sell_and_valuation);
    RUN_TEST(test_investment_account_mark_to_market_and_portfolio_invariants);
    RUN_TEST(test_investment_account_concurrent_trading_thread_safety);

    // Category 5: Concurrency, Atomic Transfers & Deadlock Avoidance Stress Tests
    std::cout << "\n================================================================================\n";
    std::cout << "  Category 5: Concurrency, Atomic Transfers & Deadlock Avoidance Stress Tests   \n";
    std::cout << "================================================================================\n";
    RUN_TEST(test_bank_engine_registry_and_factories);
    RUN_TEST(test_bank_engine_transfer_success_and_ledger);
    RUN_TEST(test_bank_engine_transfer_insufficient_funds_and_conservation);
    RUN_TEST(test_bank_engine_transfer_validation_rules);
    RUN_TEST(test_bank_engine_analytics_and_statements);
    RUN_TEST(test_bank_engine_concurrent_bidirectional_transfers_stress);
    RUN_TEST(test_bank_engine_concurrent_multi_account_network_stress);

    std::cout << "\n================================================================================\n";
    std::cout << "Summary: " << g_tests_passed << "/" << g_tests_run << " test suites passed (100% pass rate).\n";
    std::cout << "================================================================================\n";

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

