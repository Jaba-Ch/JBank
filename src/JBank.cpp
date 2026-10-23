#include "JBank.hpp"
#include <stdexcept>
#include <utility>

namespace JBank {

std::shared_ptr<CheckingAccount> BankEngine::createCheckingAccount(
    std::string accNum,
    const Customer& owner,
    Money initialBalance,
    Money overdraftLimit) {
    if (accNum.empty()) {
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        if (m_accounts.find(accNum) != m_accounts.end()) {
            return nullptr;
        }
    }
    auto acc = std::make_shared<CheckingAccount>(accNum, initialBalance, overdraftLimit);
    acc->addOwner(owner);
    if (!registerAccount(acc)) {
        return nullptr;
    }
    return acc;
}

std::shared_ptr<SavingsAccount> BankEngine::createSavingsAccount(
    std::string accNum,
    const Customer& owner,
    Money initialBalance,
    double annualRate) {
    if (accNum.empty()) {
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        if (m_accounts.find(accNum) != m_accounts.end()) {
            return nullptr;
        }
    }
    auto acc = std::make_shared<SavingsAccount>(accNum, initialBalance, annualRate);
    acc->addOwner(owner);
    if (!registerAccount(acc)) {
        return nullptr;
    }
    return acc;
}

std::shared_ptr<CDAccount> BankEngine::createCDAccount(
    std::string accNum,
    const Customer& owner,
    Money initialDeposit,
    unsigned int termMonths,
    double rate,
    Date startDate) {
    if (accNum.empty()) {
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        if (m_accounts.find(accNum) != m_accounts.end()) {
            return nullptr;
        }
    }
    auto acc = std::make_shared<CDAccount>(accNum, initialDeposit, termMonths, rate, startDate);
    acc->addOwner(owner);
    if (!registerAccount(acc)) {
        return nullptr;
    }
    return acc;
}

std::shared_ptr<InvestmentAccount> BankEngine::createInvestmentAccount(
    std::string accNum,
    const Customer& owner,
    Money initialCash) {
    if (accNum.empty()) {
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        if (m_accounts.find(accNum) != m_accounts.end()) {
            return nullptr;
        }
    }
    auto acc = std::make_shared<InvestmentAccount>(accNum, initialCash);
    acc->addOwner(owner);
    if (!registerAccount(acc)) {
        return nullptr;
    }
    return acc;
}

bool BankEngine::registerAccount(std::shared_ptr<IAccount> account) {
    if (!account) {
        return false;
    }
    const std::string& accNum = account->getAccountNumber();
    if (accNum.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_registryMutex);
    if (m_accounts.find(accNum) != m_accounts.end()) {
        return false;
    }
    m_accounts[accNum] = std::move(account);
    return true;
}

std::shared_ptr<IAccount> BankEngine::getAccount(const std::string& accNum) const {
    std::lock_guard<std::mutex> lock(m_registryMutex);
    auto it = m_accounts.find(accNum);
    if (it != m_accounts.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<IAccount>> BankEngine::getAccountsForCustomer(const std::string& customerId) const {
    std::vector<std::shared_ptr<IAccount>> result;
    if (customerId.empty()) {
        return result;
    }
    std::vector<std::shared_ptr<IAccount>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        snapshot.reserve(m_accounts.size());
        for (const auto& [_, acc] : m_accounts) {
            snapshot.push_back(acc);
        }
    }
    for (const auto& acc : snapshot) {
        const auto owners = acc->getOwners();
        for (const auto& owner : owners) {
            if (owner.id == customerId) {
                result.push_back(acc);
                break;
            }
        }
    }
    return result;
}

size_t BankEngine::getAccountCount() const {
    std::lock_guard<std::mutex> lock(m_registryMutex);
    return m_accounts.size();
}

bool BankEngine::transfer(
    const std::string& fromAccNum,
    const std::string& toAccNum,
    Money amount,
    const std::string& memo) {
    if (fromAccNum == toAccNum) {
        return false;
    }
    if (amount <= Money{0}) {
        return false;
    }

    std::shared_ptr<IAccount> fromAcc;
    std::shared_ptr<IAccount> toAcc;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        auto itFrom = m_accounts.find(fromAccNum);
        auto itTo = m_accounts.find(toAccNum);
        if (itFrom == m_accounts.end() || itTo == m_accounts.end()) {
            return false;
        }
        fromAcc = itFrom->second;
        toAcc = itTo->second;
    }

    if (!fromAcc || !toAcc || fromAcc == toAcc) {
        return false;
    }

    std::scoped_lock lock(fromAcc->getMutex(), toAcc->getMutex());

    const std::string outMemo = memo.empty() ? ("Transfer to " + toAccNum) : memo;
    const std::string inMemo = memo.empty() ? ("Transfer from " + fromAccNum) : memo;

    if (!fromAcc->withdrawUnlocked(amount, outMemo, TransactionType::TransferOut)) {
        return false;
    }

    if (!toAcc->depositUnlocked(amount, inMemo, TransactionType::TransferIn)) {
        // Roll back withdrawal if deposit fails (conservation of money)
        fromAcc->depositUnlocked(amount, "Transfer Rollback", TransactionType::TransferIn);
        return false;
    }

    return true;
}

Money BankEngine::getTotalDeposits() const {
    std::vector<std::shared_ptr<IAccount>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        snapshot.reserve(m_accounts.size());
        for (const auto& [_, acc] : m_accounts) {
            snapshot.push_back(acc);
        }
    }

    Money total;
    for (const auto& acc : snapshot) {
        total += acc->getBalance();
    }
    return total;
}

void BankEngine::exportStatement(const std::string& accNum, std::ostream& out) const {
    auto acc = getAccount(accNum);
    if (!acc) {
        throw std::invalid_argument("Account not found: " + accNum);
    }
    acc->generateStatement(out);
}

void BankEngine::exportAllStatements(std::ostream& out) const {
    std::vector<std::shared_ptr<IAccount>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_registryMutex);
        snapshot.reserve(m_accounts.size());
        for (const auto& [_, acc] : m_accounts) {
            snapshot.push_back(acc);
        }
    }
    for (const auto& acc : snapshot) {
        acc->generateStatement(out);
    }
}

} // namespace JBank
