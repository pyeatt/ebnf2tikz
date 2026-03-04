/**
 * @file util.hh
 * @brief Utility function declarations.
 */

#ifndef UTIL_HH
#define UTIL_HH

#include <string>

/**
 * @brief Convert an underscore-separated name to camelCase for LaTeX commands.
 *
 * Transforms names like @c "my_production_3" into @c "myProductionThree".
 * Digits are spelled out as words (0→Zero, 1→One, etc.) because LaTeX
 * command names must contain only letters.
 *
 * @param s The underscore-separated input string.
 * @return The camelCase version suitable for a LaTeX command name.
 */
std::string camelcase(std::string s);

#endif
