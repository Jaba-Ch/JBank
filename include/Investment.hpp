#pragma once

#include "Accounts.hpp"
#include "Money.hpp"
#include "Types.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace JBank {

// ============================================================================
// StockHolding: Equity Asset Record
// ============================================================================
struct StockHolding {
    std::string symbol;
    uint32_t shares{0};
    Money avgCostPerShare;
    Money currentMarketPrice;

    StockHolding() = default;
    StockHolding(std::string sym, uint32_t sh, Money avgCost, Money curPrice = Money{})
        : symbol(std::move(sym)),
          shares(sh),
          avgCostPerShare(avgCost),
          currentMarketPrice(curPrice) {}

    [[nodiscard]] Money getTotalCostBasis() const noexcept {
        try {
            return avgCostPerShare * static_cast<int64_t>(shares);
        } catch (...) {
            return Money{0};
        }
    }

    [[nodiscard]] Money getCurrentMarketValue() const noexcept {
        try {
            return currentMarketPrice * static_cast<int64_t>(shares);
        } catch (...) {
            return Money{0};
        }
    }

    [[nodiscard]] Money getUnrealizedGainLoss() const noexcept {
        try {
            return getCurrentMarketValue() - getTotalCostBasis();
        } catch (...) {
            return Money{0};
        }
    }

    [[nodiscard]] double getReturnOnInvestment() const noexcept {
        const int64_t basisCents = getTotalCostBasis().getCents();
        if (basisCents == 0) {
            return 0.0;
        }
        const int64_t gainLossCents = getUnrealizedGainLoss().getCents();
        return (static_cast<double>(gainLossCents) / static_cast<double>(basisCents)) * 100.0;
    }

    auto operator<=>(const StockHolding& other) const = default;
    bool operator==(const StockHolding& other) const = default;
};

// ============================================================================
// BondHolding: Fixed Income Asset Record
// ============================================================================
struct BondHolding {
    std::string cusip;
    std::string issuer;
    Money faceValue;
    double couponRate{0.0};
    Date maturityDate;
    Money currentMarketPrice;

    BondHolding() = default;
    BondHolding(std::string c, std::string iss, Money fv, double cr, Date md, Money cmp)
        : cusip(std::move(c)),
          issuer(std::move(iss)),
          faceValue(fv),
          couponRate(cr),
          maturityDate(md),
          currentMarketPrice(cmp) {}

    [[nodiscard]] Money getAnnualYield() const noexcept {
        try {
            return faceValue * couponRate;
        } catch (...) {
            return Money{0};
        }
    }

    [[nodiscard]] Money getCurrentMarketValue() const noexcept {
        return currentMarketPrice;
    }

    auto operator<=>(const BondHolding& other) const = default;
    bool operator==(const BondHolding& other) const = default;
};

// ============================================================================
// InvestmentAccount: Wealth Management & Brokerage Portfolio Account
// ============================================================================
class InvestmentAccount : public BaseAccount {
public:
    explicit InvestmentAccount(std::string accNum, Money initialCash = Money{});
    ~InvestmentAccount() override = default;

    bool buyStock(const std::string& symbol, uint32_t shares, Money pricePerShare);
    bool sellStock(const std::string& symbol, uint32_t shares, Money pricePerShare);
    bool buyBond(const BondHolding& bond);
    bool sellBond(const std::string& cusip, Money sellPrice);
    void updateStockPrice(const std::string& symbol, Money newPrice);

    [[nodiscard]] Money getTotalPortfolioValue() const;
    [[nodiscard]] Money getEquitiesValue() const;
    [[nodiscard]] Money getBondsValue() const;

    [[nodiscard]] std::vector<StockHolding> getStockHoldings() const;
    [[nodiscard]] std::vector<BondHolding> getBondHoldings() const;

    [[nodiscard]] std::string getAccountType() const override;
    void generateStatement(std::ostream& out) const override;

protected:
    void printAccountDetails(std::ostream& out) const override;

private:
    [[nodiscard]] Money getEquitiesValueUnlocked() const noexcept;
    [[nodiscard]] Money getBondsValueUnlocked() const noexcept;
    [[nodiscard]] Money getTotalPortfolioValueUnlocked() const noexcept;

    std::map<std::string, StockHolding> m_stocks;
    std::map<std::string, BondHolding> m_bonds;
};

} // namespace JBank
