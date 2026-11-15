#include "stack.h"
#include "portable.h"

namespace logic {
Stack::Stack() : topIndex(-1) {
}

bool Stack::push(const gmath::GridEntry &entry) {
    if (topIndex < MAX_SIZE - 1) {
        stackArray[++topIndex] = entry;
        return true;
    }
    return false;
}

void Stack::pop() {
    if (!empty())
        topIndex--;
}

const gmath::GridEntry &Stack::top() const {
    if (!empty())
        return stackArray[topIndex];
    throw std::out_of_range("Empty Dotio path");
}

int Stack::size() const {
    return topIndex + 1;
}

int Stack::capacity() const {
    return MAX_SIZE;
}

bool Stack::empty() const {
    return topIndex == -1;
}

void Stack::clear() {
    topIndex = -1;
}

gmath::GridEntry Stack::get(int index) const {
    if (index >= 0 && index <= topIndex)
        return stackArray[index];
    throw std::out_of_range("Dotio path index");
}
} // namespace logic
