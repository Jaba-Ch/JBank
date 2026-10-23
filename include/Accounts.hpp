#pragma once

#include "Money.hpp"
#include "Types.hpp"
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace JBank {

// ============================================================================
// IAccount: Pure Abstract Account Interface
// ============================================================================
class IAccount {
public:
    virtual ~IAccount() = default;

    [[nodiscard]] virtual const std::string& getAccountNumber() const noexcept = 0;
    [[nodiscard]] virtual Money getBalance() const noexcept = 0;
    [[nodiscard]] virtual bool isFDICInsured() const noexcept = 0;
    virtual bool deposit(Money amount, const std::string& memo = "") = 0;
    virtual bool withdraw(Money amount, const std::string& memo = "") = 0;
    virtual bool depositUnlocked(Money amount, const std::string& memo = "", TransactionType type = TransactionType::Deposit) = 0;
    virtual bool withdrawUnlocked(Money amount, const std::string& memo = "", TransactionType type = TransactionType::Withdrawal) = 0;
    [[nodiscard]] virtual std::vector<Transaction> getTransactionHistory() const = 0;
    virtual void generateStatement(std::ostream& out) const = 0;
    [[nodiscard]] virtual std::mutex& getMutex() const noexcept = 0;
    [[nodiscard]] virtual std::vector<Customer> getOwners() const = 0;
    virtual void addOwner(const Customer& owner) = 0;
};

// ============================================================================
// BaseAccount: Thread-Safe Base Account Implementation
// ============================================================================
class BaseAccount : public IAccount {
public:
    BaseAccount(std::string accNum, Money initialBalance = Money{}, bool isFDIC = true);
    ~BaseAccount() override = default;

    BaseAccount(const BaseAccount&) = delete;
    BaseAccount& operator=(const BaseAccount&) = delete;
    BaseAccount(BaseAccount&&) = delete;
    BaseAccount& operator=(BaseAccount&&) = delete;

    [[nodiscard]] const std::string& getAccountNumber() const noexcept override;
    [[nodiscard]] Money getBalance() const noexcept override;
    [[nodiscard]] bool isFDICInsured() const noexcept override;
    [[nodiscard]] std::mutex& getMutex() const noexcept override;
    [[nodiscard]] std::vector<Customer> getOwners() const override;
    void addOwner(const Customer& owner) override;

    [[nodiscard]] std::vector<Transaction> getTransactionHistory() const override;
    bool deposit(Money amount, const std::string& memo = "") override;
    bool withdraw(Money amount, const std::string& memo = "") override;
    bool depositUnlocked(Money amount, const std::string& memo = "", TransactionType type = TransactionType::Deposit) override;
    bool withdrawUnlocked(Money amount, const std::string& memo = "", TransactionType type = TransactionType::Withdrawal) override;
    void generateStatement(std::ostream& out) const override;

    [[nodiscard]] virtual std::string getAccountType() const;

protected:
    virtual void printAccountDetails(std::ostream& out) const;

    mutable std::mutex m_mutex;
    std::string m_accNum;
    Money m_balance;
    bool m_isFDIC{true};
    std::vector<Customer> m_owners;
    std::vector<Transaction> m_history;
};

// ============================================================================
// CheckingAccount: Transaction Account with Overdraft Facility
// ============================================================================
class CheckingAccount : public BaseAccount {
public:
    CheckingAccount(std::string accNum, Money initialBalance = Money{}, Money overdraftLimit = Money{});
    ~CheckingAccount() override = default;

    [[nodiscard]] Money getOverdraftLimit() const noexcept;
    bool withdraw(Money amount, const std::string& memo = "") override;
    bool withdrawUnlocked(Money amount, const std::string& memo = "", TransactionType type = TransactionType::Withdrawal) override;
    [[nodiscard]] std::string getAccountType() const override;

protected:
    void printAccountDetails(std::ostream& out) const override;

private:
    Money m_overdraftLimit;
};

// ============================================================================
// SavingsAccount: Interest-Bearing Account with Compound Growth
// ============================================================================
class SavingsAccount : public BaseAccount {
public:
    SavingsAccount(std::string accNum, Money initialBalance = Money{}, double annualRate = 0.04);
    ~SavingsAccount() override = default;

    [[nodiscard]] double getAnnualRate() const noexcept;
    void setAnnualRate(double rate) noexcept;

    bool withdraw(Money amount, const std::string& memo = "") override;
    Money applyMonthlyInterest();
    [[nodiscard]] std::string getAccountType() const override;

protected:
    void printAccountDetails(std::ostream& out) const override;

private:
    double m_annualRate{0.04};
};

// ============================================================================
// CDAccount: Certificate of Deposit with Term & Early Withdrawal Penalty
// ============================================================================
class CDAccount : public BaseAccount {
public:
    CDAccount(std::string accNum,
              Money initialDeposit,
              unsigned int termMonths,
              double interestRate,
              Date startDate,
              Money earlyWithdrawalPenalty = Money::fromDollars(50.0));
    ~CDAccount() override = default;

    [[nodiscard]] unsigned int getTermMonths() const noexcept;
    [[nodiscard]] double getInterestRate() const noexcept;
    [[nodiscard]] Date getStartDate() const noexcept;
    [[nodiscard]] Date getMaturityDate() const noexcept;
    [[nodiscard]] Money getEarlyWithdrawalPenalty() const noexcept;

    [[nodiscard]] bool isMatured(Date checkDate) const noexcept;
    [[nodiscard]] bool isMatured() const;
    void setSimulatedDate(Date checkDate);

    bool withdraw(Money amount, const std::string& memo = "") override;
    bool withdrawUnlocked(Money amount, const std::string& memo = "", TransactionType type = TransactionType::Withdrawal) override;
    [[nodiscard]] std::string getAccountType() const override;

protected:
    void printAccountDetails(std::ostream& out) const override;

private:
    unsigned int m_termMonths{0};
    double m_interestRate{0.0};
    Date m_startDate;
    Date m_maturityDate;
    Money m_penalty;
    std::optional<Date> m_simulatedDate;
};

} // namespace JBank
