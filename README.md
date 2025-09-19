LibraNet - Modern C++ Library Management System

LibraNet is a console-based library management system written in C++17.  
It supports managing Books, Audiobooks, and E-Magazines, with borrowing, returning, fines and archiving features.  

This project demonstrates object-oriented design, exception handling, repository patterns, and chrono/date handling in C++.

Features
- User Management
  - Add users with custom borrowing limits
  - Track borrowed items per user
- Item Management
  - Supports Books, Audiobooks (with playback), and E-Magazines
  - Metadata and validation included
- Borrowing and Returning
  - Borrow items with durations such as "10 days", "P14D", "2025-01-01 to 2025-01-15"
  - Automatic fine calculation for overdue items
- E-Magazine Archiving
  - Archive issues and mark them as unavailable
- Search Functionality
  - Search items by type (Book, Audiobook, EMagazine)
- Error Handling
  - Custom exception hierarchy for clear error messages

---

Technologies and Concepts Used
- C++17 STL 
- Object-Oriented Programming
  - Inheritance (Item, Book, Audiobook, EMagazine)
  - Interfaces (Playable)
  - Polymorphism
- Repository Pattern
  - InMemoryRepo for storing items and users
  - Thread-safe with std::mutex
- Custom Classes
  - Money class for handling INR with precision
  - BorrowDuration for flexible time parsing
