#include <util/ctype.h>

int isdigit(int ch)
{
  return ((ch>='0') && (ch<='9'));
}

int isalpha(int ch)
{
  return (((ch>='A') && (ch<='Z'))
          || ((ch>='a') && (ch<='z'))
          );
}

int tolower(int ch)
{
  if ((ch>='A') && (ch<='Z')) return ch+32;
  return ch;
}

int toupper(int ch)
{
  if ((ch>='a') && (ch<='z')) return ch-32;
  return ch;
}
