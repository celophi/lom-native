#include "common/text_case.h"

namespace lom::launcher {

std::string ascii_upper(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') {
            c = char(c - 'a' + 'A');
        }
    }

    return text;
}

}
