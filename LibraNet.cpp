// LibraNet.cpp
// Compile: g++ -std=c++17 LibraNet.cpp -o LibraNet.exe -Wall -Wextra

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <regex>
#include <sstream>
#include <iomanip>
#include <optional>
#include <cmath>
#include <ctime>
#include <algorithm>

using std::string;
using std::vector;
using std::unordered_map;
using std::map;
using std::shared_ptr;
using std::make_shared;
using std::optional;
using std::nullopt;
using std::cout;
using std::cerr;
using std::endl;
using std::move;
using std::to_string;
using std::chrono::system_clock;
using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::duration_cast;


class Money {
    int64_t paise_;
public:
    explicit Money(int64_t paise = 0) : paise_(paise) {}
    static Money fromINR(double inr) {
        return Money(static_cast<int64_t>(std::llround(inr * 100.0)));
    }
    double toINR() const { return paise_ / 100.0; }
    int64_t paise() const { return paise_; }
    Money operator+(const Money& o) const { return Money(paise_ + o.paise_); }
    Money operator-(const Money& o) const { return Money(paise_ - o.paise_); }
    Money operator*(int64_t n) const { return Money(paise_ * n); }
    bool operator>(const Money& o) const { return paise_ > o.paise_; }
    bool operator>=(const Money& o) const { return paise_ >= o.paise_; }
    std::string str() const {
        std::ostringstream ss;
        ss.setf(std::ios::fixed);
        ss << std::setprecision(2) << toINR();
        return ss.str() + " INR";
    }
};


struct LibraryException : public std::runtime_error {
    explicit LibraryException(const string& msg) : std::runtime_error(msg) {}
};
struct NotFoundException : public LibraryException { using LibraryException::LibraryException; };
struct InvalidInputException : public LibraryException { using LibraryException::LibraryException; };
struct BorrowException : public LibraryException { using LibraryException::LibraryException; };
struct ReturnException : public LibraryException { using LibraryException::LibraryException; };
struct ArchiveException : public LibraryException { using LibraryException::LibraryException; };
struct ItemNotAvailableException : public BorrowException { using BorrowException::BorrowException; };


enum class AvailabilityStatus { AVAILABLE, BORROWED, RESERVED, MAINTENANCE };
enum class BorrowStatus { ACTIVE, RETURNED, OVERDUE };


class BorrowDuration {
    bool explicitRange_ = false;
    system_clock::time_point end_;
    hours dur_ = hours(0);
public:
    BorrowDuration() = default;

    static BorrowDuration parse(const string& input) {
        string s = input;
        // trim
        auto trim = [](string& t) {
            size_t a = t.find_first_not_of(" \t\n\r");
            size_t b = t.find_last_not_of(" \t\n\r");
            if (a == string::npos) { t.clear(); return; }
            t = t.substr(a, b - a + 1);
        };
        trim(s);
        if (s.empty()) throw InvalidInputException("Empty duration string");

        // Date range: YYYY-MM-DD to YYYY-MM-DD
        std::regex rangeRx(R"((\d{4}-\d{2}-\d{2})\s+to\s+(\d{4}-\d{2}-\d{2}))", std::regex::icase);
        std::smatch m;
        if (std::regex_search(s, m, rangeRx)) {
            std::tm t1 = {}, t2 = {};
            std::istringstream ss1(m[1].str()), ss2(m[2].str());
            ss1 >> std::get_time(&t1, "%Y-%m-%d");
            ss2 >> std::get_time(&t2, "%Y-%m-%d");
            if (ss1.fail() || ss2.fail()) throw InvalidInputException("Invalid date format in range");
            std::time_t tt1 = std::mktime(&t1);
            std::time_t tt2 = std::mktime(&t2);
            if (tt1 == -1 || tt2 == -1) throw InvalidInputException("Failed to convert dates");
            system_clock::time_point p1 = system_clock::from_time_t(tt1);
            system_clock::time_point p2 = system_clock::from_time_t(tt2);
            if (p2 <= p1) throw InvalidInputException("End date must be after start date");
            BorrowDuration bd; bd.explicitRange_ = true; bd.end_ = p2; return bd;
        }

        
        std::regex isoRx(R"(^(P(?:(\d+)D)|PT?(?:(\d+)H))$)", std::regex::icase);
        if (std::regex_search(s, m, isoRx)) {
            if (m[1].matched) {
                int days = std::stoi(m[1].str());
                BorrowDuration bd; bd.dur_ = hours(24 * days); return bd;
            }
            if (m[2].matched) {
                int hrs = std::stoi(m[2].str());
                BorrowDuration bd; bd.dur_ = hours(hrs); return bd;
            }
        }

        
        std::regex nlRx(R"(^\s*(\d+)\s*(days?|d|weeks?|w|hours?|h)\s*$)", std::regex::icase);
        if (std::regex_search(s, m, nlRx)) {
            int num = std::stoi(m[1].str());
            string unit = m[2].str();
            BorrowDuration bd;
            if (std::regex_search(unit, std::regex(R"(week|w)", std::regex::icase))) bd.dur_ = hours(24 * 7 * num);
            else if (std::regex_search(unit, std::regex(R"(day|d)", std::regex::icase))) bd.dur_ = hours(24 * num);
            else if (std::regex_search(unit, std::regex(R"(hour|h)", std::regex::icase))) bd.dur_ = hours(num);
            else throw InvalidInputException("Unsupported duration unit");
            return bd;
        }

        throw InvalidInputException("Unsupported duration format. Supported: '10 days', '2 weeks', 'P14D', 'PT48H', 'YYYY-MM-DD to YYYY-MM-DD'");
    }

    system_clock::time_point computeDueAt(system_clock::time_point borrowStart = system_clock::now()) const {
        if (explicitRange_) return end_;
        return borrowStart + dur_;
    }
};


class User {
    int id_;
    string name_;
    int borrowLimit_;
public:
    explicit User(int id = 0, string name = "", int borrowLimit = 5) : id_(id), name_(move(name)), borrowLimit_(borrowLimit) {}
    int id() const { return id_; }
    string name() const { return name_; }
    int borrowLimit() const { return borrowLimit_; }
};

class Fine {
    int id_;
    int itemId_;
    int userId_;
    Money amount_;
    string reason_;
    system_clock::time_point appliedAt_;
public:
    Fine(int id, int itemId, int userId, Money amount, string reason)
      : id_(id), itemId_(itemId), userId_(userId), amount_(amount), reason_(move(reason)), appliedAt_(system_clock::now()) {}
    int id() const { return id_; }
    Money amount() const { return amount_; }
    string reason() const { return reason_; }
};

/* Playable interface */
class Playable {
public:
    virtual ~Playable() = default;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(std::chrono::duration<long long> pos) = 0;
};

/* Item base class */
class Item {
protected:
    int id_;
    string title_;
    vector<string> authors_;
    AvailabilityStatus status_;
    map<string, string> metadata_;
public:
    Item(int id, string title, vector<string> authors)
      : id_(id), title_(move(title)), authors_(move(authors)), status_(AvailabilityStatus::AVAILABLE) {}
    virtual ~Item() = default;
    int id() const { return id_; }
    string title() const { return title_; }
    AvailabilityStatus status() const { return status_; }
    void setStatus(AvailabilityStatus s) { status_ = s; }
    virtual string typeName() const = 0;
    virtual void validate() const {}
};

/* Book */
class Book : public Item {
    int pageCount_;
public:
    Book(int id, string title, vector<string> authors, int pageCount)
      : Item(id, move(title), move(authors)), pageCount_(pageCount) {
        validate();
    }
    int getPageCount() const { return pageCount_; }
    string typeName() const override { return "Book"; }
    void validate() const override {
        if (pageCount_ <= 0) throw InvalidInputException("Book pageCount must be > 0");
    }
};

/* Audiobook (implements Playable) */
class Audiobook : public Item, public Playable {
    hours playbackDuration_;
    string narrator_;
    bool playing_ = false;
public:
    Audiobook(int id, string title, vector<string> authors, hours playback, string narrator = "")
      : Item(id, move(title), move(authors)), playbackDuration_(playback), narrator_(move(narrator)) {
        validate();
    }
    hours getPlaybackDuration() const { return playbackDuration_; }
    string typeName() const override { return "Audiobook"; }
    void validate() const override {
        if (playbackDuration_ <= hours(0)) throw InvalidInputException("Audiobook duration must be positive");
    }
    void play() override { playing_ = true; cout << "Audiobook[" << id_ << "] play\n"; }
    void pause() override { playing_ = false; cout << "Audiobook[" << id_ << "] pause\n"; }
    void stop() override { playing_ = false; cout << "Audiobook[" << id_ << "] stop\n"; }
    void seek(std::chrono::duration<long long> pos) override {
        if (pos > playbackDuration_) throw InvalidInputException("Seek position out of range");
        cout << "Audiobook[" << id_ << "] seek to " << duration_cast<minutes>(pos).count() << " minutes\n";
    }
};

/* EMagazine */
class EMagazine : public Item {
    int issueNumber_;
    system_clock::time_point issueDate_;
    bool archived_ = false;
public:
    EMagazine(int id, string title, vector<string> authors, int issueNumber, system_clock::time_point issueDate)
      : Item(id, move(title), move(authors)), issueNumber_(issueNumber), issueDate_(issueDate) {
        validate();
    }
    string typeName() const override { return "EMagazine"; }
    int issueNumber() const { return issueNumber_; }
    bool isArchived() const { return archived_; }
    void archiveIssue() {
        if (archived_) throw ArchiveException("Issue already archived");
        archived_ = true;
        setStatus(AvailabilityStatus::MAINTENANCE);
    }
    void validate() const override {
        if (issueNumber_ <= 0) throw InvalidInputException("Issue number must be > 0");
    }
};

/* BorrowRecord */
class BorrowRecord {
    int id_;
    int itemId_;
    int userId_;
    system_clock::time_point borrowAt_;
    system_clock::time_point dueAt_;
    BorrowStatus status_;
public:
    BorrowRecord(int id, int itemId, int userId, system_clock::time_point borrowAt, system_clock::time_point dueAt)
      : id_(id), itemId_(itemId), userId_(userId), borrowAt_(borrowAt), dueAt_(dueAt), status_(BorrowStatus::ACTIVE) {}
    int id() const { return id_; }
    void setId(int id) { id_ = id; }
    int itemId() const { return itemId_; }
    int userId() const { return userId_; }
    system_clock::time_point borrowAt() const { return borrowAt_; }
    system_clock::time_point dueAt() const { return dueAt_; }
    BorrowStatus status() const { return status_; }
    void markReturned() { status_ = BorrowStatus::RETURNED; }
    bool isOverdue() const { return system_clock::now() > dueAt_; }
    int overdueDays() const {
        if (!isOverdue()) return 0;
        auto diff = std::chrono::duration_cast<hours>(system_clock::now() - dueAt_);
        int days = static_cast<int>(diff.count() / 24);
        return std::max(1, days);
    }
};


template<typename T>
class InMemoryRepo {
protected:
    unordered_map<int, shared_ptr<T>> storage_;
    mutable std::mutex mtx_;
public:
    virtual ~InMemoryRepo() = default;

    optional<shared_ptr<T>> findById(int id) const {
        std::lock_guard<std::mutex> l(mtx_);
        auto it = storage_.find(id);
        if (it == storage_.end()) return nullopt;
        return it->second;
    }

    void save(shared_ptr<T> obj, int id) {
        std::lock_guard<std::mutex> l(mtx_);
        storage_[id] = move(obj);
    }

    vector<shared_ptr<T>> all() const {
        std::lock_guard<std::mutex> l(mtx_);
        vector<shared_ptr<T>> res;
        res.reserve(storage_.size());
        for (auto &p : storage_) res.push_back(p.second);
        return res;
    }

    void remove(int id) {
        std::lock_guard<std::mutex> l(mtx_);
        storage_.erase(id);
    }
};


class ItemRepo : public InMemoryRepo<Item> {
public:
    vector<shared_ptr<Item>> findByType(const string& typeName) const {
        vector<shared_ptr<Item>> res;
        std::lock_guard<std::mutex> l(mtx_);
        for (auto &p: storage_) {
            if (p.second->typeName() == typeName) res.push_back(p.second);
        }
        return res;
    }
};

class UserRepo : public InMemoryRepo<User> {};

class BorrowRecordRepo {
    unordered_map<int, shared_ptr<BorrowRecord>> storage_;
    mutable std::mutex mtx_;
    int nextId_ = 1;
public:
    shared_ptr<BorrowRecord> save(shared_ptr<BorrowRecord> rec) {
        std::lock_guard<std::mutex> l(mtx_);
        if (rec->id() == 0) rec->setId(nextId_++);
        storage_[rec->id()] = rec;
        return rec;
    }

    void add(shared_ptr<BorrowRecord> rec) { save(rec); }

    optional<shared_ptr<BorrowRecord>> findActiveByItemId(int itemId) const {
        std::lock_guard<std::mutex> l(mtx_);
        for (auto &p: storage_) {
            auto r = p.second;
            if (r->itemId() == itemId && r->status() == BorrowStatus::ACTIVE) return r;
        }
        return nullopt;
    }

    vector<shared_ptr<BorrowRecord>> findByUserId(int userId) const {
        vector<shared_ptr<BorrowRecord>> res;
        std::lock_guard<std::mutex> l(mtx_);
        for (auto &p: storage_) if (p.second->userId() == userId) res.push_back(p.second);
        return res;
    }
};

class FineRepo {
    unordered_map<int, shared_ptr<Fine>> storage_;
    mutable std::mutex mtx_;
    int nextId_ = 1;
public:
    shared_ptr<Fine> addFine(int itemId, int userId, Money amount, const string& reason) {
        std::lock_guard<std::mutex> l(mtx_);
        int id = nextId_++;
        auto f = make_shared<Fine>(id, itemId, userId, amount, reason);
        storage_[id] = f;
        return f;
    }
    vector<shared_ptr<Fine>> findByUserId(int /*userId*/) const {
        vector<shared_ptr<Fine>> res;
        std::lock_guard<std::mutex> l(mtx_);
        for (auto &p: storage_) res.push_back(p.second);
        return res;
    }
};


class LibraryService {
    ItemRepo& items_;
    UserRepo& users_;
    BorrowRecordRepo& records_;
    FineRepo& fines_;
    Money dailyFineRate_;
public:
    LibraryService(ItemRepo& items, UserRepo& users, BorrowRecordRepo& records, FineRepo& fines, Money dailyFineRate)
      : items_(items), users_(users), records_(records), fines_(fines), dailyFineRate_(dailyFineRate) {}

    void borrowItem(int userId, int itemId, const string& durationStr) {
        auto userOpt = users_.findById(userId);
        if (!userOpt) throw NotFoundException("User not found");
        auto itemOpt = items_.findById(itemId);
        if (!itemOpt) throw NotFoundException("Item not found");
        auto item = *itemOpt;
        if (item->status() != AvailabilityStatus::AVAILABLE) throw ItemNotAvailableException("Item not available for borrowing");

        BorrowDuration bd = BorrowDuration::parse(durationStr);
        system_clock::time_point now = system_clock::now();
        system_clock::time_point due = bd.computeDueAt(now);
        if (due <= now) throw InvalidInputException("Computed due date must be in the future");

        item->setStatus(AvailabilityStatus::BORROWED);
        auto rec = make_shared<BorrowRecord>(0, itemId, userId, now, due);
        records_.add(rec);
        cout << "Borrowed item " << itemId << " by user " << userId << ". Due at epoch:" << duration_cast<seconds>(due.time_since_epoch()).count() << "\n";
    }

    void returnItem(int userId, int itemId) {
        auto userOpt = users_.findById(userId);
        if (!userOpt) throw NotFoundException("User not found");
        auto itemOpt = items_.findById(itemId);
        if (!itemOpt) throw NotFoundException("Item not found");
        auto item = *itemOpt;

        auto recOpt = records_.findActiveByItemId(itemId);
        if (!recOpt) throw ReturnException("No active borrow record for item");
        auto rec = *recOpt;
        if (rec->userId() != userId) throw ReturnException("Borrow record user mismatch");

        int overdueDays = rec->overdueDays();
        if (overdueDays > 0) {
            Money fineAmount = dailyFineRate_ * overdueDays;
            auto fine = fines_.addFine(itemId, userId, fineAmount, "Overdue by " + to_string(overdueDays) + " days");
            cout << "Applied fine " << fine->amount().str() << " for user " << userId << " on item " << itemId << "\n";
        } else {
            cout << "No fine. Item returned on time.\n";
        }

        rec->markReturned();
        item->setStatus(AvailabilityStatus::AVAILABLE);
    }

    vector<shared_ptr<Item>> searchByType(const string& typeName) {
        return items_.findByType(typeName);
    }

    void archiveMagazine(int itemId) {
        auto itemOpt = items_.findById(itemId);
        if (!itemOpt) throw NotFoundException("Item not found");
        auto it = *itemOpt;
        auto mag = std::dynamic_pointer_cast<EMagazine>(it);
        if (!mag) throw ArchiveException("Item is not an EMagazine");
        mag->archiveIssue();
        cout << "Archived magazine item " << itemId << "\n";
    }
};


int main() {
    ItemRepo itemRepo;
    UserRepo userRepo;
    BorrowRecordRepo recordRepo;
    FineRepo fineRepo;

    
    auto book1 = make_shared<Book>(101, "Design Patterns", vector<string>{"Gamma","Helm","Johnson","Vlissides"}, 395);
    auto audio1 = make_shared<Audiobook>(102, "Clean Code (Audio)", vector<string>{"Robert C. Martin"}, hours(9), "Narrator A");
    auto mag1 = make_shared<EMagazine>(103, "Tech Monthly", vector<string>{"Editorial Team"}, 15, system_clock::now());

    itemRepo.save(book1, book1->id());
    itemRepo.save(audio1, audio1->id());
    itemRepo.save(mag1, mag1->id());

    auto user1 = make_shared<User>(201, "Somen Mishra", 5);
    userRepo.save(user1, user1->id());

    LibraryService lib(itemRepo, userRepo, recordRepo, fineRepo, Money::fromINR(10.0));

  
    while (true) {
        cout << "\n--- LibraNet Menu ---\n";
        cout << "1. Borrow Item\n";
        cout << "2. Return Item\n";
        cout << "3. Archive Magazine\n";
        cout << "4. Search by Type\n";
        cout << "5. Add Item\n";
        cout << "6. Add User\n";
        cout << "0. Exit\n";
        cout << "Choice: ";

        int choice;
        if (!(std::cin >> choice)) break;
        if (choice == 0) break;

        try {
            if (choice == 1) {
                int userId, itemId;
                string duration;
                cout << "Enter userId itemId duration (e.g., 201 101 10 days): ";
                std::cin >> userId >> itemId;
                std::getline(std::cin, duration);
                if (duration.empty()) std::getline(std::cin, duration);
                lib.borrowItem(userId, itemId, duration);

            } else if (choice == 2) {
                int userId, itemId;
                cout << "Enter userId itemId: ";
                std::cin >> userId >> itemId;
                lib.returnItem(userId, itemId);

            } else if (choice == 3) {
                int itemId;
                cout << "Enter magazine itemId: ";
                std::cin >> itemId;
                lib.archiveMagazine(itemId);

            } else if (choice == 4) {
                string type;
                cout << "Enter type (Book/Audiobook/EMagazine): ";
                std::cin >> type;
                auto res = lib.searchByType(type);
                if (res.empty()) cout << "No items found of type " << type << "\n";
                for (auto &it : res) cout << "Found: " << it->id() << " - " << it->title() << "\n";

            } else if (choice == 5) {
                cout << "Select Item Type: 1=Book, 2=Audiobook, 3=EMagazine: ";
                int t; std::cin >> t;

                int id; string title; int issueNum, pages; int hoursDur;
                cout << "Enter itemId: "; std::cin >> id;
                cout << "Enter title: ";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::getline(std::cin, title);

                if (t == 1) {
                    cout << "Enter page count: ";
                    std::cin >> pages;
                    auto book = make_shared<Book>(id, title, vector<string>{"Unknown"}, pages);
                    itemRepo.save(book, book->id());
                    cout << "Book added: " << title << "\n";
                } else if (t == 2) {
                    cout << "Enter duration (hours): ";
                    std::cin >> hoursDur;
                    auto audio = make_shared<Audiobook>(id, title, vector<string>{"Unknown"}, hours(hoursDur), "Narrator");
                    itemRepo.save(audio, audio->id());
                    cout << "Audiobook added: " << title << "\n";
                } else if (t == 3) {
                    cout << "Enter issue number: ";
                    std::cin >> issueNum;
                    auto mag = make_shared<EMagazine>(id, title, vector<string>{"Editorial"}, issueNum, system_clock::now());
                    itemRepo.save(mag, mag->id());
                    cout << "Magazine added: " << title << "\n";
                } else {
                    cout << "Invalid type!\n";
                }

            } else if (choice == 6) {
                int userId, limit;
                string name;
                cout << "Enter userId: "; std::cin >> userId;
                cout << "Enter name: ";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::getline(std::cin, name);
                cout << "Enter borrow limit: "; std::cin >> limit;
                auto user = make_shared<User>(userId, name, limit);
                userRepo.save(user, user->id());
                cout << "User added: " << name << " (id=" << userId << ")\n";
            }

        } catch (const std::exception& e) {
            cerr << "Error: " << e.what() << "\n";
        }
    }

    cout << "Exiting LibraNet...\n";
    return 0;
}
