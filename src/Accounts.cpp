#include "Accounts.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <utility>

namespace JBank {

namespace {

[[nodiscard]] int getMaxDaysInMonth(int year, int month) noexcept {
    switch (month) {
        case 4: case 6: case 9: case 11:
            return 30;
        case 2: {
            const bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            return isLeap ? 29 : 28;
        }
        default:
            return 31;
    }
}

[[nodiscard]] Date calculateMaturityDate(Date startDate, unsigned int termMonths) noexcept {
    const int totalMonths = (startDate.year * 12) + (startDate.month - 1) + static_cast<int>(termMonths);
    const int newYear = totalMonths / 12;
    const int newMonth = (totalMonths % 12) + 1;
    const int maxDay = getMaxDaysInMonth(newYear, newMonth);
    const int newDay = (startDate.day > maxDay) ? maxDay : startDate.day;
    return Date(newYear, newMonth, newDay);
}

[[nodiscard]] Date getSystemDate() noexcept {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    return Date(tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
}

} // anonymous namespace

// ============================================================================
// BaseAccount Implementation
// ============================================================================

BaseAccount::BaseAccount(std::string accNum, Money initialBalance, bool isFDIC)
    : m_accNum(std::move(accNum)),
      m_balance(initialBalance),
      m_isFDIC(isFDIC) {
    if (initialBalance < Money{0}) {
        throw std::invalid_argument("Initial balance cannot be negative");
    }
    if (initialBalance > Money{0}) {
        m_history.push_back(Transaction::create(
            TransactionType::Deposit,
            initialBalance,
            m_balance,
            "Initial Deposit"
        ));
    }
}

const std::string& BaseAccount::getAccountNumber() const noexcept {
    return m_accNum;
}

Money BaseAccount::getBalance() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_balance;
}

bool BaseAccount::isFDICInsured() const noexcept {
    return m_isFDIC;
}

std::mutex& BaseAccount::getMutex() const noexcept {
    return m_mutex;
}

std::vector<Customer> BaseAccount::getOwners() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_owners;
}

void BaseAccount::addOwner(const Customer& owner) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& existing : m_owners) {
        if ((!existing.id.empty() && existing.id == owner.id) || existing == owner) {
            return;
        }
    }
    m_owners.push_back(owner);
}

std::vector<Transaction> BaseAccount::getTransactionHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

bool BaseAccount::deposit(Money amount, const std::string& memo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return depositUnlocked(amount, memo, TransactionType::Deposit);
}

bool BaseAccount::depositUnlocked(Money amount, const std::string& memo, TransactionType type) {
    if (amount <= Money{0}) {
        return false;
    }
    m_balance += amount;
    m_history.push_back(Transaction::create(
        type,
        amount,
        m_balance,
        memo.empty() ? std::string(to_string(type)) : memo
    ));
    return true;
}

bool BaseAccount::withdraw(Money amount, const std::string& memo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return withdrawUnlocked(amount, memo, TransactionType::Withdrawal);
}

bool BaseAccount::withdrawUnlocked(Money amount, const std::string& memo, TransactionType type) {
    if (amount <= Money{0} || m_balance < amount) {
        return false;
    }
    m_balance -= amount;
    m_history.push_back(Transaction::create(
        type,
        amount,
        m_balance,
        memo.empty() ? std::string(to_string(type)) : memo
    ));
    return true;
}

std::string BaseAccount::getAccountType() const {
    return "Base Account";
}

void BaseAccount::printAccountDetails(std::ostream& /*out*/) const {
    // Default base implementation outputs no subclass-specific details
}

void BaseAccount::generateStatement(std::ostream& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    out << "====================================================\n";
    out << "  JBank Account Statement\n";
    out << "====================================================\n";
    out << "Account Number: " << m_accNum << "\n";
    out << "Account Type: " << getAccountType() << "\n";
    out << "FDIC Insured: " << (m_isFDIC ? "Yes" : "No") << "\n";
    out << "Current Balance: " << m_balance.toString() << "\n";
    printAccountDetails(out);
    out << "Authorized Owners (" << m_owners.size() << "):\n";
    if (m_owners.empty()) {
        out << "  (No owners registered)\n";
    } else {
        for (const auto& owner : m_owners) {
            out << "  - " << owner.getFullName() << " [ID: " << owner.id
                << ", Email: " << owner.email << "]\n";
        }
    }
    out << "----------------------------------------------------\n";
    out << "Transaction History (" << m_history.size() << " transactions):\n";
    if (m_history.empty()) {
        out << "  (No transactions recorded)\n";
    } else {
        for (const auto& tx : m_history) {
            out << "  " << tx.toString() << "\n";
        }
    }
    out << "====================================================\n";
}

// ============================================================================
// CheckingAccount Implementation
// ============================================================================

CheckingAccount::CheckingAccount(std::string accNum, Money initialBalance, Money overdraftLimit)
    : BaseAccount(std::move(accNum), initialBalance, true),
      m_overdraftLimit(overdraftLimit) {
    if (overdraftLimit < Money{0}) {
        throw std::invalid_argument("Overdraft limit cannot be negative");
    }
}

Money CheckingAccount::getOverdraftLimit() const noexcept {
    return m_overdraftLimit;
}

bool CheckingAccount::withdraw(Money amount, const std::string& memo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return withdrawUnlocked(amount, memo, TransactionType::Withdrawal);
}

bool CheckingAccount::withdrawUnlocked(Money amount, const std::string& memo, TransactionType type) {
    if (amount <= Money{0}) {
        return false;
    }

    const int64_t balanceCents = m_balance.getCents();
    const int64_t amountCents = amount.getCents();
    const int64_t limitCents = m_overdraftLimit.getCents();

    int64_t maxAllowed = 0;
    if (limitCents > 0 && balanceCents > std::numeric_limits<int64_t>::max() - limitCents) {
        maxAllowed = std::numeric_limits<int64_t>::max();
    } else {
        maxAllowed = balanceCents + limitCents;
    }

    if (amountCents > maxAllowed) {
        return false;
    }

    m_balance -= amount;
    m_history.push_back(Transaction::create(
        type,
        amount,
        m_balance,
        memo.empty() ? std::string(to_string(type)) : memo
    ));
    return true;
}

std::string CheckingAccount::getAccountType() const {
    return "Checking Account";
}

void CheckingAccount::printAccountDetails(std::ostream& out) const {
    out << "Overdraft Limit: " << m_overdraftLimit.toString() << "\n";
}

// ============================================================================
// SavingsAccount Implementation
// ============================================================================

SavingsAccount::SavingsAccount(std::string accNum, Money initialBalance, double annualRate)
    : BaseAccount(std::move(accNum), initialBalance, true),
      m_annualRate(annualRate) {
    if (annualRate < 0.0) {
        throw std::invalid_argument("Annual interest rate cannot be negative");
    }
}

double SavingsAccount::getAnnualRate() const noexcept {
    return m_annualRate;
}

void SavingsAccount::setAnnualRate(double rate) noexcept {
    if (rate >= 0.0) {
        m_annualRate = rate;
    }
}

bool SavingsAccount::withdraw(Money amount, const std::string& memo) {
    return BaseAccount::withdraw(amount, memo);
}

Money SavingsAccount::applyMonthlyInterest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_balance <= Money{0} || m_annualRate <= 0.0) {
        return Money{0};
    }

    const double interestDouble = static_cast<double>(m_balance.getCents()) * (m_annualRate / 12.0);
    const int64_t interestCents = static_cast<int64_t>(std::round(interestDouble));
    if (interestCents <= 0) {
        return Money{0};
    }

    const Money interest(interestCents);
    m_balance += interest;
    m_history.push_back(Transaction::create(
        TransactionType::Interest,
        interest,
        m_balance,
        "Monthly Interest Accrual"
    ));
    return interest;
}

std::string SavingsAccount::getAccountType() const {
    return "Savings Account";
}

void SavingsAccount::printAccountDetails(std::ostream& out) const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f%%", m_annualRate * 100.0);
    out << "Annual Rate:     " << buf << "\n";
}

// ============================================================================
// CDAccount Implementation
// ============================================================================

CDAccount::CDAccount(std::string accNum,
                     Money initialDeposit,
                     unsigned int termMonths,
                     double interestRate,
                     Date startDate,
                     Money earlyWithdrawalPenalty)
    : BaseAccount(std::move(accNum), initialDeposit, true),
      m_termMonths(termMonths),
      m_interestRate(interestRate),
      m_startDate(startDate),
      m_maturityDate(calculateMaturityDate(startDate, termMonths)),
      m_penalty(earlyWithdrawalPenalty) {
    if (termMonths == 0) {
        throw std::invalid_argument("Term months must be positive");
    }
    if (interestRate < 0.0) {
        throw std::invalid_argument("Interest rate cannot be negative");
    }
    if (earlyWithdrawalPenalty < Money{0}) {
        throw std::invalid_argument("Early withdrawal penalty cannot be negative");
    }
}

unsigned int CDAccount::getTermMonths() const noexcept {
    return m_termMonths;
}

double CDAccount::getInterestRate() const noexcept {
    return m_interestRate;
}

Date CDAccount::getStartDate() const noexcept {
    return m_startDate;
}

Date CDAccount::getMaturityDate() const noexcept {
    return m_maturityDate;
}

Money CDAccount::getEarlyWithdrawalPenalty() const noexcept {
    return m_penalty;
}

bool CDAccount::isMatured(Date checkDate) const noexcept {
    return checkDate >= m_maturityDate;
}

bool CDAccount::isMatured() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const Date effectiveDate = m_simulatedDate.value_or(getSystemDate());
    return isMatured(effectiveDate);
}

void CDAccount::setSimulatedDate(Date checkDate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_simulatedDate = checkDate;
}

bool CDAccount::withdraw(Money amount, const std::string& memo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return withdrawUnlocked(amount, memo, TransactionType::Withdrawal);
}

bool CDAccount::withdrawUnlocked(Money amount, const std::string& memo, TransactionType type) {
    if (amount <= Money{0}) {
        return false;
    }

    const Date effectiveDate = m_simulatedDate.value_or(getSystemDate());
    const bool matured = isMatured(effectiveDate);

    if (!matured) {
        if (m_balance < amount || m_balance - amount < m_penalty) {
            return false;
        }

        m_balance -= amount;
        m_history.push_back(Transaction::create(
            type,
            amount,
            m_balance,
            memo.empty() ? (type == TransactionType::Withdrawal ? "Early Withdrawal" : std::string(to_string(type))) : memo
        ));

        if (m_penalty > Money{0}) {
            m_balance -= m_penalty;
            m_history.push_back(Transaction::create(
                TransactionType::Fee,
                m_penalty,
                m_balance,
                "Early Withdrawal Penalty"
            ));
        }
        return true;
    } else {
        if (m_balance < amount) {
            return false;
        }
        m_balance -= amount;
        m_history.push_back(Transaction::create(
            type,
            amount,
            m_balance,
            memo.empty() ? std::string(to_string(type)) : memo
        ));
        return true;
    }
}

std::string CDAccount::getAccountType() const {
    return "Certificate of Deposit (CD)";
}

void CDAccount::printAccountDetails(std::ostream& out) const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f%%", m_interestRate * 100.0);
    out << "Term Months:     " << m_termMonths << "\n";
    out << "Start Date:      " << m_startDate.toString() << "\n";
    out << "Maturity Date:   " << m_maturityDate.toString() << "\n";
    out << "Interest Rate:   " << buf << "\n";
    out << "Early Penalty:   " << m_penalty.toString() << "\n";
}

} // namespace JBank
