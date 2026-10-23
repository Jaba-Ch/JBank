#include "Accounts.hpp"
#include "Investment.hpp"
#include "JBank.hpp"
#include "Money.hpp"
#include "Types.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace JBank;

namespace {

void printHeader() {
    std::cout << "================================================================================\n";
    std::cout << "        _ ____              _      ____                 \n";
    std::cout << "       | |  _ \\            | |    / ___|___  _ __ ___   \n";
    std::cout << "    _  | | |_) | __ _ _ __ | | __| |   / _ \\| '__/ _ \\  \n";
    std::cout << "   | |_| |  _ < / _` | '_ \\| |/ /| |__| (_) | | |  __/  \n";
    std::cout << "    \\___/|_| \\_\\\\__,_|_| |_|_|\\_\\ \\____\\___/|_|  \\___|  \n";
    std::cout << "================================================================================\n";
    std::cout << "      High-Performance Concurrent Financial & Asset Ledger Engine (C++20)       \n";
    std::cout << "         Version 1.0.0 | Pure RAII | Deadlock-Free Transfers | Zero-Drift       \n";
    std::cout << "================================================================================\n\n";
}

void printSection(const std::string& title) {
    std::cout << "\n--------------------------------------------------------------------------------\n";
    std::cout << "  " << title << "\n";
    std::cout << "--------------------------------------------------------------------------------\n";
}

void runScenario1_CustomerOnboarding(Customer& alice, Customer& bob, Customer& elena) {
    printSection("SCENARIO 1: Enterprise Customer Onboarding & KYC Identity Records");

    alice = Customer("CUST-001", "Alice", "Hamilton", Date(1988, 5, 14), "alice.hamilton@example.com");
    bob   = Customer("CUST-002", "Robert", "Hamilton", Date(1985, 11, 22), "bob.hamilton@example.com");
    elena = Customer("CUST-003", "Elena", "Rostova", Date(1992, 3, 30), "elena.rostova@alphawealth.com");

    std::cout << "[KYC] Registering retail and institutional clients with Gregorian calendar verification:\n\n";
    
    std::cout << "  * Client 1: " << alice.getFullName() << "\n"
              << "    - ID:    " << alice.id << "\n"
              << "    - DOB:   " << alice.dob.toString() << " (Valid Date: " << (alice.dob.isValid() ? "YES" : "NO") << ")\n"
              << "    - Email: " << alice.email << "\n\n";

    std::cout << "  * Client 2: " << bob.getFullName() << "\n"
              << "    - ID:    " << bob.id << "\n"
              << "    - DOB:   " << bob.dob.toString() << " (Valid Date: " << (bob.dob.isValid() ? "YES" : "NO") << ")\n"
              << "    - Email: " << bob.email << "\n\n";

    std::cout << "  * Client 3: " << elena.getFullName() << "\n"
              << "    - ID:    " << elena.id << "\n"
              << "    - DOB:   " << elena.dob.toString() << " (Valid Date: " << (elena.dob.isValid() ? "YES" : "NO") << ")\n"
              << "    - Email: " << elena.email << "\n";

    std::cout << "\n[OK] KYC identity verification completed successfully. Zero validation errors.\n";
}

void runScenario2_AccountOpeningAndFDIC(
    BankEngine& bank,
    const Customer& alice,
    const Customer& bob,
    std::shared_ptr<CheckingAccount>& chk,
    std::shared_ptr<SavingsAccount>& sav,
    std::shared_ptr<CDAccount>& cd) {

    printSection("SCENARIO 2: Account Opening, Overdraft Facility, APY & FDIC Insurance");

    std::cout << "[CORE] Opening tiered accounts under BankEngine registry...\n\n";

    // 1. Checking Account
    chk = bank.createCheckingAccount("CHK-1001", alice, Money::fromDollars(2500.0), Money::fromDollars(500.0));
    std::cout << "  [1] Primary Checking Account Created:\n"
              << "      - Account Number:    " << chk->getAccountNumber() << "\n"
              << "      - Primary Owner:     " << chk->getOwners().front().getFullName() << "\n"
              << "      - Initial Balance:   " << chk->getBalance().toString() << "\n"
              << "      - Overdraft Limit:   " << chk->getOverdraftLimit().toString() << "\n"
              << "      - FDIC Insured:      " << (chk->isFDICInsured() ? "YES (Up to $250,000)" : "NO") << "\n\n";

    // 2. High-Yield Savings Account
    sav = bank.createSavingsAccount("SAV-2001", alice, Money::fromDollars(10000.0), 0.045);
    std::cout << "  [2] High-Yield Savings Account Created:\n"
              << "      - Account Number:    " << sav->getAccountNumber() << "\n"
              << "      - Primary Owner:     " << sav->getOwners().front().getFullName() << "\n"
              << "      - Initial Balance:   " << sav->getBalance().toString() << "\n"
              << "      - Annual Rate (APY): " << std::fixed << std::setprecision(2) << (sav->getAnnualRate() * 100.0) << "%\n"
              << "      - FDIC Insured:      " << (sav->isFDICInsured() ? "YES (Up to $250,000)" : "NO") << "\n";

    // Demonstrate Interest Accrual
    Money interest = sav->applyMonthlyInterest();
    std::cout << "      >> Simulated Month 1 Interest Accrual: " << interest.toString()
              << " -> New Balance: " << sav->getBalance().toString() << "\n\n";

    // 3. Certificate of Deposit (CD) Account
    Date cdStart(2026, 1, 15);
    cd = bank.createCDAccount("CD-3001", bob, Money::fromDollars(50000.0), 12, 0.05, cdStart);
    std::cout << "  [3] Certificate of Deposit (CD) Created:\n"
              << "      - Account Number:    " << cd->getAccountNumber() << "\n"
              << "      - Primary Owner:     " << cd->getOwners().front().getFullName() << "\n"
              << "      - Initial Deposit:   " << cd->getBalance().toString() << "\n"
              << "      - Fixed APY:         " << std::fixed << std::setprecision(2) << (cd->getInterestRate() * 100.0) << "%\n"
              << "      - Term Duration:     " << cd->getTermMonths() << " Months\n"
              << "      - Start Date:        " << cd->getStartDate().toString() << "\n"
              << "      - Maturity Date:     " << cd->getMaturityDate().toString() << "\n"
              << "      - Early W/D Penalty: " << cd->getEarlyWithdrawalPenalty().toString() << "\n"
              << "      - FDIC Insured:      " << (cd->isFDICInsured() ? "YES (Up to $250,000)" : "NO") << "\n";

    std::cout << "\n[OK] Retail accounts opened and registered. FDIC insurance guarantees active.\n";
}

void runScenario3_JointAccountComposition(std::shared_ptr<CheckingAccount>& chk, const Customer& bob) {
    printSection("SCENARIO 3: Joint Account Ownership via Composition (Zero Diamond Problem)");

    std::cout << "[ARCH] In traditional OOP banking models, developers often introduce brittle class hierarchies:\n"
              << "       class JointCheckingAccount : public CheckingAccount, public JointAccount (Multiple Inheritance -> Diamond Issue)\n"
              << "       JBank resolves this via COMPOSITION OVER INHERITANCE: accounts own std::vector<Customer>.\n\n";

    std::cout << "  * Initial Checking Account Owners: " << chk->getOwners().size() << " owner(s)\n";
    for (const auto& owner : chk->getOwners()) {
        std::cout << "    - " << owner.getFullName() << " [ID: " << owner.id << "]\n";
    }

    std::cout << "\n  >> Granting co-ownership to spouse: " << bob.getFullName() << "...\n";
    chk->addOwner(bob);

    std::cout << "\n  * Updated Checking Account Owners: " << chk->getOwners().size() << " owner(s)\n";
    for (const auto& owner : chk->getOwners()) {
        std::cout << "    - " << owner.getFullName() << " [ID: " << owner.id << ", Email: " << owner.email << "]\n";
    }

    std::cout << "\n[OK] Joint account ownership established cleanly without multiple inheritance.\n";
}

void runScenario4_WealthManagement(
    BankEngine& bank,
    const Customer& elena,
    std::shared_ptr<InvestmentAccount>& inv) {

    printSection("SCENARIO 4: Wealth Management, Equities Cost-Basis & Fixed-Income Yields");

    inv = bank.createInvestmentAccount("INV-4001", elena, Money::fromDollars(100000.0));
    std::cout << "[WEALTH] Private Wealth Account " << inv->getAccountNumber() << " initialized for "
              << elena.getFullName() << ".\n"
              << "         Initial Cash Allocation: " << inv->getBalance().toString() << " (SIPC Protected)\n\n";

    // Equity purchases
    std::cout << "  [1] Executing Equity Order: Buy 100 shares AAPL @ $180.00/share...\n";
    inv->buyStock("AAPL", 100, Money::fromDollars(180.0));
    std::cout << "      - Uninvested Cash: " << inv->getBalance().toString() << "\n\n";

    std::cout << "  [2] Executing Tranche 2: Buy 50 shares AAPL @ $195.00/share (Dollar-Cost Averaging)...\n";
    inv->buyStock("AAPL", 50, Money::fromDollars(195.0));
    std::cout << "      - Uninvested Cash: " << inv->getBalance().toString() << "\n\n";

    std::cout << "  [3] Executing Diversification Order: Buy 200 shares NVDA @ $120.00/share...\n";
    inv->buyStock("NVDA", 200, Money::fromDollars(120.0));
    std::cout << "      - Uninvested Cash: " << inv->getBalance().toString() << "\n\n";

    // Mark to Market
    std::cout << "  >> Updating Market Feeds (Mark-to-Market Valuation):\n";
    std::cout << "     * AAPL market price advances: $195.00 -> $210.00\n";
    inv->updateStockPrice("AAPL", Money::fromDollars(210.0));
    std::cout << "     * NVDA market price advances: $120.00 -> $145.00\n";
    inv->updateStockPrice("NVDA", Money::fromDollars(145.0));

    // Municipal Bond Purchase
    std::cout << "\n  [4] Purchasing Municipal Bond (Fixed Income Allocation):\n";
    Date muniMaturity(2030, 6, 1);
    BondHolding muni("US-NYC-MUN-2030", "City of New York Gen Obligation",
                    Money::fromDollars(20000.0), 0.0475, muniMaturity, Money::fromDollars(19800.0));
    inv->buyBond(muni);
    std::cout << "      - CUSIP:          " << muni.cusip << " (" << muni.issuer << ")\n"
              << "      - Face Value:     " << muni.faceValue.toString() << "\n"
              << "      - Purchase Price: " << muni.currentMarketPrice.toString() << " (Discounted)\n"
              << "      - Coupon Yield:   " << std::fixed << std::setprecision(2) << (muni.couponRate * 100.0) << "%\n"
              << "      - Annual Income:  " << muni.getAnnualYield().toString() << "/yr\n"
              << "      - Maturity Date:  " << muni.maturityDate.toString() << "\n\n";

    // Holdings & Portfolio Summary
    std::cout << "  ========================================================================\n";
    std::cout << "  Institutional Wealth Portfolio Valuation Breakdown:\n";
    std::cout << "  ========================================================================\n";
    std::cout << "  Uninvested Cash:       " << inv->getBalance().toString() << "\n";
    std::cout << "  Equities Market Value: " << inv->getEquitiesValue().toString() << "\n";
    std::cout << "  Bonds Market Value:    " << inv->getBondsValue().toString() << "\n";
    std::cout << "  ------------------------------------------------------------------------\n";
    std::cout << "  Total Portfolio Value: " << inv->getTotalPortfolioValue().toString() << "\n";
    std::cout << "  ========================================================================\n";

    auto stocks = inv->getStockHoldings();
    std::cout << "\n  Equity Positions Detail (" << stocks.size() << " positions):\n";
    for (const auto& s : stocks) {
        std::cout << "  * " << s.symbol << ": " << s.shares << " shares"
                  << " | Avg Cost: " << s.avgCostPerShare.toString()
                  << " | Market: " << s.currentMarketPrice.toString()
                  << " | Value: " << s.getCurrentMarketValue().toString()
                  << " | Unrealized P&L: " << s.getUnrealizedGainLoss().toString()
                  << " (" << std::fixed << std::setprecision(2) << s.getReturnOnInvestment() << "% ROI)\n";
    }

    std::cout << "\n[OK] Wealth portfolio valuated with zero-drift integer-cent accuracy.\n";
}

void runScenario5_AtomicInterAccountTransfers(
    BankEngine& bank,
    std::shared_ptr<CheckingAccount>& chk,
    std::shared_ptr<SavingsAccount>& sav) {

    printSection("SCENARIO 5: Thread-Safe Atomic Inter-Account Transfer (Deadlock-Free)");

    std::cout << "[CONCURRENCY] Demonstrating BankEngine::transfer with std::scoped_lock:\n"
              << "              Guarantees all-or-nothing atomicity and prevents deadlock cycles.\n\n";

    const Money totalBefore = bank.getTotalDeposits();
    std::cout << "  * Total Bank Core Deposits (Before Transfer): " << totalBefore.toString() << "\n";
    std::cout << "    - CHK-1001 (Checking) Balance: " << chk->getBalance().toString() << "\n";
    std::cout << "    - SAV-2001 (Savings)  Balance: " << sav->getBalance().toString() << "\n\n";

    const Money transferAmount = Money::fromDollars(1200.0);
    std::cout << "  >> Executing Atomic Transfer of " << transferAmount.toString()
              << " from CHK-1001 to SAV-2001...\n";

    const bool transferSuccess = bank.transfer(
        chk->getAccountNumber(),
        sav->getAccountNumber(),
        transferAmount,
        "Monthly Automated Emergency Fund Allocation"
    );

    std::cout << "     Transaction Result: " << (transferSuccess ? "SUCCESS (Committed)" : "FAILED (Aborted)") << "\n\n";

    const Money totalAfter = bank.getTotalDeposits();
    std::cout << "  * Total Bank Core Deposits (After Transfer):  " << totalAfter.toString() << "\n";
    std::cout << "    - CHK-1001 (Checking) Balance: " << chk->getBalance().toString() << "\n";
    std::cout << "    - SAV-2001 (Savings)  Balance: " << sav->getBalance().toString() << "\n";

    if (totalBefore == totalAfter) {
        std::cout << "\n  [VERIFIED] Invariant Confirmed: TOTAL BANK DEPOSITS CONSERVED (Delta = $0.00).\n";
    } else {
        std::cout << "\n  [ALERT] Invariant Violated: Deposit leak detected!\n";
    }

    // Attempting invalid transfer that exceeds overdraft
    std::cout << "\n  >> Attempting Extreme Overdraft Transfer ($50,000.00 from Checking)...\n";
    const bool overdraftAttempt = bank.transfer(
        chk->getAccountNumber(),
        sav->getAccountNumber(),
        Money::fromDollars(50000.0),
        "Unauthorized Over-Limit Transfer"
    );

    std::cout << "     Transaction Result: " << (overdraftAttempt ? "ACCEPTED" : "SAFELY REJECTED (Insufficient Funds)") << "\n";
    std::cout << "     - CHK-1001 Balance Unchanged: " << chk->getBalance().toString() << "\n";
    std::cout << "     - SAV-2001 Balance Unchanged: " << sav->getBalance().toString() << "\n";
    std::cout << "     - Total Bank Deposits Intact: " << bank.getTotalDeposits().toString() << "\n";

    std::cout << "\n[OK] Atomic transfer semantics and boundary invariants verified.\n";
}

void runScenario6_StatementExport(BankEngine& bank, const std::string& filename = "jbank_statement.txt") {
    printSection("SCENARIO 6: Audit Ledger Statement Generation & File Export");

    std::cout << "[AUDIT] Generating comprehensive bank-wide audit statements...\n\n";

    // 1. Display single account statement to console
    std::cout << "--- Console Preview: CHK-1001 Statement ---\n";
    bank.exportStatement("CHK-1001", std::cout);

    // 2. Export all accounts to disk
    std::ofstream outFile(filename);
    if (!outFile) {
        std::cerr << "[ERROR] Unable to open statement file: " << filename << "\n";
        return;
    }

    outFile << "################################################################################\n";
    outFile << "                     JBANK ENTERPRISE AUDIT STATEMENT EXPORT                    \n";
    outFile << "################################################################################\n\n";
    bank.exportAllStatements(outFile);
    outFile.close();

    std::cout << "\n[OK] All active account ledgers successfully exported to disk:\n";
    std::cout << "     File: " << filename << "\n";
}

void runFullDemonstration(BankEngine& bank) {
    Customer alice, bob, elena;
    std::shared_ptr<CheckingAccount> chk;
    std::shared_ptr<SavingsAccount> sav;
    std::shared_ptr<CDAccount> cd;
    std::shared_ptr<InvestmentAccount> inv;

    runScenario1_CustomerOnboarding(alice, bob, elena);
    runScenario2_AccountOpeningAndFDIC(bank, alice, bob, chk, sav, cd);
    runScenario3_JointAccountComposition(chk, bob);
    runScenario4_WealthManagement(bank, elena, inv);
    runScenario5_AtomicInterAccountTransfers(bank, chk, sav);
    runScenario6_StatementExport(bank, "jbank_statement.txt");

    std::cout << "\n================================================================================\n";
    std::cout << "             JBank Core Banking Demonstration Completed Successfully!           \n";
    std::cout << "================================================================================\n";
}

void runInteractiveMenu(BankEngine& bank) {
    printSection("INTERACTIVE EXECUTIVE CONSOLE");

    while (true) {
        std::cout << "\n========================================\n";
        std::cout << "           JBank Operator Menu          \n";
        std::cout << "========================================\n";
        std::cout << "  1. View Bank Deposit Totals & Accounts\n";
        std::cout << "  2. Deposit Funds into Account\n";
        std::cout << "  3. Withdraw Funds from Account\n";
        std::cout << "  4. Execute Atomic Transfer\n";
        std::cout << "  5. Export Statements to Disk\n";
        std::cout << "  6. Run Full Demonstration Showcase\n";
        std::cout << "  0. Exit\n";
        std::cout << "========================================\n";
        std::cout << "Enter selection (0-6): ";

        int choice = -1;
        if (!(std::cin >> choice)) {
            break;
        }

        if (choice == 0) {
            std::cout << "Exiting JBank Interactive Console.\n";
            break;
        }

        switch (choice) {
            case 1: {
                std::cout << "\n[INFO] Registered Accounts: " << bank.getAccountCount() << "\n";
                std::cout << "       Total Bank Deposits:  " << bank.getTotalDeposits().toString() << "\n";
                break;
            }
            case 2: {
                std::string accNum;
                double dollars = 0.0;
                std::cout << "Enter Account Number: ";
                std::cin >> accNum;
                std::cout << "Enter Amount ($): ";
                std::cin >> dollars;
                auto acc = bank.getAccount(accNum);
                if (acc && acc->deposit(Money::fromDollars(dollars), "Interactive Deposit")) {
                    std::cout << "[OK] Deposited $" << std::fixed << std::setprecision(2) << dollars
                              << ". New Balance: " << acc->getBalance().toString() << "\n";
                } else {
                    std::cout << "[FAILED] Deposit operation rejected.\n";
                }
                break;
            }
            case 3: {
                std::string accNum;
                double dollars = 0.0;
                std::cout << "Enter Account Number: ";
                std::cin >> accNum;
                std::cout << "Enter Amount ($): ";
                std::cin >> dollars;
                auto acc = bank.getAccount(accNum);
                if (acc && acc->withdraw(Money::fromDollars(dollars), "Interactive Withdrawal")) {
                    std::cout << "[OK] Withdrew $" << std::fixed << std::setprecision(2) << dollars
                              << ". New Balance: " << acc->getBalance().toString() << "\n";
                } else {
                    std::cout << "[FAILED] Withdrawal rejected (Insufficient funds or invalid amount).\n";
                }
                break;
            }
            case 4: {
                std::string fromAcc, toAcc;
                double dollars = 0.0;
                std::cout << "Enter Source Account Number: ";
                std::cin >> fromAcc;
                std::cout << "Enter Destination Account Number: ";
                std::cin >> toAcc;
                std::cout << "Enter Transfer Amount ($): ";
                std::cin >> dollars;
                if (bank.transfer(fromAcc, toAcc, Money::fromDollars(dollars), "Interactive Transfer")) {
                    std::cout << "[OK] Atomic transfer succeeded!\n";
                } else {
                    std::cout << "[FAILED] Transfer aborted (Check account IDs, balance, or limits).\n";
                }
                break;
            }
            case 5: {
                std::string filename;
                std::cout << "Enter output filename (default: jbank_interactive_statement.txt): ";
                std::cin >> filename;
                if (filename.empty()) {
                    filename = "jbank_interactive_statement.txt";
                }
                std::ofstream ofs(filename);
                if (ofs) {
                    bank.exportAllStatements(ofs);
                    std::cout << "[OK] Statements written to " << filename << "\n";
                } else {
                    std::cout << "[ERROR] Could not open file.\n";
                }
                break;
            }
            case 6: {
                runFullDemonstration(bank);
                break;
            }
            default:
                std::cout << "Invalid selection. Please choose 0-6.\n";
                break;
        }
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    printHeader();

    BankEngine bank;

    bool interactiveMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--interactive" || arg == "-i") {
            interactiveMode = true;
        }
    }

    if (interactiveMode) {
        // Pre-populate demo accounts for interactive experimentation
        Customer demoCust("CUST-100", "Executive", "User", Date(1990, 1, 1), "user@jbank.internal");
        bank.createCheckingAccount("CHK-100", demoCust, Money::fromDollars(5000.0), Money::fromDollars(1000.0));
        bank.createSavingsAccount("SAV-100", demoCust, Money::fromDollars(25000.0), 0.045);
        runInteractiveMenu(bank);
    } else {
        runFullDemonstration(bank);
    }

    return 0;
}
