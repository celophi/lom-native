#ifndef LOM_LAUNCHER_TEXT_CASE_H
#define LOM_LAUNCHER_TEXT_CASE_H

#include <string>

namespace lom::launcher {

/**
 * @brief Upper-case ASCII letters, leaving other bytes (including UTF-8) unchanged.
 * @param text Text to convert.
 * @return Converted copy.
 */
std::string ascii_upper(std::string text);

}

#endif
