// Smart Banking and Financial Management System
// OOP Mid Term Break Assignment - Project 3
// Compile: g++ -std=c++17 -o bank bank.cpp
// Run:     ./bank          (interactive menu)
//          ./bank --test   (runs the built-in test cases)

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <limits>
#include <cctype>

using namespace std;

// ---------------------------------------------------------------
// Exception type used for every business-rule violation
// ---------------------------------------------------------------
class BankException : public runtime_error {
public:
    explicit BankException(const string& msg) : runtime_error(msg) {}
};

// ---------------------------------------------------------------
// Clock: the bank works in whole days so FD maturity and monthly
// processing can be demonstrated without waiting in real time.
// ---------------------------------------------------------------
class Clock {
    static int today;
public:
    static int now() { return today; }
    static void advance(int days) { today += days; }
    static void reset() { today = 0; }
};
int Clock::today = 0;

string money(double v) {
    ostringstream os;
    os << fixed << setprecision(2) << v;
    return os.str();
}

// ---------------------------------------------------------------
// Transaction: one line of an account's history
// ---------------------------------------------------------------
struct Transaction {
    int id;
    int day;
    string type;
    double amount;
    double balanceAfter;
    string note;

    Transaction(int i, int d, const string& t, double a, double b, const string& n)
        : id(i), day(d), type(t), amount(a), balanceAfter(b), note(n) {}
};

ostream& operator<<(ostream& os, const Transaction& t) {
    os << left << setw(5) << t.id
       << setw(6) << t.day
       << setw(18) << t.type
       << right << fixed << setprecision(2)
       << setw(12) << t.amount
       << setw(14) << t.balanceAfter
       << "  " << t.note;
    return os;
}

// ---------------------------------------------------------------
// Customer: holds personal details and the accounts they own
// (the accounts themselves are owned and deleted by Bank)
// ---------------------------------------------------------------
class Account;

class Customer {
    string id, name, phone, address;
    vector<Account*> accounts;
public:
    Customer(const string& i, const string& n, const string& p, const string& a)
        : id(i), name(n), phone(p), address(a) {}

    const string& getId() const { return id; }
    const string& getName() const { return name; }
    const string& getPhone() const { return phone; }
    const string& getAddress() const { return address; }
    const vector<Account*>& getAccounts() const { return accounts; }
    void addAccount(Account* a) { accounts.push_back(a); }
};

// ---------------------------------------------------------------
// Account: abstract base class
// ---------------------------------------------------------------
class Account {
protected:
    static int nextAccNo;
    int accNo;
    Customer* owner;
    double balance;
    int openedDay;
    int txnCounter;
    vector<Transaction> history;

    void record(const string& type, double amt, const string& note) {
        history.push_back(Transaction(++txnCounter, Clock::now(), type, amt, balance, note));
    }

public:
    Account(Customer* o, double opening)
        : accNo(nextAccNo++), owner(o), balance(opening),
          openedDay(Clock::now()), txnCounter(0) {}

    virtual ~Account() {}

    int getAccNo() const { return accNo; }
    double getBalance() const { return balance; }
    Customer* getOwner() const { return owner; }
    const vector<Transaction>& getHistory() const { return history; }

    virtual string typeName() const = 0;
    virtual void withdraw(double amt, const string& note = "Cash withdrawal") = 0;
    virtual void monthEnd() = 0;
    virtual bool canReceiveTransfer() const { return true; }

    virtual void deposit(double amt, const string& note = "Cash deposit") {
        if (amt <= 0)
            throw BankException("Deposit amount must be greater than zero.");
        balance += amt;
        record("Deposit", amt, note);
    }

    virtual void printDetails(ostream& os) const {
        os << "  Account No : " << accNo << "\n"
           << "  Type       : " << typeName() << "\n"
           << "  Holder     : " << owner->getName() << " (" << owner->getId() << ")\n"
           << "  Balance    : Rs. " << money(balance) << "\n";
    }

    // operator overloads: deposit / withdraw / compare / print
    Account& operator+=(double amt) { deposit(amt); return *this; }
    Account& operator-=(double amt) { withdraw(amt); return *this; }
    bool operator<(const Account& other) const { return balance < other.balance; }
    bool operator==(const Account& other) const { return accNo == other.accNo; }

    friend ostream& operator<<(ostream& os, const Account& a) {
        a.printDetails(os);
        return os;
    }
};
int Account::nextAccNo = 1001;

// ---------------------------------------------------------------
// SavingsAccount
//   - minimum balance Rs. 1000
//   - 4% p.a. interest, credited every month end
//   - 3 free withdrawals per month, Rs. 20 fee for each extra one
// ---------------------------------------------------------------
class SavingsAccount : public Account {
    static constexpr double MIN_BALANCE = 1000.0;
    static constexpr double RATE = 0.04;
    static constexpr int FREE_WITHDRAWALS = 3;
    static constexpr double EXTRA_FEE = 20.0;
    int withdrawalsThisMonth;
public:
    SavingsAccount(Customer* o, double opening) : Account(o, opening), withdrawalsThisMonth(0) {
        if (opening < MIN_BALANCE)
            throw BankException("Savings account needs an opening balance of at least Rs. 1000.");
        record("Account opened", opening, "Opening balance");
    }

    string typeName() const override { return "Savings"; }

    void withdraw(double amt, const string& note = "Cash withdrawal") override {
        if (amt <= 0)
            throw BankException("Withdrawal amount must be greater than zero.");
        double fee = (withdrawalsThisMonth >= FREE_WITHDRAWALS) ? EXTRA_FEE : 0.0;
        if (balance - amt - fee < MIN_BALANCE)
            throw BankException("Withdrawal rejected: balance would fall below the minimum of Rs. 1000.");
        balance -= amt;
        record("Withdrawal", amt, note);
        withdrawalsThisMonth++;
        if (fee > 0) {
            balance -= fee;
            record("Penalty", fee, "Fee for withdrawal beyond monthly limit");
        }
    }

    void monthEnd() override {
        double interest = balance * RATE / 12.0;
        balance += interest;
        record("Interest", interest, "Monthly interest at 4% p.a.");
        withdrawalsThisMonth = 0;
    }

    void printDetails(ostream& os) const override {
        Account::printDetails(os);
        os << "  Min balance: Rs. " << money(MIN_BALANCE) << "\n"
           << "  Withdrawals this month: " << withdrawalsThisMonth << " (free: " << FREE_WITHDRAWALS << ")\n";
    }
};

// ---------------------------------------------------------------
// CurrentAccount
//   - opening balance Rs. 5000, overdraft limit Rs. 20000
//   - 12% p.a. interest charged monthly on the overdrawn amount
// ---------------------------------------------------------------
class CurrentAccount : public Account {
    static constexpr double MIN_OPENING = 5000.0;
    static constexpr double OD_LIMIT = 20000.0;
    static constexpr double OD_RATE = 0.12;
public:
    CurrentAccount(Customer* o, double opening) : Account(o, opening) {
        if (opening < MIN_OPENING)
            throw BankException("Current account needs an opening balance of at least Rs. 5000.");
        record("Account opened", opening, "Opening balance");
    }

    string typeName() const override { return "Current"; }

    void withdraw(double amt, const string& note = "Cash withdrawal") override {
        if (amt <= 0)
            throw BankException("Withdrawal amount must be greater than zero.");
        if (balance - amt < -OD_LIMIT)
            throw BankException("Withdrawal rejected: overdraft limit of Rs. 20000 would be exceeded.");
        balance -= amt;
        record("Withdrawal", amt, note);
    }

    void monthEnd() override {
        if (balance < 0) {
            double charge = (-balance) * OD_RATE / 12.0;
            balance -= charge;
            record("Overdraft interest", charge, "12% p.a. on overdrawn amount");
        }
    }

    void printDetails(ostream& os) const override {
        Account::printDetails(os);
        os << "  Overdraft limit: Rs. " << money(OD_LIMIT) << "\n";
    }
};

// ---------------------------------------------------------------
// FixedDepositAccount
//   - one-time deposit for a fixed term, 7% p.a. on maturity
//   - no further deposits or incoming transfers
//   - can only be withdrawn in full; closing early earns 4% p.a.
//     for the days run and a 1% penalty on the principal
// ---------------------------------------------------------------
class FixedDepositAccount : public Account {
    static constexpr double MIN_DEPOSIT = 10000.0;
    static constexpr double RATE = 0.07;
    static constexpr double EARLY_RATE = 0.04;
    static constexpr double EARLY_PENALTY = 0.01;
    double principal;
    int termDays;
    bool closed;
public:
    FixedDepositAccount(Customer* o, double amount, int days)
        : Account(o, amount), principal(amount), termDays(days), closed(false) {
        if (amount < MIN_DEPOSIT)
            throw BankException("Fixed deposit needs a minimum amount of Rs. 10000.");
        if (days < 30)
            throw BankException("Fixed deposit term must be at least 30 days.");
        record("FD opened", amount, "Term " + to_string(days) + " days at 7% p.a.");
    }

    string typeName() const override { return "Fixed Deposit"; }
    bool canReceiveTransfer() const override { return false; }
    bool isClosed() const { return closed; }
    int maturityDay() const { return openedDay + termDays; }
    bool isMatured() const { return Clock::now() >= maturityDay(); }

    double payoutAmount() const {
        if (closed) return 0.0;
        if (isMatured())
            return principal + principal * RATE * termDays / 365.0;
        int elapsed = Clock::now() - openedDay;
        return principal + principal * EARLY_RATE * elapsed / 365.0 - principal * EARLY_PENALTY;
    }

    void deposit(double, const string& = "") override {
        throw BankException("Deposits are not allowed into a fixed deposit account.");
    }

    void withdraw(double amt, const string& note = "FD closure") override {
        if (closed)
            throw BankException("This fixed deposit is already closed.");
        double pay = payoutAmount();
        if (amt < pay - 0.005 || amt > pay + 0.005)
            throw BankException("A fixed deposit can only be withdrawn in full. Amount payable today: Rs. " + money(pay));
        if (!isMatured()) {
            double penalty = principal * EARLY_PENALTY;
            record("Penalty", penalty, "Premature closure penalty (1% of principal)");
        }
        balance = 0.0;
        closed = true;
        record("FD closed", pay, note);
    }

    void monthEnd() override {}  // interest is paid only on closure

    void printDetails(ostream& os) const override {
        Account::printDetails(os);
        os << "  Principal  : Rs. " << money(principal) << "\n"
           << "  Term       : " << termDays << " days (matures on day " << maturityDay() << ")\n"
           << "  Status     : " << (closed ? "Closed" : (isMatured() ? "Matured" : "Running")) << "\n";
        if (!closed)
            os << "  Payable if closed today: Rs. " << money(payoutAmount()) << "\n";
    }
};

// ---------------------------------------------------------------
// Bank: owns customers and accounts, coordinates operations
// ---------------------------------------------------------------
class Bank {
    string name;
    vector<Customer*> customers;
    vector<Account*> accounts;
    int customerSeq;
public:
    Bank(const string& n) : name(n), customerSeq(100) {}
    Bank(const Bank&) = delete;
    Bank& operator=(const Bank&) = delete;

    ~Bank() {
        for (size_t i = 0; i < accounts.size(); i++) delete accounts[i];
        for (size_t i = 0; i < customers.size(); i++) delete customers[i];
    }

    const string& getName() const { return name; }
    const vector<Customer*>& getCustomers() const { return customers; }
    const vector<Account*>& getAccounts() const { return accounts; }

    Customer* registerCustomer(const string& cname, const string& phone, const string& address) {
        if (cname.empty())
            throw BankException("Customer name cannot be empty.");
        if (phone.size() != 10)
            throw BankException("Phone number must have exactly 10 digits.");
        for (size_t i = 0; i < phone.size(); i++)
            if (!isdigit((unsigned char)phone[i]))
                throw BankException("Phone number must contain digits only.");
        string id = "C" + to_string(++customerSeq);
        Customer* c = new Customer(id, cname, phone, address);
        customers.push_back(c);
        return c;
    }

    Customer* findCustomer(const string& id) const {
        for (size_t i = 0; i < customers.size(); i++)
            if (customers[i]->getId() == id) return customers[i];
        return nullptr;
    }

    Account* findAccount(int no) const {
        for (size_t i = 0; i < accounts.size(); i++)
            if (accounts[i]->getAccNo() == no) return accounts[i];
        return nullptr;
    }

    // type: 'S' savings, 'C' current, 'F' fixed deposit
    Account* openAccount(Customer* c, char type, double amount, int termDays = 0) {
        if (c == nullptr)
            throw BankException("Customer not found.");
        Account* a = nullptr;
        switch (toupper(type)) {
            case 'S': a = new SavingsAccount(c, amount); break;
            case 'C': a = new CurrentAccount(c, amount); break;
            case 'F': a = new FixedDepositAccount(c, amount, termDays); break;
            default: throw BankException("Unknown account type.");
        }
        accounts.push_back(a);
        c->addAccount(a);
        return a;
    }

    Account* require(int no) const {
        Account* a = findAccount(no);
        if (!a) throw BankException("Account " + to_string(no) + " does not exist.");
        return a;
    }

    void transfer(int fromNo, int toNo, double amt) {
        if (fromNo == toNo)
            throw BankException("Cannot transfer to the same account.");
        Account* from = require(fromNo);
        Account* to = require(toNo);
        if (amt <= 0)
            throw BankException("Transfer amount must be greater than zero.");
        if (!to->canReceiveTransfer())
            throw BankException("Destination account cannot receive transfers.");
        from->withdraw(amt, "Transfer to " + to_string(toNo));
        to->deposit(amt, "Transfer from " + to_string(fromNo));
    }

    void monthEndProcessing() {
        for (size_t i = 0; i < accounts.size(); i++)
            accounts[i]->monthEnd();
    }

    void printStatement(int no) const {
        Account* a = require(no);
        cout << "\n" << string(74, '=') << "\n";
        cout << "  " << name << " - Account Statement\n";
        cout << string(74, '=') << "\n";
        cout << *a;
        cout << string(74, '-') << "\n";
        cout << left << setw(5) << "No" << setw(6) << "Day" << setw(18) << "Type"
             << right << setw(12) << "Amount" << setw(14) << "Balance" << "  Note\n";
        cout << string(74, '-') << "\n";
        const vector<Transaction>& h = a->getHistory();
        for (size_t i = 0; i < h.size(); i++) cout << h[i] << "\n";
        cout << string(74, '-') << "\n";
        cout << "  Closing balance: Rs. " << money(a->getBalance()) << "\n\n";
    }

    void listAccounts() const {
        if (accounts.empty()) { cout << "No accounts yet.\n"; return; }
        cout << left << setw(10) << "Acc No" << setw(16) << "Type" << setw(8) << "Cust"
             << setw(20) << "Holder" << right << setw(14) << "Balance" << "\n";
        cout << string(68, '-') << "\n";
        for (size_t i = 0; i < accounts.size(); i++) {
            Account* a = accounts[i];
            cout << left << setw(10) << a->getAccNo() << setw(16) << a->typeName()
                 << setw(8) << a->getOwner()->getId() << setw(20) << a->getOwner()->getName()
                 << right << setw(14) << money(a->getBalance()) << "\n";
        }
    }
};

// ---------------------------------------------------------------
// Input helpers for the menu
// ---------------------------------------------------------------
string readLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}

double readNumber(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        try {
            size_t used = 0;
            double v = stod(s, &used);
            if (used == s.size()) return v;
        } catch (...) {}
        cout << "  Please enter a valid number.\n";
    }
}

int readInt(const string& prompt) { return (int)readNumber(prompt); }

void showMenu() {
    cout << "\n=========== SMART BANKING SYSTEM ===========\n"
         << " 1. Register customer\n"
         << " 2. Open account (Savings / Current / FD)\n"
         << " 3. Deposit\n"
         << " 4. Withdraw\n"
         << " 5. Transfer money\n"
         << " 6. Account statement\n"
         << " 7. Account details\n"
         << " 8. List all accounts\n"
         << " 9. Month-end processing (interest / penalties)\n"
         << "10. Advance system date (days)\n"
         << " 0. Exit\n"
         << "Choice: ";
}

void runMenu() {
    Bank bank("Smart Bank");
    while (true) {
        showMenu();
        string line;
        if (!getline(cin, line)) break;
        int choice;
        try { choice = stoi(line); } catch (...) { cout << "Invalid choice.\n"; continue; }
        if (choice == 0) break;

        try {
            switch (choice) {
                case 1: {
                    string n = readLine("Name: ");
                    string p = readLine("Phone (10 digits): ");
                    string a = readLine("Address: ");
                    Customer* c = bank.registerCustomer(n, p, a);
                    cout << "Registered. Customer ID: " << c->getId() << "\n";
                    break;
                }
                case 2: {
                    string id = readLine("Customer ID: ");
                    string t = readLine("Type (S = Savings, C = Current, F = Fixed deposit): ");
                    if (t.empty()) throw BankException("Account type is required.");
                    double amt = readNumber("Opening amount: ");
                    int term = 0;
                    if (toupper(t[0]) == 'F') term = readInt("Term in days: ");
                    Account* a = bank.openAccount(bank.findCustomer(id), t[0], amt, term);
                    cout << "Account opened. Account No: " << a->getAccNo() << "\n";
                    break;
                }
                case 3: {
                    int no = readInt("Account No: ");
                    double amt = readNumber("Amount: ");
                    Account* a = bank.require(no);
                    *a += amt;
                    cout << "Deposited. New balance: Rs. " << money(a->getBalance()) << "\n";
                    break;
                }
                case 4: {
                    int no = readInt("Account No: ");
                    double amt = readNumber("Amount: ");
                    Account* a = bank.require(no);
                    *a -= amt;
                    cout << "Withdrawn. New balance: Rs. " << money(a->getBalance()) << "\n";
                    break;
                }
                case 5: {
                    int from = readInt("From account: ");
                    int to = readInt("To account: ");
                    double amt = readNumber("Amount: ");
                    bank.transfer(from, to, amt);
                    cout << "Transfer complete.\n";
                    break;
                }
                case 6:
                    bank.printStatement(readInt("Account No: "));
                    break;
                case 7:
                    cout << *bank.require(readInt("Account No: "));
                    break;
                case 8:
                    bank.listAccounts();
                    break;
                case 9:
                    bank.monthEndProcessing();
                    cout << "Month-end processing done for " << bank.getAccounts().size() << " account(s).\n";
                    break;
                case 10: {
                    int d = readInt("Days to advance: ");
                    if (d < 0) throw BankException("Cannot move the date backwards.");
                    Clock::advance(d);
                    cout << "System day is now " << Clock::now() << ".\n";
                    break;
                }
                default:
                    cout << "Invalid choice.\n";
            }
        } catch (const BankException& e) {
            cout << "Error: " << e.what() << "\n";
        }
    }
    cout << "Goodbye.\n";
}

// ---------------------------------------------------------------
// Test cases (run with ./bank --test)
// ---------------------------------------------------------------
static int passed = 0, failed = 0;

void report(const string& id, const string& what, bool ok) {
    cout << left << setw(7) << id << setw(62) << what << (ok ? "PASS" : "FAIL") << "\n";
    if (ok) passed++; else failed++;
}

bool throws(function<void()> f) {
    try { f(); } catch (const BankException&) { return true; }
    return false;
}

bool near(double a, double b) { return a > b - 0.01 && a < b + 0.01; }

void runTests() {
    Clock::reset();
    Bank bank("Test Bank");
    Customer* amit = bank.registerCustomer("Amit Verma", "9876543210", "Jalandhar");
    Customer* neha = bank.registerCustomer("Neha Singh", "9123456780", "Amritsar");

    cout << left << setw(7) << "ID" << setw(62) << "Test case" << "Result\n";
    cout << string(76, '-') << "\n";

    report("TC01", "Register customer with valid details", amit != nullptr && amit->getId() == "C101");
    report("TC02", "Reject phone number with fewer than 10 digits",
           throws([&] { bank.registerCustomer("X", "12345", "Y"); }));
    report("TC03", "Reject phone number containing letters",
           throws([&] { bank.registerCustomer("X", "98765abcde", "Y"); }));

    Account* sav = bank.openAccount(amit, 'S', 5000);
    report("TC04", "Open savings account with Rs. 5000", sav->getBalance() == 5000);
    report("TC05", "Reject savings account below Rs. 1000 opening balance",
           throws([&] { bank.openAccount(neha, 'S', 500); }));

    Account* cur = bank.openAccount(neha, 'C', 10000);
    report("TC06", "Open current account with Rs. 10000", cur->getBalance() == 10000);
    report("TC07", "Reject current account below Rs. 5000",
           throws([&] { bank.openAccount(neha, 'C', 2000); }));

    *sav += 2000;
    report("TC08", "Deposit Rs. 2000 using operator+=", sav->getBalance() == 7000);
    report("TC09", "Reject negative deposit", throws([&] { *sav += -50; }));

    *sav -= 1000;
    report("TC10", "Withdraw Rs. 1000 using operator-=", sav->getBalance() == 6000);
    report("TC11", "Savings withdrawal below minimum balance is rejected",
           throws([&] { *sav -= 5500; }) && sav->getBalance() == 6000);

    report("TC12", "Current account overdraft within limit allowed",
           !throws([&] { *cur -= 25000; }) && cur->getBalance() == -15000);
    report("TC13", "Current account overdraft beyond Rs. 20000 rejected",
           throws([&] { *cur -= 6000; }) && cur->getBalance() == -15000);

    bank.transfer(sav->getAccNo(), cur->getAccNo(), 2000);
    report("TC14", "Transfer Rs. 2000 savings to current",
           sav->getBalance() == 4000 && cur->getBalance() == -13000);
    report("TC15", "Transfer to same account rejected",
           throws([&] { bank.transfer(sav->getAccNo(), sav->getAccNo(), 100); }));
    report("TC16", "Transfer to non-existent account rejected",
           throws([&] { bank.transfer(sav->getAccNo(), 9999, 100); }));
    report("TC17", "Failed transfer leaves source balance unchanged",
           throws([&] { bank.transfer(sav->getAccNo(), cur->getAccNo(), 3500); }) && sav->getBalance() == 4000);

    // savings: 1 withdrawal done above (TC10) + 1 transfer (TC14) = 2 used; third is free, fourth charged
    *sav -= 100;
    double before = sav->getBalance();
    *sav -= 100;
    report("TC18", "Fourth withdrawal in a month carries Rs. 20 penalty", near(before - 100 - 20, sav->getBalance()));

    double b = sav->getBalance();
    bank.monthEndProcessing();
    report("TC19", "Savings interest credited at 4% p.a. (monthly)", near(sav->getBalance(), b + b * 0.04 / 12));

    report("TC20", "Overdraft interest charged at 12% p.a. on negative balance",
           near(cur->getBalance(), -13000 - 13000 * 0.12 / 12));

    Account* fd = bank.openAccount(amit, 'F', 50000, 365);
    report("TC21", "Open fixed deposit of Rs. 50000 for 365 days", fd->getBalance() == 50000);
    report("TC22", "Deposit into fixed deposit rejected", throws([&] { *fd += 100; }));
    report("TC23", "Transfer into fixed deposit rejected",
           throws([&] { bank.transfer(sav->getAccNo(), fd->getAccNo(), 100); }));
    report("TC24", "Partial FD withdrawal rejected", throws([&] { *fd -= 1000; }));
    report("TC25", "Reject FD below Rs. 10000",
           throws([&] { bank.openAccount(amit, 'F', 5000, 365); }));

    FixedDepositAccount* fdp = dynamic_cast<FixedDepositAccount*>(fd);
    Clock::advance(365);
    report("TC26", "FD pays 7% interest on maturity", near(fdp->payoutAmount(), 53500.00));
    *fd -= fdp->payoutAmount();
    report("TC27", "FD closes after full withdrawal", fdp->isClosed() && fd->getBalance() == 0);

    Account* fd2 = bank.openAccount(neha, 'F', 20000, 180);
    FixedDepositAccount* fdp2 = dynamic_cast<FixedDepositAccount*>(fd2);
    Clock::advance(90);
    // 4% for 90 days = 197.26, minus 1% penalty = 200
    report("TC28", "Premature FD closure applies 1% penalty and 4% rate",
           near(fdp2->payoutAmount(), 20000 + 20000 * 0.04 * 90 / 365.0 - 200));

    report("TC29", "Statement history is recorded for savings account", sav->getHistory().size() >= 6);
    report("TC30", "Account comparison using operator<", (*cur < *sav) && !(*sav < *cur));

    cout << string(76, '-') << "\n";
    cout << "Total: " << (passed + failed) << "   Passed: " << passed << "   Failed: " << failed << "\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1 && string(argv[1]) == "--test") {
        runTests();
        return failed == 0 ? 0 : 1;
    }
    runMenu();
    return 0;
}