#ifndef STACK_H
#define STACK_H

#include "dotio/math/gmath.h"

namespace dotio {
class Stack {
  private:
    int topIndex;
    static const int MAX_SIZE = 50;
    dotio::math::GridEntry stackArray[MAX_SIZE];

  public:
    Stack();
    bool push(const dotio::math::GridEntry &entry);
    void pop();
    const dotio::math::GridEntry &top() const;
    int size() const;
    int capacity() const;
    bool empty() const;
    void clear();
    dotio::math::GridEntry get(int index) const;
};
} // namespace dotio

#endif // STACK_H
