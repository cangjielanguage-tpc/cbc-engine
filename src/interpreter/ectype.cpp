#include "ectype.h"

void Interpretation::Ectype::VisitReferences(std::function<void(Value::Reference*)> visitor) {
    for (int i = 0; i < IReg::COUNT; i++) {
        if (iregMarks[i] == Mark::REFERENCE) {
            visitor(&iregs[i].reference);
        }
    }
}
