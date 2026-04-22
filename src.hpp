#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <iostream>

class RefCellError : public std::runtime_error {
public:
    explicit RefCellError(const std::string& message) : std::runtime_error(message) {}
    virtual ~RefCellError() = default;
};

class BorrowError : public RefCellError {
public:
    explicit BorrowError(const std::string& message) : RefCellError(message) {}
};

class BorrowMutError : public RefCellError {
public:
    explicit BorrowMutError(const std::string& message) : RefCellError(message) {}
};

class DestructionError : public RefCellError {
public:
    explicit DestructionError(const std::string& message) : RefCellError(message) {}
};

template <typename T>
class RefCell {
private:
    T value;
    mutable int imm_count = 0;
    mutable bool has_mut = false;

public:
    class Ref;
    class RefMut;

    explicit RefCell(const T& initial_value) : value(initial_value) {}
    explicit RefCell(T&& initial_value) : value(std::move(initial_value)) {}

    RefCell(const RefCell&) = delete;
    RefCell& operator=(const RefCell&) = delete;
    RefCell(RefCell&&) = delete;
    RefCell& operator=(RefCell&&) = delete;

    Ref borrow() const {
        if (has_mut) {
            throw BorrowError("immutable borrow while mutable exists");
        }
        imm_count += 1;
        return Ref(this, &value);
    }

    std::optional<Ref> try_borrow() const {
        if (has_mut) return std::nullopt;
        imm_count += 1;
        return Ref(this, &value);
    }

    RefMut borrow_mut() {
        if (has_mut || imm_count > 0) {
            throw BorrowMutError("mutable borrow while borrowed");
        }
        has_mut = true;
        return RefMut(this, &value);
    }

    std::optional<RefMut> try_borrow_mut() {
        if (has_mut || imm_count > 0) return std::nullopt;
        has_mut = true;
        return RefMut(this, &value);
    }

    class Ref {
    private:
        const RefCell<T>* owner = nullptr;
        const T* ptr = nullptr;
        void release() {
            if (owner) {
                owner->imm_count -= 1;
            }
            owner = nullptr;
            ptr = nullptr;
        }
    public:
        Ref() = default;
        Ref(const RefCell<T>* o, const T* p) : owner(o), ptr(p) {}
        ~Ref() { release(); }

        const T& operator*() const { return *ptr; }
        const T* operator->() const { return ptr; }

        Ref(const Ref& other) : owner(other.owner), ptr(other.ptr) {
            if (owner) owner->imm_count += 1;
        }
        Ref& operator=(const Ref& other) {
            if (this == &other) return *this;
            release();
            owner = other.owner;
            ptr = other.ptr;
            if (owner) owner->imm_count += 1;
            return *this;
        }

        Ref(Ref&& other) noexcept : owner(other.owner), ptr(other.ptr) {
            other.owner = nullptr;
            other.ptr = nullptr;
        }
        Ref& operator=(Ref&& other) noexcept {
            if (this == &other) return *this;
            release();
            owner = other.owner;
            ptr = other.ptr;
            other.owner = nullptr;
            other.ptr = nullptr;
            return *this;
        }
    };

    class RefMut {
    private:
        RefCell<T>* owner = nullptr;
        T* ptr = nullptr;
        void release() {
            if (owner) {
                owner->has_mut = false;
            }
            owner = nullptr;
            ptr = nullptr;
        }
    public:
        RefMut() = default;
        RefMut(RefCell<T>* o, T* p) : owner(o), ptr(p) {}
        ~RefMut() { release(); }

        T& operator*() { return *ptr; }
        T* operator->() { return ptr; }

        RefMut(const RefMut&) = delete;
        RefMut& operator=(const RefMut&) = delete;

        RefMut(RefMut&& other) noexcept : owner(other.owner), ptr(other.ptr) {
            other.owner = nullptr;
            other.ptr = nullptr;
        }
        RefMut& operator=(RefMut&& other) noexcept {
            if (this == &other) return *this;
            release();
            owner = other.owner;
            ptr = other.ptr;
            other.owner = nullptr;
            other.ptr = nullptr;
            return *this;
        }
    };

    ~RefCell() {
        if (imm_count != 0 || has_mut) {
            throw DestructionError("RefCell destroyed while borrowed");
        }
    }
};
