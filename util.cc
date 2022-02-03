#include <string>
#include <util.hh>

string camelcase(string s)
{
  const char *sp = s.c_str();
  string r;
  if(sp == NULL || *sp == 0)
    return r;
  for( ;*sp != '_' && *sp != 0; sp++)
    r += *sp;
  if(*sp == '_')
    {
      sp++;
      if(*sp != 0)
	r += toupper(*sp);
      return r + camelcase(++sp);
    }
  return r;
}
