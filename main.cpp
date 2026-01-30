#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace std;

const string DATA_DIR = "";
const int MAX_USERID = 30, MAX_PASSWORD = 30, MAX_USERNAME = 30;
const int MAX_ISBN = 20, MAX_BOOKNAME = 60, MAX_AUTHOR = 60, MAX_KEYWORD = 60;
const int MAX_PRICE_LEN = 13, MAX_QUANTITY_LEN = 10;

struct User {
    string userID, password, username;
    int privilege;
};

struct Book {
    string ISBN, bookName, author, keyword;
    double price;
    int quantity;
};

// Global state: in-memory for performance (index + cache); persist to files
map<string, User> users;
map<string, Book> books;  // ISBN -> Book
map<string, set<string>> nameIndex;   // name -> ISBNs
map<string, set<string>> authorIndex; // author -> ISBNs
map<string, set<string>> keywordIndex; // keyword -> ISBNs

vector<pair<double, double>> financeLog;  // (income, expenditure) per transaction
vector<string> opLog;  // operation log
bool initialized = false;

string initFlagPath() { return DATA_DIR + "init.flag"; }
string usersPath() { return DATA_DIR + "users.dat"; }
string booksPath() { return DATA_DIR + "books.dat"; }
string financePath() { return DATA_DIR + "finance.dat"; }
string logPath() { return DATA_DIR + "oplog.dat"; }

void ensureInit() {
    if (initialized) return;
    ifstream f(initFlagPath());
    if (f.good()) {
        f.close();
        ifstream uf(usersPath());
        if (uf.good()) {
            string line;
            while (getline(uf, line) && !line.empty()) {
                size_t i = 0;
                string uid, pw, uname; int priv;
                while (i < line.size() && line[i] != '\t') uid += line[i++]; i++;
                while (i < line.size() && line[i] != '\t') pw += line[i++]; i++;
                while (i < line.size() && line[i] != '\t') uname += line[i++]; i++;
                priv = (i < line.size()) ? (line[i] - '0') : 1;
                users[uid] = {uid, pw, uname, priv};
            }
        }
        ifstream bf(booksPath());
        if (bf.good()) {
            string line;
            while (getline(bf, line) && !line.empty()) {
                Book b; b.price = 0; b.quantity = 0;
                size_t i = 0;
                auto next = [&]() {
                    string s;
                    while (i < line.size() && line[i] != '\t') s += line[i++];
                    if (i < line.size()) i++;
                    return s;
                };
                b.ISBN = next();
                b.bookName = next();
                b.author = next();
                b.keyword = next();
                if (i < line.size()) { string p = next(); if (!p.empty()) b.price = stod(p); }
                if (i < line.size()) { string q = next(); if (!q.empty()) b.quantity = stoi(q); }
                books[b.ISBN] = b;
                if (!b.bookName.empty()) nameIndex[b.bookName].insert(b.ISBN);
                if (!b.author.empty()) authorIndex[b.author].insert(b.ISBN);
                if (!b.keyword.empty()) {
                    string k;
                    for (char c : b.keyword) {
                        if (c == '|') { if (!k.empty()) { keywordIndex[k].insert(b.ISBN); k.clear(); } }
                        else k += c;
                    }
                    if (!k.empty()) keywordIndex[k].insert(b.ISBN);
                }
            }
        }
        ifstream ff(financePath());
        if (ff.good()) {
            string line;
            while (getline(ff, line) && !line.empty()) {
                size_t i = 0;
                string a, b;
                while (i < line.size() && line[i] != '\t') a += line[i++]; i++;
                while (i < line.size()) b += line[i++];
                financeLog.push_back({stod(a), stod(b)});
            }
        }
    } else {
        users["root"] = {"root", "sjtu", "", 7};
        ofstream out(initFlagPath());
        out.close();
        ofstream uout(usersPath());
        uout << "root\tsjtu\t\t7\n";
        uout.close();
    }
    initialized = true;
}

void saveUsers() {
    ofstream f(usersPath());
    for (const auto& p : users)
        f << p.second.userID << '\t' << p.second.password << '\t' << p.second.username << '\t' << p.second.privilege << '\n';
}

void saveBooks() {
    ofstream f(booksPath());
    for (const auto& p : books)
        f << p.second.ISBN << '\t' << p.second.bookName << '\t' << p.second.author << '\t' << p.second.keyword << '\t'
          << fixed << setprecision(2) << p.second.price << '\t' << p.second.quantity << '\n';
}

void saveFinance() {
    ofstream f(financePath());
    for (const auto& p : financeLog)
        f << fixed << setprecision(2) << p.first << '\t' << p.second << '\n';
}

void appendLog(const string& s) {
    opLog.push_back(s);
    ofstream f(logPath(), ios::app);
    f << s << '\n';
}

// Validation
bool validUserID(const string& s) {
    if (s.size() > MAX_USERID) return false;
    for (char c : s) if (!isalnum(c) && c != '_') return false;
    return true;
}
bool validPassword(const string& s) {
    if (s.size() > MAX_PASSWORD) return false;
    for (char c : s) if (!isalnum(c) && c != '_') return false;
    return true;
}
bool validUsername(const string& s) {
    if (s.size() > MAX_USERNAME) return false;
    for (char c : s) if (c < 32 || c == 127) return false;
    return true;
}
bool validPriv(int p) { return p == 1 || p == 3 || p == 7; }
bool validISBN(const string& s) {
    if (s.size() > MAX_ISBN) return false;
    for (char c : s) if (c < 32 || c == 127) return false;
    return true;
}
bool validBookName(const string& s) {
    if (s.size() > MAX_BOOKNAME) return false;
    for (char c : s) if (c < 32 || c == 127 || c == '"') return false;
    return true;
}
bool validKeyword(const string& s) {
    if (s.size() > MAX_KEYWORD) return false;
    for (char c : s) if (c < 32 || c == 127 || c == '"') return false;
    return true;
}
bool validPrice(const string& s) {
    if (s.size() > MAX_PRICE_LEN) return false;
    int dot = 0;
    for (char c : s) {
        if (c == '.') dot++;
        else if (!isdigit(c)) return false;
    }
    return dot <= 1;
}
bool validQuantity(const string& s) {
    if (s.empty() || s.size() > MAX_QUANTITY_LEN) return false;
    for (char c : s) if (!isdigit(c)) return false;
    if (s.size() == 10 && s > "2147483647") return false;
    return true;
}

// Trim spaces
string trim(const string& s) {
    size_t a = s.find_first_not_of(' ');
    if (a == string::npos) return "";
    return s.substr(a, s.find_last_not_of(' ') - a + 1);
}

// Parse command line into parts; handle quoted strings
vector<string> parseCommand(const string& line) {
    vector<string> parts;
    string cur;
    bool inQuote = false;
    for (size_t i = 0; i < line.size(); i++) {
        if (inQuote) {
            if (line[i] == '"') inQuote = false;
            else cur += line[i];
        } else {
            if (line[i] == '"') inQuote = true;
            else if (line[i] == ' ') {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else cur += line[i];
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
}

int main() {
    ensureInit();

    vector<pair<string, string>> loginStack;  // (userID, selectedISBN)
    string selectedISBN;

    string line;
    while (getline(cin, line)) {
        string trimmed = trim(line);
        if (trimmed.empty()) continue;

        vector<string> parts = parseCommand(trimmed);
        if (parts.empty()) continue;

        string cmd = parts[0];
        int priv = 0;
        if (!loginStack.empty()) priv = users[loginStack.back().first].privilege;

        if (cmd == "quit" || cmd == "exit") {
            break;
        }

        if (cmd == "su") {
            if (parts.size() < 2) { cout << "Invalid\n"; continue; }
            string uid = parts[1], pw;
            if (parts.size() >= 3) pw = parts[2];
            if (!validUserID(uid) || (parts.size() >= 3 && !validPassword(pw))) { cout << "Invalid\n"; continue; }
            if (users.find(uid) == users.end()) { cout << "Invalid\n"; continue; }
            if (priv > users[uid].privilege) { /* can omit password */ }
            else if (pw != users[uid].password) { cout << "Invalid\n"; continue; }
            loginStack.push_back({uid, ""});
            appendLog("su " + uid);
            continue;
        }

        if (cmd == "logout") {
            if (priv < 1) { cout << "Invalid\n"; continue; }
            appendLog("logout " + loginStack.back().first);
            loginStack.pop_back();
            if (!loginStack.empty()) selectedISBN = loginStack.back().second;
            else selectedISBN.clear();
            continue;
        }

        if (cmd == "register") {
            if (parts.size() != 4) { cout << "Invalid\n"; continue; }
            string uid = parts[1], pw = parts[2], uname = parts[3];
            if (!validUserID(uid) || !validPassword(pw) || !validUsername(uname)) { cout << "Invalid\n"; continue; }
            if (users.count(uid)) { cout << "Invalid\n"; continue; }
            users[uid] = {uid, pw, uname, 1};
            saveUsers();
            appendLog("register " + uid);
            continue;
        }

        if (cmd == "passwd") {
            if (priv < 1) { cout << "Invalid\n"; continue; }
            if (parts.size() != 3 && parts.size() != 4) { cout << "Invalid\n"; continue; }
            string uid = parts[1], newPw = parts[parts.size()-1];
            string curPw;
            if (parts.size() == 3) {
                if (priv != 7) { cout << "Invalid\n"; continue; }
                curPw = "";  // root can omit current password
            } else {
                curPw = parts[2];
                if (!validPassword(curPw)) { cout << "Invalid\n"; continue; }
            }
            if (!validUserID(uid) || !validPassword(newPw)) { cout << "Invalid\n"; continue; }
            if (users.find(uid) == users.end()) { cout << "Invalid\n"; continue; }
            if (priv != 7 && curPw != users[uid].password) { cout << "Invalid\n"; continue; }
            users[uid].password = newPw;
            saveUsers();
            appendLog("passwd " + uid);
            continue;
        }

        if (cmd == "useradd") {
            if (priv < 3) { cout << "Invalid\n"; continue; }
            if (parts.size() != 5) { cout << "Invalid\n"; continue; }
            string uid = parts[1], pw = parts[2], uname = parts[4];
            int p = -1;
            if (parts[3].size() == 1 && isdigit(parts[3][0])) p = parts[3][0] - '0';
            if (!validUserID(uid) || !validPassword(pw) || !validUsername(uname) || !validPriv(p)) { cout << "Invalid\n"; continue; }
            if (p >= priv || users.count(uid)) { cout << "Invalid\n"; continue; }
            users[uid] = {uid, pw, uname, p};
            saveUsers();
            appendLog("useradd " + uid);
            continue;
        }

        if (cmd == "delete") {
            if (priv < 7) { cout << "Invalid\n"; continue; }
            if (parts.size() != 2) { cout << "Invalid\n"; continue; }
            string uid = parts[1];
            if (!validUserID(uid)) { cout << "Invalid\n"; continue; }
            if (users.find(uid) == users.end()) { cout << "Invalid\n"; continue; }
            for (const auto& p : loginStack) if (p.first == uid) { cout << "Invalid\n"; goto next_cmd; }
            users.erase(uid);
            saveUsers();
            appendLog("delete " + uid);
            continue;
        next_cmd:;
        }

        if (cmd == "show" && parts.size() >= 2 && parts[1] == "finance") {
            if (priv < 7) { cout << "Invalid\n"; continue; }
            if (parts.size() == 2) {
                double inc = 0, exp = 0;
                for (const auto& p : financeLog) { inc += p.first; exp += p.second; }
                cout << "+ " << fixed << setprecision(2) << inc << " - " << exp << '\n';
                continue;
            }
            if (parts.size() == 3) {
                string cs = parts[2];
                if (!validQuantity(cs)) { cout << "Invalid\n"; continue; }
                int c = stoi(cs);
                if (c == 0) { cout << '\n'; continue; }
                if (c > (int)financeLog.size()) { cout << "Invalid\n"; continue; }
                double inc = 0, exp = 0;
                for (int i = financeLog.size() - c; i < (int)financeLog.size(); i++) {
                    inc += financeLog[i].first; exp += financeLog[i].second;
                }
                cout << "+ " << fixed << setprecision(2) << inc << " - " << exp << '\n';
                continue;
            }
            cout << "Invalid\n";
            continue;
        }

        if (cmd == "show") {
            if (priv < 1) { cout << "Invalid\n"; continue; }
            set<string> isbns;
            if (parts.size() == 1) {
                for (const auto& b : books) isbns.insert(b.first);
            } else {
                if (parts.size() != 2) { cout << "Invalid\n"; continue; }
                string arg = parts[1];
                if (arg.find("-ISBN=") == 0) {
                    string isbn = arg.substr(6);
                    if (isbn.empty()) { cout << "Invalid\n"; continue; }
                    if (books.count(isbn)) isbns.insert(isbn);
                } else if (arg.find("-name=") == 0) {
                    string name = (arg.size() > 7 && arg[6] == '"' && arg.back() == '"') ? arg.substr(8, arg.size()-9) : arg.substr(6);
                    if (name.empty()) { cout << "Invalid\n"; continue; }
                    if (nameIndex.count(name)) for (const string& isbn : nameIndex[name]) isbns.insert(isbn);
                } else if (arg.find("-author=") == 0) {
                    string author = (arg.size() > 9 && arg[8] == '"' && arg.back() == '"') ? arg.substr(10, arg.size()-11) : arg.substr(8);
                    if (author.empty()) { cout << "Invalid\n"; continue; }
                    if (authorIndex.count(author)) for (const string& isbn : authorIndex[author]) isbns.insert(isbn);
                } else if (arg.find("-keyword=") == 0) {
                    string kw = (arg.size() > 11 && arg[10] == '"' && arg.back() == '"') ? arg.substr(11, arg.size()-12) : arg.substr(9);
                    if (kw.empty()) { cout << "Invalid\n"; continue; }
                    if (kw.find('|') != string::npos) { cout << "Invalid\n"; continue; }  // multiple keywords not allowed
                    if (keywordIndex.count(kw)) for (const string& isbn : keywordIndex[kw]) isbns.insert(isbn);
                } else { cout << "Invalid\n"; continue; }
            }
            vector<string> list(isbns.begin(), isbns.end());
            sort(list.begin(), list.end());
            if (list.empty()) cout << '\n';
            else for (const string& isbn : list) {
                Book& b = books[isbn];
                cout << b.ISBN << '\t' << b.bookName << '\t' << b.author << '\t' << b.keyword << '\t'
                     << fixed << setprecision(2) << b.price << '\t' << b.quantity << '\n';
            }
            continue;
        }

        if (cmd == "buy") {
            if (priv < 1) { cout << "Invalid\n"; continue; }
            if (parts.size() != 3) { cout << "Invalid\n"; continue; }
            string isbn = parts[1], qs = parts[2];
            if (!validISBN(isbn) || !validQuantity(qs)) { cout << "Invalid\n"; continue; }
            int q = stoi(qs);
            if (q <= 0) { cout << "Invalid\n"; continue; }
            if (books.find(isbn) == books.end()) { cout << "Invalid\n"; continue; }
            Book& b = books[isbn];
            if (b.quantity < q) { cout << "Invalid\n"; continue; }
            b.quantity -= q;
            double total = b.price * q;
            financeLog.push_back({total, 0});
            saveFinance();
            saveBooks();
            cout << fixed << setprecision(2) << total << '\n';
            appendLog("buy " + isbn + " " + to_string(q));
            continue;
        }

        if (cmd == "select") {
            if (priv < 3) { cout << "Invalid\n"; continue; }
            if (parts.size() != 2) { cout << "Invalid\n"; continue; }
            string isbn = parts[1];
            if (!validISBN(isbn)) { cout << "Invalid\n"; continue; }
            if (!loginStack.empty()) loginStack.back().second = isbn;
            selectedISBN = isbn;
            if (books.find(isbn) == books.end()) {
                books[isbn] = {isbn, "", "", "", 0, 0};
                saveBooks();
            }
            appendLog("select " + isbn + " by " + (loginStack.empty() ? "" : loginStack.back().first));
            continue;
        }

        if (cmd == "modify") {
            if (priv < 3) { cout << "Invalid\n"; continue; }
            if (selectedISBN.empty() || parts.size() < 2) { cout << "Invalid\n"; continue; }
            Book& b = books[selectedISBN];
            set<string> seen;
            string newISBN;
            for (size_t i = 1; i < parts.size(); i++) {
                string arg = parts[i];
                if (arg.find("-ISBN=") == 0) {
                    if (seen.count("ISBN")) { cout << "Invalid\n"; goto next_mod; }
                    seen.insert("ISBN");
                    newISBN = arg.substr(6);
                    if (newISBN.empty()) { cout << "Invalid\n"; goto next_mod; }
                    if (newISBN == selectedISBN) { cout << "Invalid\n"; goto next_mod; }
                    if (books.count(newISBN)) { cout << "Invalid\n"; goto next_mod; }
                } else if (arg.find("-name=") == 0) {
                    if (seen.count("name")) { cout << "Invalid\n"; goto next_mod; }
                    seen.insert("name");
                    string v = (arg.size() > 7 && arg[6] == '"' && arg.back() == '"') ? arg.substr(8, arg.size()-9) : arg.substr(6);
                    if (!validBookName(v)) { cout << "Invalid\n"; goto next_mod; }
                } else if (arg.find("-author=") == 0) {
                    if (seen.count("author")) { cout << "Invalid\n"; goto next_mod; }
                    seen.insert("author");
                    string v = (arg.size() > 9 && arg[8] == '"' && arg.back() == '"') ? arg.substr(10, arg.size()-11) : arg.substr(8);
                    if (!validBookName(v)) { cout << "Invalid\n"; goto next_mod; }
                } else if (arg.find("-keyword=") == 0) {
                    if (seen.count("keyword")) { cout << "Invalid\n"; goto next_mod; }
                    seen.insert("keyword");
                    string kw = (arg.size() > 11 && arg[10] == '"' && arg.back() == '"') ? arg.substr(11, arg.size()-12) : arg.substr(9);
                    if (!validKeyword(kw)) { cout << "Invalid\n"; goto next_mod; }
                    set<string> segs;
                    string s;
                    for (char c : kw) {
                        if (c == '|') { if (!s.empty()) { if (segs.count(s)) { cout << "Invalid\n"; goto next_mod; } segs.insert(s); s.clear(); } }
                        else s += c;
                    }
                    if (!s.empty() && segs.count(s)) { cout << "Invalid\n"; goto next_mod; }
                } else if (arg.find("-price=") == 0) {
                    if (seen.count("price")) { cout << "Invalid\n"; goto next_mod; }
                    seen.insert("price");
                    string p = arg.substr(7);
                    if (p.empty() || !validPrice(p)) { cout << "Invalid\n"; goto next_mod; }
                } else { cout << "Invalid\n"; goto next_mod; }
            }
            // Apply modifications
            for (size_t i = 1; i < parts.size(); i++) {
                string arg = parts[i];
                if (arg.find("-ISBN=") == 0) {
                    newISBN = arg.substr(6);
                } else if (arg.find("-name=") == 0) {
                    string newName = (arg.size() > 7 && arg[6] == '"' && arg.back() == '"') ? arg.substr(8, arg.size()-9) : arg.substr(6);
                    string oldName = b.bookName;
                    if (!oldName.empty()) { nameIndex[oldName].erase(selectedISBN); if (nameIndex[oldName].empty()) nameIndex.erase(oldName); }
                    b.bookName = newName;
                    if (!newName.empty()) nameIndex[newName].insert(selectedISBN);
                } else if (arg.find("-author=") == 0) {
                    string newA = (arg.size() > 9 && arg[8] == '"' && arg.back() == '"') ? arg.substr(10, arg.size()-11) : arg.substr(8);
                    string oldA = b.author;
                    if (!oldA.empty()) { authorIndex[oldA].erase(selectedISBN); if (authorIndex[oldA].empty()) authorIndex.erase(oldA); }
                    b.author = newA;
                    if (!newA.empty()) authorIndex[newA].insert(selectedISBN);
                } else if (arg.find("-keyword=") == 0) {
                    string newKw = (arg.size() > 11 && arg[10] == '"' && arg.back() == '"') ? arg.substr(11, arg.size()-12) : arg.substr(9);
                    string oldKw = b.keyword;
                    if (!oldKw.empty()) {
                        string k;
                        for (char c : oldKw) {
                            if (c == '|') { if (!k.empty()) { keywordIndex[k].erase(selectedISBN); if (keywordIndex[k].empty()) keywordIndex.erase(k); k.clear(); } }
                            else k += c;
                        }
                        if (!k.empty()) { keywordIndex[k].erase(selectedISBN); if (keywordIndex[k].empty()) keywordIndex.erase(k); }
                    }
                    b.keyword = newKw;
                    if (!newKw.empty()) {
                        string k;
                        for (char c : newKw) {
                            if (c == '|') { if (!k.empty()) { keywordIndex[k].insert(selectedISBN); k.clear(); } }
                            else k += c;
                        }
                        if (!k.empty()) keywordIndex[k].insert(selectedISBN);
                    }
                } else if (arg.find("-price=") == 0) {
                    b.price = stod(arg.substr(7));
                }
            }
            if (!newISBN.empty()) {
                Book bCopy = b;
                books.erase(selectedISBN);
                bCopy.ISBN = newISBN;
                books[newISBN] = bCopy;
                for (auto& p : loginStack) if (p.second == selectedISBN) p.second = newISBN;
                selectedISBN = newISBN;
            }
            saveBooks();
            appendLog("modify " + selectedISBN);
            continue;
        next_mod:;
        }

        if (cmd == "import") {
            if (priv < 3) { cout << "Invalid\n"; continue; }
            if (selectedISBN.empty() || parts.size() != 3) { cout << "Invalid\n"; continue; }
            string qs = parts[1], costStr = parts[2];
            if (!validQuantity(qs) || !validPrice(costStr)) { cout << "Invalid\n"; continue; }
            int q = stoi(qs);
            double cost = stod(costStr);
            if (q <= 0 || cost <= 0) { cout << "Invalid\n"; continue; }
            Book& b = books[selectedISBN];
            b.quantity += q;
            financeLog.push_back({0, cost});
            saveFinance();
            saveBooks();
            appendLog("import " + selectedISBN + " " + to_string(q));
            continue;
        }

        if (cmd == "log" || cmd == "report") {
            if (priv < 7) { cout << "Invalid\n"; continue; }
            if (cmd == "log") {
                for (const string& s : opLog) cout << s << '\n';
                cout << "--- Finance ---\n";
                for (size_t i = 0; i < financeLog.size(); i++)
                    cout << "Transaction " << (i+1) << ": +" << fixed << setprecision(2) << financeLog[i].first << " -" << financeLog[i].second << '\n';
            } else if (parts.size() == 2) {
                if (parts[1] == "finance") {
                    double inc = 0, exp = 0;
                    for (const auto& p : financeLog) { inc += p.first; exp += p.second; }
                    cout << "=== Finance Report ===\n+ " << fixed << setprecision(2) << inc << " - " << exp << "\nTotal: " << (inc - exp) << '\n';
                } else if (parts[1] == "employee") {
                    cout << "=== Employee Report ===\n";
                    for (const string& s : opLog) cout << s << '\n';
                } else cout << "Invalid\n";
            } else cout << "Invalid\n";
            continue;
        }

        cout << "Invalid\n";
    }
    return 0;
}
