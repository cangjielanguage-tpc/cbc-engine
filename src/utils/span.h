#ifndef SPAN_H
#define SPAN_H

#include "vector"

namespace Collections {

template <typename T>
class Span {
public:
    Span(size_t size) : vector(size) {}
    Span(std::vector<T> v) : vector(v) {}

    inline T& operator[](size_t idx) const {
        return vector[idx];
    }

    inline size_t Size() const {
        return vector.size();
    }

    bool Equals(Span const& another) const {
        if (Size() != another.Size()) {
            return false;
        }
        auto size = Size();
        for (size_t i = 0; i < size; i++) {
            if (vector[i] != another.vector[i]) {
                return false;
            }
        }
        return true;
    }
private:
    std::vector<T> vector;
};

}

#endif // SPAN_H
