#include "Investment.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

namespace JBank {

InvestmentAccount::InvestmentAccount(std::string accNum, Money initialCash)
    : BaseAccount(std::move(accNum), initialCash, false) {}

std::string InvestmentAccount::getAccountType() const {
    return "Investment Account";
}

Money InvestmentAccount::getEquitiesValueUnlocked() const noexcept {
    int64_t totalCents = 0;
    for (const auto& [_, holding] : m_stocks) {
        totalCents += holding.getCurrentMarketValue().getCents();
    }
    return Money(totalCents);
}

Money InvestmentAccount::getBondsValueUnlocked() const noexcept {
    int64_t totalCents = 0;
    for (const auto& [_, bond] : m_bonds) {
        totalCents += bond.getCurrentMarketValue().getCents();
    }
    return Money(totalCents);
}

Money InvestmentAccount::getTotalPortfolioValueUnlocked() const noexcept {
    return m_balance + getEquitiesValueUnlocked() + getBondsValueUnlocked();
}

Money InvestmentAccount::getEquitiesValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getEquitiesValueUnlocked();
}

Money InvestmentAccount::getBondsValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getBondsValueUnlocked();
}

Money InvestmentAccount::getTotalPortfolioValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getTotalPortfolioValueUnlocked();
}

std::vector<StockHolding> InvestmentAccount::getStockHoldings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<StockHolding> result;
    result.reserve(m_stocks.size());
    for (const auto& [_, holding] : m_stocks) {
        result.push_back(holding);
    }
    return result;
}

std::vector<BondHolding> InvestmentAccount::getBondHoldings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BondHolding> result;
    result.reserve(m_bonds.size());
    for (const auto& [_, bond] : m_bonds) {
        result.push_back(bond);
    }
    return result;
}

bool InvestmentAccount::buyStock(const std::string& symbol, uint32_t shares, Money pricePerShare) {
    if (symbol.empty() || shares == 0 || pricePerShare <= Money{0}) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Compute total cost: shares * pricePerShare
    int64_t costCents = 0;
    try {
        Money totalCost = pricePerShare * static_cast<int64_t>(shares);
        costCents = totalCost.getCents();
    } catch (...) {
        return false;
    }

    const Money totalCost(costCents);
    if (m_balance < totalCost) {
        return false;
    }

    auto it = m_stocks.find(symbol);
    if (it == m_stocks.end()) {
        m_balance -= totalCost;
        m_stocks.emplace(symbol, StockHolding{symbol, shares, pricePerShare, pricePerShare});
    } else {
        StockHolding& holding = it->second;
        const uint64_t totalShares = static_cast<uint64_t>(holding.shares) + static_cast<uint64_t>(shares);
        if (totalShares > std::numeric_limits<uint32_t>::max()) {
            return false;
        }

        m_balance -= totalCost;

        // Weighted average cost basis calculation
        const int64_t oldBasisCents = holding.getTotalCostBasis().getCents();
        const int64_t newBasisCents = totalCost.getCents();
        const double avgCostDouble = static_cast<double>(oldBasisCents + newBasisCents) / static_cast<double>(totalShares);
        const int64_t roundedAvgCents = static_cast<int64_t>(std::round(avgCostDouble));

        holding.shares = static_cast<uint32_t>(totalShares);
        holding.avgCostPerShare = Money(roundedAvgCents);
        holding.currentMarketPrice = pricePerShare;
    }

    m_history.push_back(Transaction::create(
        TransactionType::StockBuy,
        totalCost,
        m_balance,
        "Bought " + std::to_string(shares) + " shares of " + symbol + " @ " + pricePerShare.toString()
    ));

    return true;
}

bool InvestmentAccount::sellStock(const std::string& symbol, uint32_t shares, Money pricePerShare) {
    if (symbol.empty() || shares == 0 || pricePerShare <= Money{0}) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_stocks.find(symbol);
    if (it == m_stocks.end()) {
        return false;
    }

    StockHolding& holding = it->second;
    if (holding.shares < shares) {
        return false;
    }

    int64_t proceedsCents = 0;
    try {
        Money totalProceeds = pricePerShare * static_cast<int64_t>(shares);
        proceedsCents = totalProceeds.getCents();
    } catch (...) {
        return false;
    }

    const Money totalProceeds(proceedsCents);
    m_balance += totalProceeds;

    holding.shares -= shares;
    holding.currentMarketPrice = pricePerShare;

    if (holding.shares == 0) {
        m_stocks.erase(it);
    }

    m_history.push_back(Transaction::create(
        TransactionType::StockSell,
        totalProceeds,
        m_balance,
        "Sold " + std::to_string(shares) + " shares of " + symbol + " @ " + pricePerShare.toString()
    ));

    return true;
}

bool InvestmentAccount::buyBond(const BondHolding& bond) {
    if (bond.cusip.empty() || bond.currentMarketPrice <= Money{0}) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_balance < bond.currentMarketPrice) {
        return false;
    }

    m_balance -= bond.currentMarketPrice;
    m_bonds[bond.cusip] = bond;

    m_history.push_back(Transaction::create(
        TransactionType::BondBuy,
        bond.currentMarketPrice,
        m_balance,
        "Bought bond " + bond.cusip + " (" + bond.issuer + ")"
    ));

    return true;
}

bool InvestmentAccount::sellBond(const std::string& cusip, Money sellPrice) {
    if (cusip.empty() || sellPrice <= Money{0}) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_bonds.find(cusip);
    if (it == m_bonds.end()) {
        return false;
    }

    m_balance += sellPrice;
    const std::string issuer = it->second.issuer;
    m_bonds.erase(it);

    m_history.push_back(Transaction::create(
        TransactionType::BondSell,
        sellPrice,
        m_balance,
        "Sold bond " + cusip + " (" + issuer + ")"
    ));

    return true;
}

void InvestmentAccount::updateStockPrice(const std::string& symbol, Money newPrice) {
    if (symbol.empty() || newPrice < Money{0}) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stocks.find(symbol);
    if (it != m_stocks.end()) {
        it->second.currentMarketPrice = newPrice;
    }
}

void InvestmentAccount::printAccountDetails(std::ostream& out) const {
    out << "Equities Market Value: " << getEquitiesValueUnlocked().toString() << "\n";
    out << "Bonds Market Value:    " << getBondsValueUnlocked().toString() << "\n";
    out << "Total Portfolio Value: " << getTotalPortfolioValueUnlocked().toString() << "\n";

    out << "Stock Holdings (" << m_stocks.size() << "):\n";
    if (m_stocks.empty()) {
        out << "  (No stock positions)\n";
    } else {
        for (const auto& [symbol, stock] : m_stocks) {
            char roiBuf[32];
            std::snprintf(roiBuf, sizeof(roiBuf), "%.2f%%", stock.getReturnOnInvestment());
            out << "  - " << symbol << ": " << stock.shares << " shares @ "
                << stock.avgCostPerShare.toString() << " (Current: "
                << stock.currentMarketPrice.toString() << ", Value: "
                << stock.getCurrentMarketValue().toString() << ", Unrealized P&L: "
                << stock.getUnrealizedGainLoss().toString() << ", ROI: "
                << roiBuf << ")\n";
        }
    }

    out << "Bond Holdings (" << m_bonds.size() << "):\n";
    if (m_bonds.empty()) {
        out << "  (No bond positions)\n";
    } else {
        for (const auto& [cusip, bond] : m_bonds) {
            char rateBuf[32];
            std::snprintf(rateBuf, sizeof(rateBuf), "%.2f%%", bond.couponRate * 100.0);
            out << "  - " << cusip << " (" << bond.issuer << "): Face: "
                << bond.faceValue.toString() << ", Coupon: " << rateBuf
                << ", Maturity: " << bond.maturityDate.toString()
                << ", Market Price: " << bond.currentMarketPrice.toString()
                << ", Annual Yield: " << bond.getAnnualYield().toString() << "\n";
        }
    }
}

void InvestmentAccount::generateStatement(std::ostream& out) const {
    BaseAccount::generateStatement(out);
}

} // namespace JBank
