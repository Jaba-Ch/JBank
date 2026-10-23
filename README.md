# JBank

### High-Performance Concurrent Financial & Asset Ledger Engine in Modern C++20

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Build System](https://img.shields.io/badge/CMake-3.16%2B-064F8C.svg?style=flat-square&logo=cmake)](https://cmake.org/)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=flat-square)]()
[![Tests](https://img.shields.io/badge/tests-33%2F33%20passed-success.svg?style=flat-square)]()
[![License: All Rights Reserved](https://img.shields.io/badge/License-All%20Rights%20Reserved-red.svg?style=flat-square)](LICENSE)

---

## Overview

**JBank** is a lightweight, high-performance core banking, asset ledger, and wealth management engine written in **Modern C++20**. It is engineered to demonstrate the architectural principles, mathematical precision, and concurrency guarantees required in production financial infrastructure.

Unlike conventional banking implementations that rely on IEEE-754 floating-point primitives (`float` / `double`) and deep inheritance trees, JBank is designed around three core engineering priorities:

1. **Numerical Correctness**: A 64-bit fixed-point integer-cent value object (`Money`) eliminates decimal rounding drift and provides checked arithmetic overflow protection.
2. **Deadlock-Free Concurrency**: Multi-account atomic transfers employ C++20 `std::scoped_lock` with per-account lock striping, mathematically preventing Coffman circular waits while preserving system-wide balance conservation invariants.
3. **Clean Compositional Design**: Multi-owner joint accounts are implemented through dynamic object composition (`std::vector<Customer>`) rather than multiple inheritance, completely eliminating C++ diamond inheritance ambiguities.

The engine relies strictly on the C++20 Standard Library and CMake, with **zero external third-party dependencies** and clean compilation with **zero warnings** under `-Wall -Wextra -Wpedantic` and `/W4 /permissive-`.

---

## System Architecture

JBank uses a decoupled, layered design separating immutable mathematical value types, pure polymorphic account interfaces, domain records, and a thread-safe central ledger engine.

### Class & Component Topology

```mermaid
classDiagram
    direction TB

    class Money {
        -int64_t m_cents
        +fromDollars(double) Money
        +getCents() int64_t
        +toDollars() double
        +operator+(Money) Money
        +operator-(Money) Money
        +operator*(double) Money
        +operator/(int64_t) Money
        +operator<=>(Money)
    }

    class Date {
        +int year
        +int month
        +int day
        +isValid() bool
        +toString() string
        +operator<=>(Date)
    }

    class Customer {
        +string id
        +string firstName
        +string lastName
        +Date dob
        +string email
        +getFullName() string
    }

    class Transaction {
        +string id
        +time_point timestamp
        +TransactionType type
        +Money amount
        +Money resultingBalance
        +string description
        +create() Transaction
    }

    class IAccount {
        <<interface>>
        +getAccountNumber()* string
        +getBalance()* Money
        +isFDICInsured()* bool
        +deposit(Money, string)* bool
        +withdraw(Money, string)* bool
        +depositUnlocked(Money, string, TransactionType)* bool
        +withdrawUnlocked(Money, string, TransactionType)* bool
        +getTransactionHistory()* vector~Transaction~
        +generateStatement(ostream)* void
        +getMutex()* mutex&
        +getOwners()* vector~Customer~
        +addOwner(Customer)* void
    }

    class BaseAccount {
        #mutex m_mutex
        #string m_accNum
        #Money m_balance
        #bool m_isFDIC
        #vector~Customer~ m_owners
        #vector~Transaction~ m_history
        +getAccountNumber() string
        +getBalance() Money
        +isFDICInsured() bool
        +getMutex() mutex&
        +getOwners() vector~Customer~
        +addOwner(Customer) void
        +deposit(Money, string) bool
        +withdraw(Money, string) bool
        +generateStatement(ostream) void
    }

    class CheckingAccount {
        -Money m_overdraftLimit
        +getOverdraftLimit() Money
        +withdraw(Money, string) bool
        +withdrawUnlocked(Money, string, TransactionType) bool
    }

    class SavingsAccount {
        -double m_annualRate
        +getAnnualRate() double
        +setAnnualRate(double) void
        +applyMonthlyInterest() Money
        +withdraw(Money, string) bool
    }

    class CDAccount {
        -unsigned int m_termMonths
        -double m_interestRate
        -Date m_startDate
        -Date m_maturityDate
        -Money m_penalty
        +isMatured(Date) bool
        +getEarlyWithdrawalPenalty() Money
        +withdraw(Money, string) bool
    }

    class InvestmentAccount {
        -map~string, StockHolding~ m_stocks
        -map~string, BondHolding~ m_bonds
        +buyStock(string, uint32_t, Money) bool
        +sellStock(string, uint32_t, Money) bool
        +buyBond(BondHolding) bool
        +sellBond(string, Money) bool
        +updateStockPrice(string, Money) void
        +getTotalPortfolioValue() Money
        +getEquitiesValue() Money
        +getBondsValue() Money
    }

    class BankEngine {
        -mutex m_registryMutex
        -map~string, shared_ptr~IAccount~~ m_accounts
        +createCheckingAccount() shared_ptr~CheckingAccount~
        +createSavingsAccount() shared_ptr~SavingsAccount~
        +createCDAccount() shared_ptr~CDAccount~
        +createInvestmentAccount() shared_ptr~InvestmentAccount~
        +registerAccount(shared_ptr~IAccount~) bool
        +getAccount(string) shared_ptr~IAccount~
        +getAccountsForCustomer(string) vector~shared_ptr~IAccount~~
        +transfer(string, string, Money, string) bool
        +getTotalDeposits() Money
        +exportStatement(string, ostream) void
        +exportAllStatements(ostream) void
    }

    IAccount <|.. BaseAccount : implements
    BaseAccount <|-- CheckingAccount : inherits
    BaseAccount <|-- SavingsAccount : inherits
    BaseAccount <|-- CDAccount : inherits
    BaseAccount <|-- InvestmentAccount : inherits

    BaseAccount o-- Customer : compositions (m_owners)
    BaseAccount *-- Transaction : audit ledger (m_history)
    BaseAccount *-- Money : balance
    BankEngine o-- IAccount : registry & orchestration
    InvestmentAccount *-- Money : valuation
```

### Architectural Flow

```
+====================================================================================+
|                                 APPLICATION LAYER                                  |
|         Interactive CLI (main.cpp)    |    Automated Test Harness (33 Suites)      |
+====================================================================================+
                                          |
                                          v
+====================================================================================+
|                     BANK ENGINE & TRANSACTION ORCHESTRATION                        |
|  - Thread-Safe Registry: std::map<string, std::shared_ptr<IAccount>>               |
|  - Deadlock-Free Transfers: std::scoped_lock on source and destination mutexes     |
|  - System Invariant Verification: All-or-nothing rollback on partial failure       |
|  - Audit Export: Disk and stream serialization of double-entry transaction history|
+====================================================================================+
                                          |
                                          v
+====================================================================================+
|                            ACCOUNT DOMAIN HIERARCHY                                |
|                             <<interface>> IAccount                                 |
|                                       ^                                            |
|                                       |                                            |
|                                  BaseAccount                                       |
|                   +-------------------+-------------------+                        |
|                   |                   |                   |                        |
|            CheckingAccount     SavingsAccount         CDAccount                    |
|           (Overdraft Limit)   (Monthly APY Comp)   (Maturity & Penalty)            |
|                                       |                                            |
|                                       +-------------------+                        |
|                                                           |                        |
|                                                   InvestmentAccount                |
|                                                (Equities, Bonds, MTM)              |
+====================================================================================+
         |                                       |                      |
         v                                       v                      v
+-----------------------+              +-------------------+   +--------------------+
| DOMAIN COMPOSITION    |              | ASSET VALUATION   |   | IMMUTABLE VALUE    |
| - Customer Record     |              | - StockHolding    |   |   OBJECTS & MATH   |
|   (KYC, DOB, Email)   |              |   (Avg Cost, ROI) |   | - Money (64-bit)   |
| - Transaction Record  |              | - BondHolding     |   | - Date (Gregorian) |
|   (Immutable, Timed)  |              |   (Coupon, Yield) |   | - TransactionType  |
+-----------------------+              +-------------------+   +--------------------+
```

---

## Technical Deep Dive

### 1. Fixed-Point Currency Engine (`Money`)

Standard binary floating-point types (`float`, `double`) cannot precisely represent base-10 fractional numbers due to IEEE-754 binary encoding:

```cpp
// IEEE-754 precision defect:
double balance = 0.10 + 0.20; // balance == 0.30000000000000004
```

In high-volume ledger transactions, fractional discrepancies accumulate rapidly, resulting in ledger corruption.

JBank solves this with a dedicated, zero-drift `Money` value object:
- **Underlying Storage**: Signed 64-bit integer cents (`int64_t`), representing exact currency amounts from $-\$92,233,720,368,547,758.07$ to $+\$92,233,720,368,547,758.07$.
- **Half-Away-From-Zero Rounding**: Conversion from external floating-point inputs uses explicit half-away-from-zero rounding (`std::round(dollars * 100.0)`).
- **Checked Arithmetic**: All arithmetic operators (`+`, `-`, `*`, `/`) detect potential integer overflow or underflow prior to calculation and throw `std::overflow_error`, preventing undefined behavior. Division by zero throws `std::invalid_argument`.
- **Register-Efficient**: Encapsulates a single `int64_t`, allowing it to pass in a CPU register by value.

### 2. Concurrency Model & Deadlock-Free Transfers

A classic concurrency flaw in banking systems is the transfer circular wait condition:

```
Thread A: transfer(Account 1 -> Account 2)  [Locks Acc 1, waits for Acc 2]
Thread B: transfer(Account 2 -> Account 1)  [Locks Acc 2, waits for Acc 1]
==> DEADLOCK!
```

JBank eliminates circular wait through two design choices:

1. **Simultaneous Dual Mutex Acquisition**:
   The engine executes transfers using C++20 `std::scoped_lock`:
   ```cpp
   std::scoped_lock lock(fromAcc->getMutex(), toAcc->getMutex());
   ```
   `std::scoped_lock` uses a deadlock-avoidance algorithm (sorting pointer addresses or using a non-blocking `try_lock` back-off protocol), satisfying Coffman's condition for deadlock freedom regardless of transfer direction.

2. **Atomic Rollback & Conservation Invariant**:
   If an initial debit succeeds but the subsequent credit fails (e.g., due to an account rule or internal condition), an automated rollback deposit is immediately executed on the source account. The system balance invariant is preserved:
   $$\sum_{i=1}^{N} \text{Balance}_i(t_0) = \sum_{i=1}^{N} \text{Balance}_i(t_{\text{final}})$$

3. **Fine-Grained Lock Striping**:
   Rather than locking the entire registry during transfers, each account owns a `mutable std::mutex`. Independent account transactions run fully in parallel, preventing global lock contention.

### 3. Composition Over Inheritance (No Diamond Problem)

Modeling joint accounts through multiple inheritance is a common anti-pattern in C++:

```cpp
// Fragile Anti-Pattern: Causes duplicate subobjects, vtable bloat, and diamond ambiguity
class JointCheckingAccount : public CheckingAccount, public JointAccount { ... };
```

This pattern causes a combinatorial explosion of subclasses (`JointChecking`, `JointSavings`, `JointCD`, `JointInvestment`) and introduces virtual inheritance overhead.

JBank models multi-owner accounts through **composition**:
- `BaseAccount` contains a `std::vector<Customer> m_owners`.
- Any account type can dynamically add authorized signers at runtime:
  ```cpp
  account->addOwner(customer);
  ```
- No changes to the class hierarchy are required, and the diamond problem is completely avoided.

### 4. Multi-Asset Wealth Management

The `InvestmentAccount` extends `BaseAccount` into capital markets asset management:
- **Dollar-Cost Averaging**: Maintains a weighted-average cost basis across multiple tranches:
  $$\text{Average Cost Basis} = \frac{\sum (Q_i \times P_i)}{\sum Q_i}$$
- **Mark-to-Market Valuation**: Updates market feeds and computes live unrealized P&L:
  $$\text{Unrealized P/L} = (\text{Shares} \times P_{\text{market}}) - (\text{Shares} \times P_{\text{avg}})$$
- **Fixed-Income Holdings**: Tracks municipal bond coupon yield schedules and maturity dates.
- **Regulatory Boundaries**: Distinguishes between retail FDIC coverage (depository accounts up to \$250,000) and SIPC market-risk coverage (brokerage assets).

### 5. Modern C++20 Standards

- **Spaceship Operator (`operator<=>`)**: Implemented across `Money`, `Date`, `Customer`, `Transaction`, and holdings to generate consistent, strongly-ordered comparison operators with zero boilerplate.
- **Compile-Time `constexpr`**: Evaluates default constructors, numeric extractors, transaction string mappings, and calendar validation at compile time, enabling `static_assert` verification in tests.
- **Attributes & Safety**:
  - `[[nodiscard]]` prevents silently ignored transaction and query return values.
  - `noexcept` specifications on move semantics and query inspectors support compiler optimizations and efficient vector reallocations.
  - Structured bindings (`auto [symbol, holding] : m_stocks`) provide clean, expressive map iterations.

---

## File Structure

```
JBank/
├── CMakeLists.txt              # Standard CMake 3.16+ build configuration
├── README.md                   # Project documentation
├── include/
│   ├── Accounts.hpp            # IAccount interface, BaseAccount, Checking, Savings, CD
│   ├── Investment.hpp          # StockHolding, BondHolding, InvestmentAccount
│   ├── JBank.hpp               # BankEngine central registry & transfer coordinator
│   ├── Money.hpp               # Zero-drift 64-bit integer-cent currency class
│   └── Types.hpp               # Date, Customer, Transaction, TransactionType
├── src/
│   ├── Accounts.cpp            # Depository account logic, APY, overdraft, maturity
│   ├── Investment.cpp          # Equities DCA, mark-to-market, municipal bond yields
│   ├── JBank.cpp               # BankEngine atomic transfer implementation & rollback
│   ├── Money.cpp               # Checked fixed-point arithmetic & formatting
│   └── main.cpp                # Showcase demonstration & interactive operator CLI
└── tests/
    └── test_jbank.cpp          # 33-suite comprehensive regression & stress test harness
```

---

## Quickstart Guide

### Prerequisites
- **Compiler**: Modern C++20 compiler (GCC 11+, Clang 13+, or MSVC 2019+).
- **Build System**: CMake 3.16 or newer.
- **Build Tool**: Ninja, GNU Make, or MinGW `mingw32-make`.

### 1. Configure the Build

```bash
git clone https://github.com/your-username/JBank.git
cd JBank

# Configure with CMake (specify your preferred generator if desired)
cmake -B build -S .
```

*For MinGW on Windows:*
```bash
cmake -B build -S . -G "MinGW Makefiles"
```

### 2. Build

```bash
cmake --build build
```

This builds three targets:
1. `jbank_core`: Static core banking engine library.
2. `jbank_app`: Showcase CLI and interactive operator terminal.
3. `jbank_tests`: Automated unit and multithreaded stress test runner.

### 3. Run the Demonstration Showcase

```bash
./build/jbank_app
```

The application runs a complete, deterministic demonstration of six financial scenarios:
1. **KYC Onboarding**: Customer creation with Gregorian calendar validation.
2. **Account Opening**: Checking with overdraft limits, Savings with compound APY, and CD with early withdrawal penalties.
3. **Joint Accounts**: Dynamic multi-owner composition without diamond inheritance.
4. **Wealth Management**: Dollar-cost averaging on equities, mark-to-market revaluations, and municipal bond coupon yields.
5. **Atomic Transfer**: Deadlock-free inter-account transfer, balance conservation verification, and safe rejection of overdraft overruns.
6. **Audit Export**: Streaming formatted statements to the console and to `jbank_statement.txt`.

### 4. Interactive Operator Terminal

You can launch an interactive banking session using the `--interactive` (or `-i`) flag:

```bash
./build/jbank_app --interactive
```

The interactive operator menu supports:
- Viewing account details and balances
- Processing deposits and withdrawals
- Executing atomic transfers
- Inspecting wealth management portfolios and live stock quotes
- Exporting account statements to disk

### 5. Run the Test Suite

```bash
./build/jbank_tests
```

---

## Automated Test Harness

The test suite covers **33 comprehensive test suites** across 5 categories, validating edge cases, concurrency stress, and arithmetic limits:

| Category | Suites | Focus Areas |
|---|:---:|---|
| **1. Fixed-Point Money & Invariants** | 8 | IEEE-754 precision, integer overflow/underflow, division by zero, spaceship comparisons, string formatting |
| **2. Domain Types & Calendar Date** | 5 | Gregorian leap-year validation, ISO-8601 formatting, KYC customer records, transaction factory |
| **3. Account Hierarchy & Rules** | 6 | Overdraft limits, monthly compound APY, CD maturity dates and penalty deductions, polymorphic dispatch |
| **4. Wealth Management & Assets** | 7 | Weighted cost-basis averaging, mark-to-market revaluation, municipal bond yields, concurrent portfolio trades |
| **5. Concurrency & Stress Tests** | 7 | Deadlock freedom under bidirectional contention, 10-thread 2,000-transaction stress test, deposit conservation invariant |

**Result:** 33/33 test suites passing (100% pass rate, 0 warnings under `-Wall -Wextra -Wpedantic`).

---

## License & Copyright

Copyright © 2026. **All Rights Reserved.**

This software is proprietary and confidential. Unauthorized copying, distribution, modification, public display, or commercial use of this codebase, via any medium, is strictly prohibited without prior written permission from the author.
