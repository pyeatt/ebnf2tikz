#include <string>
#include <util.hh>

/* Spell out a digit as a word so LaTeX command names remain all-letters */
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
