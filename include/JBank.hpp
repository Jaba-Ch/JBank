#pragma once

#include "Accounts.hpp"
#include "Investment.hpp"
#include "Money.hpp"
#include "Types.hpp"
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace JBank {

// ============================================================================
// BankEngine: Central Banking Manager, Account Registry & Atomic Transfers
// ============================================================================
class BankEngine {
public:
    BankEngine() = default;
    ~BankEngine() = default;

    BankEngine(const BankEngine&) = delete;
    BankEngine& operator=(const BankEngine&) = delete;
    BankEngine(BankEngine&&) = delete;
    BankEngine& operator=(BankEngine&&) = delete;

    // Registry & Factory
    std::shared_ptr<CheckingAccount> createCheckingAccount(
        std::string accNum,
        const Customer& owner,
        Money initialBalance = Money{},
        Money overdraftLimit = Money{});

    std::shared_ptr<SavingsAccount> createSavingsAccount(
        std::string accNum,
        const Customer& owner,
        Money initialBalance = Money{},
        double annualRate = 0.04);

    std::shared_ptr<CDAccount> createCDAccount(
        std::string accNum,
        const Customer& owner,
        Money initialDeposit,
        unsigned int termMonths,
        double rate,
        Date startDate);

    std::shared_ptr<InvestmentAccount> createInvestmentAccount(
        std::string accNum,
        const Customer& owner,
        Money initialCash = Money{});

    bool registerAccount(std::shared_ptr<IAccount> account);

    [[nodiscard]] std::shared_ptr<IAccount> getAccount(const std::string& accNum) const;

    [[nodiscard]] std::vector<std::shared_ptr<IAccount>> getAccountsForCustomer(const std::string& customerId) const;

    [[nodiscard]] size_t getAccountCount() const;

    // Atomic Deadlock-Free Inter-Account Transfers
    bool transfer(
        const std::string& fromAccNum,
        const std::string& toAccNum,
        Money amount,
        const std::string& memo = "");

    // Analytics & Statements
    [[nodiscard]] Money getTotalDeposits() const;

    void exportStatement(const std::string& accNum, std::ostream& out) const;

    void exportAllStatements(std::ostream& out) const;

private:
    mutable std::mutex m_registryMutex;
    std::map<std::string, std::shared_ptr<IAccount>> m_accounts;
};

} // namespace JBank
