#define TORNADO_LIBRARY
#include "Tornado.cpp"
extern "C" {
#include "C_Tornado.h"
}

/*-------------------------------------------------*/
/* пЕЮКХГЮЖХЪ ЙКЮЯЯЮ TORNADO_METHOD                */
/*-------------------------------------------------*/

// йНМЯРПСЙРНП, ОПХЯБЮХБЮЧЫХИ ОЮПЮЛЕРПЮЛ ЛЕРНДЮ ЯФЮРХЪ ГМЮВЕМХЪ ОН СЛНКВЮМХЧ
TORNADO_METHOD::TORNADO_METHOD()
{
  m = std_Tornado_method [default_Tornado_method];
}

// тСМЙЖХЪ ПЮЯОЮЙНБЙХ
int TORNADO_METHOD::decompress (CALLBACK_FUNC *callback, VOID_FUNC *auxdata)
{
  return tor_decompress (callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

// тСМЙЖХЪ СОЮЙНБЙХ
int TORNADO_METHOD::compress (CALLBACK_FUNC *callback, VOID_FUNC *auxdata)
{
  return tor_compress (m, callback, auxdata);
}

// сЯРЮМНБХРЭ ПЮГЛЕП ЯКНБЮПЪ Х СЛЕМЭЬХРЭ ПЮГЛЕП УЩЬЮ, ЕЯКХ НМ ЯКХЬЙНЛ БЕКХЙ ДКЪ РЮЙНЦН ЛЮКЕМЭЙНЦН АКНЙЮ
void TORNADO_METHOD::SetDictionary (MemSize dict)
{
  if (dict>0) {
    m.buffer   = dict;
    m.hashsize = mymin (m.hashsize/sizeof(void*), 1<<(lb(m.buffer-1)+1)) * sizeof(void*);
  }
}

// гЮОХЯЮРЭ Б buf[MAX_METHOD_STRLEN] ЯРПНЙС, НОХЯШБЮЧЫСЧ ЛЕРНД ЯФЮРХЪ Х ЕЦН ОЮПЮЛЕРПШ (ТСМЙЖХЪ, НАПЮРМЮЪ Й parse_TORNADO)
void TORNADO_METHOD::ShowCompressionMethod (char *buf)
{
    struct PackMethod defaults = std_Tornado_method[m.number];  char NumStr[100], BufferStr[100], HashSizeStr[100], TempHashSizeStr[100], RowStr[100], EncStr[100], ParserStr[100], StepStr[100];
    showMem (m.buffer,   BufferStr);
    showMem (m.hashsize, TempHashSizeStr);
    sprintf (NumStr,      m.number  !=default_Tornado_method? ":%d"  : "", m.number);
    sprintf (HashSizeStr, m.hashsize!=defaults.hashsize     ? ":h%s" : "", TempHashSizeStr);
    sprintf (RowStr,      m.hash_row_width  !=defaults.hash_row_width?  ":l%d"  : "", m.hash_row_width);
    sprintf (EncStr,      m.encoding_method !=defaults.encoding_method? ":c%d"  : "", m.encoding_method);
    sprintf (ParserStr,   m.match_parser    !=defaults.match_parser?    ":p%d"  : "", m.match_parser);
    sprintf (StepStr,     m.update_step     !=defaults.update_step?     ":u%d"  : "", m.update_step);
    sprintf (buf, "tor%s:%s%s%s%s%s%s", NumStr, BufferStr, HashSizeStr, RowStr, EncStr, ParserStr, StepStr);
}

#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// йНМЯРПСХПСЕР НАЗЕЙР РХОЮ TORNADO_METHOD Я ГЮДЮММШЛХ ОЮПЮЛЕРПЮЛХ СОЮЙНБЙХ
// ХКХ БНГБПЮЫЮЕР NULL, ЕЯКХ ЩРН ДПСЦНИ ЛЕРНД ЯФЮРХЪ ХКХ ДНОСЫЕМЮ НЬХАЙЮ Б ОЮПЮЛЕРПЮУ
COMPRESSION_METHOD* parse_TORNADO (char** parameters)
{
  if (strcmp (parameters[0], "tor") == 0) {
    // еЯКХ МЮГБЮМХЕ ЛЕРНДЮ (МСКЕБНИ ОЮПЮЛЕРП) - "tor", РН ПЮГАЕП╦Л НЯРЮКЭМШЕ ОЮПЮЛЕРПШ

    TORNADO_METHOD *p = new TORNADO_METHOD;
    int error = 0;  // оПХГМЮЙ РНЦН, ВРН ОПХ ПЮГАНПЕ ОЮПЮЛЕРПНБ ОПНХГНЬКЮ НЬХАЙЮ

    // оЕПЕАЕП╦Л БЯЕ ОЮПЮЛЕРПШ ЛЕРНДЮ (ХКХ БШИДЕЛ ПЮМЭЬЕ ОПХ БНГМХЙМНБЕМХХ НЬХАЙХ ОПХ ПЮГАНПЕ НВЕПЕДМНЦН ОЮПЮЛЕРПЮ)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // оЮПЮЛЕРПШ, ЯНДЕПФЮЫХЕ ГМЮВЕМХЪ
        case 'b': p->m.buffer          = parseMem (param+1, &error); continue;
        case 'h': p->m.hashsize        = parseMem (param+1, &error); continue;
        case 'l': p->m.hash_row_width  = parseInt (param+1, &error); continue;
        case 'c': p->m.encoding_method = parseInt (param+1, &error); continue;
        case 'p': p->m.match_parser    = parseInt (param+1, &error); continue;
        case 'u': p->m.update_step     = parseInt (param+1, &error); continue;
      }
      // яЧДЮ ЛШ ОНОЮДЮЕЛ, ЕЯКХ Б ОЮПЮЛЕРПЕ МЕ СЙЮГЮМН ЕЦН МЮГБЮМХЕ
      // еЯКХ ЩРНР ОЮПЮЛЕРП СДЮЯРЯЪ ПЮГНАПЮРЭ ЙЮЙ ЖЕКНЕ ВХЯКН (Р.Е. Б М╦Л - РНКЭЙН ЖХТПШ),
      // РН АСДЕЛ ЯВХРЮРЭ, ВРН ЩРН МНЛЕП ОПЕЯЕРЮ, ХМЮВЕ ОНОПНАСЕЛ ПЮГНАПЮРЭ ЕЦН ЙЮЙ buffer
      int n = parseInt (param, &error);
      if (!error)  p->m = std_Tornado_method[n];
      else         error=0, p->m.buffer = parseMem (param, &error);
    }

    if (error)  {delete p; return NULL;}  // нЬХАЙЮ ОПХ ОЮПЯХМЦЕ ОЮПЮЛЕРПНБ ЛЕРНДЮ
    return p;
  } else
    return NULL;   // щРН МЕ ЛЕРНД TORNADO
}

static int TORNADO_x = AddCompressionMethod (parse_TORNADO);   // гЮПЕЦХЯРПХПСЕЛ ОЮПЯЕП ЛЕРНДЮ TORNADO
