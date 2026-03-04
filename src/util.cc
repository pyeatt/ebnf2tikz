/**
 * @file util.cc
 * @brief Implementation of utility functions.
 *
 * @see util.hh
 */

#include <string>
#include <util.hh>

using namespace std;

/**
 * @brief Spell out a single digit character as an English word.
 *
 * Used by camelcase() to convert digits to words so that the
 * resulting string is a valid LaTeX command name (all letters).
 *
 * @param c A character; if '0'-'9', returns the word form.
 * @return The digit word (e.g., "Zero") or the character as a string.
 */
static string digitWord(char c)
{
  static const char *words[] = {
    "Zero","One","Two","Three","Four",
    "Five","Six","Seven","Eight","Nine"
  };
  if(c >= '0' && c <= '9')
    return words[c - '0'];
  return string(1, c);
}

/** @brief Convert underscore-separated name to camelCase. @see camelcase() in util.hh */
string camelcase(string s)
{
  const char *sp = s.c_str();
  string r;
  if(*sp == 0)
    return r;
  for( ; *sp != '_' && *sp != 0; sp++)
    {
      if(*sp >= '0' && *sp <= '9')
	r += digitWord(*sp);
      else
	r += *sp;
    }
  if(*sp == '_')
    {
      sp++;
      if(*sp != 0)
	{
	  if(*sp >= '0' && *sp <= '9')
	    r += digitWord(*sp);
	  else
	    r += toupper(*sp);
	}
      return r + camelcase(++sp);
    }
  return r;
}
