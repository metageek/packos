 #include <util/stream.h>
#include <util/ctype.h>
#include <packos/arch.h>

static unsigned long long udiv64by10(unsigned long long a,
                                     byte* mod)
{
  typedef union {
    unsigned long long full;
    struct {
#ifdef PACKOS_BIG_ENDIAN
      unsigned long high,low;
#else
      unsigned long low,high;
#endif
    } halves;
  } U;

  unsigned long resHigh=0,resLow=0;
  unsigned long aHigh=0,aLow=0;
  int i;
  uint16_t carry=0;

  {
    U u;
    u.full=a;
    aHigh=u.halves.high;
    aLow=u.halves.low;
  }

  for (i=7; i>=0; i--)
    {
      byte b=aHigh>>24;
      aHigh<<=8;
      aHigh|=(aLow>>24);
      aLow<<=8;
      
      uint16_t s=b;
      s+=carry;

      carry=(s%10)<<8;
      resHigh<<=8;
      resHigh|=(resLow>>24);
      resLow<<=8;

      {
        unsigned long resLowNew=resLow+s/10;
        if (resLowNew<resLow)
          resHigh++;
        resLow=resLowNew;
      }
    }

  carry>>=8;
  if (mod)
    (*mod)=carry;

  {
    U u;
    u.halves.high=resHigh;
    u.halves.low=resLow;
    return u.full;
  }
}

static int layoutIntUnsigned(Stream stream,
                             unsigned long long i,
                             int fieldWidth,
                             int precision,
                             bool zeroPadded,
                             PackosError* error)
{
  if (i)
    {
      byte mod;
      unsigned long long nextI=udiv64by10(i,&mod);
      char ch='0'+mod;
      if (layoutIntUnsigned(stream,nextI,
                            fieldWidth-1,precision-1,zeroPadded,
                            error)<0)
        return -1;
      return StreamWrite(stream,&ch,1,error);
    }

  {
    char ch='0';
    while (precision>0)
      {
        if (StreamWrite(stream,&ch,1,error)<0)
          return -1;
        precision--;
        fieldWidth--;
      }
  }

  {
    char ch=(zeroPadded ? '0' : ' ');
    while (fieldWidth>0)
      {
        if (StreamWrite(stream,&ch,1,error)<0)
          return -1;
        fieldWidth--;
      }
  }

  return 0;
}

static int layoutIntSigned(Stream stream,
                           long long i,
                           int fieldWidth,
                           int precision,
                           bool zeroPadded,
                           PackosError* error)
{
  if (i<0)
    {
      if (StreamWrite(stream,"-",1,error)<0)
        return -1;
      i=-i;
    }

  return layoutIntUnsigned(stream,i,fieldWidth,precision,zeroPadded,error);
}

static int layoutInt(Stream stream,
                     long long i,
                     int fieldWidth,
                     int precision,
                     bool zeroPadded,
                     bool isUnsigned,
                     PackosError* error)
{
  if ((!i) && precision)
    return StreamWrite(stream,"0",1,error);

  if (isUnsigned)
    return layoutIntUnsigned(stream,i,fieldWidth,precision,zeroPadded,error);
  else
    return layoutIntSigned(stream,i,fieldWidth,precision,zeroPadded,error);
}

static int layoutOctUnsigned(Stream stream,
                             unsigned long long i,
                             int precision,
                             bool zeroPadded,
                             PackosError* error)
{
  if (i)
    {
      char ch='0'+(i&7ULL);
      if (layoutOctUnsigned(stream,i>>3,precision-1,zeroPadded,error)<0)
        return -1;
      return StreamWrite(stream,&ch,1,error);
    }

  if (precision>0)
    {
      char ch=(zeroPadded ? '0' : ' ');
      while (precision>0)
        {
          if (StreamWrite(stream,&ch,1,error)<0)
            return -1;
          precision--;
        }
    }

  return 0;
}

static int layoutOctSigned(Stream stream,
                           long long i,
                           int precision,
                           bool zeroPadded,
                           PackosError* error)
{
  if (i<0)
    {
      if (StreamWrite(stream,"-",1,error)<0)
        return -1;
      i=-i;
    }

  return layoutOctUnsigned(stream,i,precision,zeroPadded,error);
}

static int layoutOct(Stream stream,
                     long long i,
                     int precision,
                     bool zeroPadded,
                     bool isUnsigned,
                     PackosError* error)
{
  if ((!i) && precision)
    return StreamWrite(stream,"0",1,error);

  if (isUnsigned)
    return layoutOctUnsigned(stream,i,zeroPadded,precision,error);
  else
    return layoutOctSigned(stream,i,zeroPadded,precision,error);
}

static int layoutHexUnsigned(Stream stream,
                             unsigned long long i,
                             int precision,
                             bool zeroPadded,
                             bool asUpper,
                             PackosError* error)
{
  if (i)
    {
      char ch;
      unsigned long long nibble=(i&15ULL);
      if (nibble<10)
        ch='0'+nibble;
      else
        ch=(asUpper ? 'A' : 'a')+(nibble-10);

      if (layoutHexUnsigned(stream,i>>4,precision-1,
                            zeroPadded,asUpper,error)<0)
        return -1;
      return StreamWrite(stream,&ch,1,error);
    }

  if (precision>0)
    {
      char ch=(zeroPadded ? '0' : ' ');
      while (precision>0)
        {
          if (StreamWrite(stream,&ch,1,error)<0)
            return -1;
          precision--;
        }
    }

  return 0;
}

static int layoutHexSigned(Stream stream,
                           long long i,
                           int precision,
                           bool zeroPadded,
                           bool asUpper,
                           PackosError* error)
{
  if (i<0)
    {
      if (StreamWrite(stream,"-",1,error)<0)
        return -1;
      i=-i;
    }

  return layoutHexUnsigned(stream,i,precision,zeroPadded,asUpper,error);
}

static int layoutHex(Stream stream,
                     long long i,
                     int precision,
                     bool zeroPadded,
                     bool asUpper,
                     bool isUnsigned,
                     PackosError* error)
{
  if ((!i) && precision)
    return StreamWrite(stream,"0",1,error);

  if (isUnsigned)
    return layoutHexUnsigned(stream,i,precision,zeroPadded,asUpper,error);
  else
    return layoutHexSigned(stream,i,precision,zeroPadded,asUpper,error);
}

static int layoutExp(Stream stream,
                     long double x,
                     int fieldWidth,
                     int precision,
                     bool zeroPadded,
                     bool asUpper,
                     PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}

static int layoutDouble(Stream stream,
                        long double x,
                        int fieldWidth,
                        int precision,
                        PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}

static int layoutDoubleFlex(Stream stream,
                            long double x,
                            int fieldWidth,
                            int precision,
                            bool zeroPadded,
                            bool asUpper,
                            PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}

static int layoutDoubleHex(Stream stream,
                           long double x,
                           int fieldWidth,
                           int precision,
                           bool zeroPadded,
                           bool asUpper,
                           PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}

int UtilVPrintfStream(Stream stream,
                      PackosError* error,
                      const char* format,
                      va_list ap)
{
  char ch;
  bool alternateForm,zeroPadded,leftAdjusted,signedBlank,alwaysSigned,
    groupThousands,localeDigits;
  int fieldWidth=-1,precision=-1;

  enum {
    lengthDefault=0,
    lengthHalfHalf,
    lengthHalf,
    lengthLong,
    lengthLongLong,
    lengthLongDouble,
    lengthIntMax,
    lengthSize,
    lengthPtrDiff
  } lengthModifier;

  enum {
    convInt,
    convOctal,
    convUnsigned,
    convHexLower,
    convHexUpper,
    convExpLower,
    convExpUpper,
    convDouble,
    convDoubleFlexLower,
    convDoubleFlexUpper,
    convDoubleHexLower,
    convDoubleHexUpper,
    convChar,
    convString,
    convPointer
  } convType;

 CHAR_START:
  alternateForm=zeroPadded=leftAdjusted=signedBlank
    =alwaysSigned=groupThousands=localeDigits
    =false;

  ch=*(format++);
  if (!ch) return 0;
  if (ch!='%')
    {
      if (StreamWrite(stream,&ch,1,error)<0)
        return -1;
      goto CHAR_START;
    }

 FLAGS:
  ch=*(format++);
  switch (ch)
    {
    case 0:
      *error=packosErrorInvalidArg;
      return -1;

    case '%':
      if (StreamWrite(stream,format,1,error)<0)
        return -1;
      goto CHAR_START;

    case '#':
      alternateForm=true;
      break;

    case '0':
      zeroPadded=true;
      break;

    case '-':
      leftAdjusted=true;
      break;

    case ' ':
      signedBlank=true;
      break;

    case '+':
      alwaysSigned=true;
      break;

    case '\'':
      groupThousands=true;
      break;

    case 'I':
      localeDigits=true;
      break;

    default:
      goto FIELD_WIDTH;
    }

  goto FLAGS;

 FIELD_WIDTH:
  if (!isdigit(ch))
    {
      fieldWidth=-1;
      goto PRECISION;
    }
  else
    fieldWidth=0;

  while (isdigit(ch))
    {
      fieldWidth*=10;
      fieldWidth+=(ch-'0');
      ch=*(format++);
    }

  if (!ch)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

 PRECISION:
  if (ch!='.')
    {
      precision=-1;
      goto LENGTH_MODIFIER;
    }
  else
    precision=0;

  while (isdigit(ch))
    {
      precision*=10;
      precision+=(ch-'0');
      ch=*(format++);
    }

  if (!ch)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

 LENGTH_MODIFIER:
  switch (ch)
    {
    case 'h':
      if ((*format)=='h')
        {
          ch=*(format++);
          lengthModifier=lengthHalfHalf;
        }
      else
        lengthModifier=lengthHalf;
      break;

    case 'l':
      if ((*format)=='l')
        {
          format++;
          ch=*(format++);
          lengthModifier=lengthLongLong;
        }
      else
        lengthModifier=lengthLong;
      break;

    case 'L':
      ch=*(format++);
      lengthModifier=lengthLongDouble;
      break;

    case 'q':
      ch=*(format++);
      lengthModifier=lengthLongLong;
      break;

    case 'j':
      ch=*(format++);
      lengthModifier=lengthIntMax;
      break;

    case 'z':
      ch=*(format++);
      lengthModifier=lengthSize;
      break;

    case 't':
      ch=*(format++);
      lengthModifier=lengthPtrDiff;
      break;

    default:
      lengthModifier=lengthDefault;
      break;
    }

  /* CONVERSION_SPECIFIER:*/
  switch (ch)
    {
    case 'd':
    case 'i':
      convType=convInt;
      break;

    case 'o':
      convType=convOctal;
      break;

    case 'u':
      convType=convUnsigned;
      break;

    case 'x':
      convType=convHexLower;
      break;

    case 'X':
      convType=convHexUpper;
      break;

    case 'e':
      convType=convExpLower;
      break;

    case 'E':
      convType=convExpUpper;
      break;

    case 'f':
    case 'F':
      convType=convDouble;
      break;

    case 'g':
      convType=convDoubleFlexLower;
      break;

    case 'G':
      convType=convDoubleFlexUpper;
      break;

    case 'a':
      convType=convDoubleHexLower;
      break;

    case 'A':
      convType=convDoubleHexUpper;
      break;

    case 'c':
      convType=convChar;
      break;

    case 'p':
      convType=convPointer;
      break;

    case 's':
      convType=convString;
      break;

    case 'C':
      convType=convChar;
      lengthModifier=lengthLong;
      break;

    case 'S':
      convType=convString;
      lengthModifier=lengthLong;
      break;

    default:
      goto CHAR_START;
    }

  /* FORMAT_PARSED:*/
  {
    int intArg;
    short shortArg;
    long longArg;
    long long longLongArg;
    SizeType sizeArg;
    PtrDiffType ptrDiffArg;

    double doubleArg;
    long double longDoubleArg;
    char charArg;
    const char* stringArg;
    const void* pointerArg;


    if (lengthModifier==lengthIntMax)
      lengthModifier=lengthLongLong;

    switch (convType)
      {
      case convInt:
      case convOctal:
      case convUnsigned:
      case convHexLower:
      case convHexUpper:
        switch (lengthModifier)
          {
          case lengthHalfHalf:
            charArg=(char)(va_arg(ap,int));
            longLongArg=(long long)charArg;
            break;

          case lengthHalf:
            shortArg=(short)(va_arg(ap,int));
            longLongArg=(long long)shortArg;
            break;

          case lengthDefault:
            intArg=va_arg(ap,int);
            longLongArg=(long long)intArg;
            break;

          case lengthLong:
            longArg=va_arg(ap,long);
            longLongArg=(long long)longArg;
            break;

          case lengthLongLong:
            longLongArg=va_arg(ap,long long);
            break;

          case lengthSize:
            sizeArg=va_arg(ap,SizeType);
            longLongArg=(long long)sizeArg;
            break;

          case lengthPtrDiff:
            ptrDiffArg=va_arg(ap,PtrDiffType);
            longLongArg=(long long)ptrDiffArg;
            break;

          case lengthIntMax:
            *error=packosErrorUnknownError;
            return -1;

          case lengthLongDouble:
            *error=packosErrorInvalidArg;
            return -1;
          }
        break;

      case convExpLower:
      case convExpUpper:
      case convDouble:
      case convDoubleFlexLower:
      case convDoubleFlexUpper:
      case convDoubleHexLower:
      case convDoubleHexUpper:
        switch (lengthModifier)
          {
          case lengthHalf:
          case lengthHalfHalf:
          case lengthLong:
          case lengthLongLong:
          case lengthIntMax:
          case lengthSize:
          case lengthPtrDiff:
            *error=packosErrorInvalidArg;
            return -1;

          case lengthDefault:
            doubleArg=va_arg(ap,double);
            longDoubleArg=(long double)doubleArg;
            break;

          case lengthLongDouble:
            longDoubleArg=va_arg(ap,long double);
            break;
          }

      case convChar:
        switch (lengthModifier)
          {
          case lengthDefault:
            charArg=(char)va_arg(ap,int);
            break;

          case lengthLong:
          default:
            goto CHAR_START;
          }
        break;

      case convString:
        switch (lengthModifier)
          {
          case lengthDefault:
            stringArg=va_arg(ap,char*);
            break;

          case lengthLong:
          default:
            goto CHAR_START;
          }
        break;

      case convPointer:
        pointerArg=va_arg(ap,void*);
#ifdef PACKOS_64BIT
        lengthModifier=lengthLongLong;
        longLongArg=(long long)pointerArg;
#else
        longArg=(long)pointerArg;
        longLongArg=(long long)longArg;
        lengthModifier=lengthLong;
#endif
        alternateForm=true;
        convType=convHexLower;
        zeroPadded=true;
        break;
      }

    switch (convType)
      {
      case convInt:
      case convUnsigned:
        if (precision<0) precision=1;
        if (layoutInt(stream,longLongArg,
                      fieldWidth,precision,zeroPadded,(convType==convUnsigned),
                      error
                      )<0)
          return -1;
        break;

      case convOctal:
        if (layoutOct(stream,longLongArg,precision,
                      zeroPadded,(convType==convUnsigned),error)<0)
          return -1;
        break;

      case convHexLower:
      case convHexUpper:
        if (alternateForm)
          {
            if (StreamWrite(stream,"0x",2,error)<0)
              return -1;
          }
        if (layoutHex(stream,
                      (
                       lengthModifier=lengthLongLong
                       ? longLongArg
                       : longArg
                       ),
                      precision,zeroPadded,(convType==convHexUpper),true,
                      error)<0)
          return -1;
        break;

      case convExpLower:
      case convExpUpper:
        if (layoutExp(stream,longDoubleArg,
                      fieldWidth,precision,zeroPadded,(convType==convExpUpper),
                      error
                      )<0)
          return -1;
        break;

      case convDouble:
        if (layoutDouble(stream,longDoubleArg,
                         fieldWidth,precision,error)<0)
          return -1;
        break;

      case convDoubleFlexLower:
      case convDoubleFlexUpper:
        if (layoutDoubleFlex(stream,longDoubleArg,
                             fieldWidth,precision,zeroPadded,
                             (convType==convDoubleFlexUpper),
                             error
                             )<0)
          return -1;
        break;

      case convDoubleHexLower:
      case convDoubleHexUpper:
        if (layoutDoubleHex(stream,longDoubleArg,
                            fieldWidth,precision,zeroPadded,
                            (convType==convDoubleHexUpper),
                            error
                            )<0)
          return -1;
        break;

      case convChar:
        if (StreamWrite(stream,&charArg,1,error)<0)
          return -1;
        break;

      case convString:
        {
          int len=0;
          const char* s=stringArg;
          while (*s++)
            len++;
          if (StreamWrite(stream,stringArg,len,error)<0)
            return -1;
        }
        break;

      case convPointer:
        *error=packosErrorUnknownError;
        return -1;
      }
  }

  goto CHAR_START;
}

int UtilPrintfStream(Stream stream,
                     PackosError* error,
                     const char* format,
                     ...)
{
  va_list ap;
  va_start(ap,format);
  return UtilVPrintfStream(stream,error,format,ap);
}
