#  LibraNet – Library Management System (C++17)

LibraNet is a simple **console-based library management system** written in C++17.  
It supports managing **Books, Audiobooks, and E-Magazines**, borrowing and returning items, applying fines for overdue returns, and more.  

This project demonstrates **OOP design in C++**, exception handling, in-memory repositories, and interactive menus.

---

##  Features

- **Item Types**
  -  **Book** (with page count)
  -  **Audiobook** (with playback duration & narrator, supports play/pause/seek)
  -  **E-Magazine** (with issue number, archiving support)

- **User Management**
  - Add new users with borrow limits
  - Track active borrow records per user
  - View user fines and overdue items

- **Borrowing System**
  - Borrow items using durations like:
    - `"10 days"`, `"2 weeks"`, `"48h"`
    - ISO-8601 style: `P7D`, `PT48H`
    - Date range: `2025-09-19 to 2025-09-22`
  - Return items (fines applied if overdue)
  - Renew active (non-overdue) borrows
  - Reserve items (and cancel reservations)
  - Auto fine calculation (configurable daily fine rate, default = ₹10/day)

- **Search**
  - Find items by type (`Book`, `Audiobook`, `EMagazine`)

- **Extras**
  - Human-readable due dates
  - Archiving of magazines (sets them to maintenance mode)
  - Thread-safe in-memory repositories

---

## Installation & Compilation

### Requirements
- A C++17 compatible compiler (e.g., `g++`, `clang++`, MSVC)
- Linux, macOS, or Windows

### Build
```bash
g++ -std=c++17 LibraNet_extended.cpp -o LibraNet.exe -Wall -Wextra
