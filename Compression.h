#ifndef FREEARC_COMPRESSION_H
#define FREEARC_COMPRESSION_H

// Type used to represent memory amounts
typedef unsigned MemSize;

//йНДШ НЬХАНЙ
#define FREEARC_OK                               0     /* ALL RIGHT */
#define FREEARC_ERRCODE_GENERAL                  (-1)  /* Some error when (de)compressing */
#define FREEARC_ERRCODE_INVALID_COMPRESSOR       (-2)  /* Invalid compression method or parameters */
#define FREEARC_ERRCODE_ONLY_DECOMPRESS          (-3)  /* Program builded with FREEARC_DECOMPRESS_ONLY, so don't try to use compress */
#define FREEARC_ERRCODE_OUTBLOCK_TOO_SMALL       (-4)  /* Output block size in (de)compressMem is not enough for all output data */
#define FREEARC_ERRCODE_NOT_ENOUGH_MEMORY        (-5)  /* Can't allocate memory needed for (de)compression */
#define FREEARC_ERRCODE_IO                       (-6)  /* Error when reading or writing data */


// йНМЯРЮМРШ ДКЪ СДНАМНИ ГЮОХЯХ НАЗ╦ЛНБ ОЮЛЪРХ
#define b_ 1
#define kb (1024*b_)
#define mb (1024*kb)
#define gb (1024*mb)

// йНКХВЕЯРБН АЮИР, ЙНРНПШЕ ДНКФМШ ВХРЮРЭЯЪ/ГЮОХЯШБЮРЭЯЪ ГЮ НДХМ ПЮГ БН БЯЕУ СОЮЙНБЫХЙЮУ
#define BUFFER_SIZE (64*kb)

// йНКХВЕЯРБН АЮИР, ЙНРНПШЕ ДНКФМШ ВХРЮРЭЯЪ/ГЮОХЯШБЮРЭЯЪ ГЮ НДХМ ПЮГ Б АШЯРПШУ ЛЕРНДЮУ (storing, ПЮЯОЮЙНБЙЮ ЮЯХЛЛЕРПХВМШУ ЮКЦНПХРЛНБ Х РНЛС ОНДНАМНЕ)
#define LARGE_BUFFER_SIZE (256*kb)

// дНОНКМХРЕКЭМШЕ НОПЕДЕКЕМХЪ ДКЪ СДНАЯРБЮ ЯНГДЮМХЪ ОЮПЯЕПНБ ЯРПНЙ ЛЕРНДНБ ЯФЮРХЪ
#define COMPRESSION_METHOD_PARAMETERS_DELIMITER     ':'   /* пЮГДЕКХРЕКЭ ОЮПЮЛЕРПНБ Б ЯРПНЙНБНЛ НОХЯЮМХХ ЛЕРНДЮ ЯФЮРХЪ */
#define MAX_COMPRESSION_METHODS  200         /* дНКФМН АШРЭ МЕ ЛЕМЭЬЕ ВХЯКЮ ЛЕРНДНБ ЯФЮРХЪ, ПЕЦХЯРПХПСЕЛШУ Я ОНЛНЫЭЧ AddCompressionMethod */
#define MAX_PARAMETERS           20          /* дНКФМН АШРЭ МЕ ЛЕМЭЬЕ ЛЮЙЯХЛЮКЭМНЦН ЙНК-БЮ ОЮПЮЛЕРПНБ (ПЮГДЕК╦ММШУ ДБНЕРНВХЪЛХ), ЙНРНПНЕ ЛНФЕР ХЛЕРЭ ЛЕРНД ЯФЮРХЪ */
#define MAX_METHOD_STRLEN        256         /* лЮЙЯХЛЮКЭМЮЪ ДКХМЮ ЯРПНЙХ, НОХЯШБЮЧЫЕИ ЛЕРНД ЯФЮРХЪ */

/******************************************************************************
** яРЮМДЮПРМШЕ НОПЕДЕКЕМХЪ ****************************************************
******************************************************************************/
#define make4byte(a,b,c,d)       ((a)+256*((b)+256*((c)+256*(((uint32)d)))))
#define FreeAndNil(p)            ((p) && (free(p), (p)=NULL))
#define iterate(num, statement)  {for( int i=0; i<num; i++) {statement;}}
#define iterate_var(i, num)      for( int i=0; i<num; i++)
#define iterate_array(i, array)  for( int i=0; i<array.size; i++)
#define TRUE                     1
#define FALSE                    0

#define in_set( c, set )         (strchr (set, c ) != NULL)
#define in_set0( c, set )        (memchr (set, c, sizeof(set) ) != 0)
#define str_end(str)             (strchr (str,'\0'))
#define last_char(str)           (str_end(str) [-1])
#define strequ(a,b)              (strcmp((a),(b))==EQUAL)
#define namecmp                  stricmp
#define nameequ(s1,s2)           (namecmp(s1,s2)==EQUAL)
#define end_with(str,with)       (nameequ (str_end(str)-strlen(with), with))
#define strdup_msg(s)            (strcpy (new char[strlen(s)+1], (s)))
#define find_extension(str)      (find_extension_in_entry (parse_path(str)))
#define mymax(a,b)               ((a)>(b)? (a) : (b))
#define mymin(a,b)               ((a)<(b)? (a) : (b))
#define char2int(c)              ((c)-'0')
#define elements(arr)            (sizeof(arr)/sizeof(*arr))
#define endof(arr)               ((arr)+elements(arr))
#define EQUAL                    0   /* result of strcmp/memcmp for equal strings */

// Read unsigned 16/24/32-bit value at given address
#define value16(p)               (*(uint*)(p) & 0xffff)
#define value24(p)               (*(uint*)(p) & 0xffffff)
#define value32(p)               (*(ulong*)(p))

#ifndef CHECK
#define CHECK(a,b)               {if (!(a))  printf b, exit(1);}
#endif

// Include statements marked as debug(..)  only if we enabled debugging
#ifdef DEBUG
#define debug
#else
#define debug(stmt) 0
#endif

// Include statements marked as stat(..)  only if we enabled gathering stats
#ifdef STAT
#define stat
#else
#define stat(stmt) 0
#endif


/******************************************************************************
** яХМНМХЛШ ДКЪ ОПНЯРШУ РХОНБ, ХЯОНКЭГСЕЛШУ Б ОПНЦПЮЛЛЕ ***********************
******************************************************************************/
typedef unsigned int       uint,   UINT;
typedef unsigned long      uint32, ulong;
typedef unsigned short int uint16, ushort;
typedef unsigned char      uint8,  uchar, byte, BYTE;
typedef   signed long      sint32;
typedef   signed short int sint16;
typedef   signed char      sint8;

#ifdef __GNUC__
typedef          long long sint64;
typedef unsigned long long uint64;
#elif _MSC_EXTENSIONS || _VISUALC || __INTEL_COMPILER || __BORLANDC__
typedef          __int64 sint64;
typedef unsigned __int64 uint64;
#else
typedef          long long sint64;
typedef unsigned long long uint64;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// рХО ТСМЙЖХХ ДКЪ ВРЕМХЪ БУНДМШУ Х ГЮОХЯХ БШУНДМШУ ДЮММШУ СОЮЙНБЫХЙНБ/ПЮЯОЮЙНБЫХЙНБ
typedef int INOUT_FUNC (void *buf, int size);

// пЮЯОЮЙНБЮРЭ ДЮММШЕ, СОЮЙНБЮММШЕ ГЮДЮММШЛ ЛЕРНДНЛ
int decompress (char *method, INOUT_FUNC *read_f, INOUT_FUNC *write_f, double *t);
// оПНВХРЮРЭ ХГ БУНДМНЦН ОНРНЙЮ НАНГМЮВЕМХЕ ЛЕРНДЮ ЯФЮРХЪ Х ПЮЯОЮЙНБЮРЭ ДЮММШЕ ЩРХЛ ЛЕРНДНЛ
int DecompressWithHeader (INOUT_FUNC *read_f, INOUT_FUNC *write_f, double *t);
// пЮЯОЮЙНБЮРЭ ДЮММШЕ Б ОЮЛЪРХ, ГЮОХЯЮБ Б БШУНДМНИ АСТЕП МЕ АНКЕЕ outputSize АЮИР.
// бНГБПЮЫЮЕР ЙНД НЬХАЙХ ХКХ ЙНКХВЕЯРБН АЮИР, ГЮОХЯЮММШУ Б БШУНДМНИ АСТЕП
int DecompressMem (char *method, void *input, int inputSize, void *output, int outputSize, double *t);
int DecompressMemWithHeader     (void *input, int inputSize, void *output, int outputSize, double *t);

#ifndef FREEARC_DECOMPRESS_ONLY
// сОЮЙНБЮРЭ ДЮММШЕ ГЮДЮММШЛ ЛЕРНДНЛ
int compress   (char *method, INOUT_FUNC *read_f, INOUT_FUNC *write_f, double *t);
// гЮОХЯЮРЭ Б БШУНДМНИ ОНРНЙ НАНГМЮВЕМХЕ ЛЕРНДЮ ЯФЮРХЪ Х СОЮЙНБЮРЭ ДЮММШЕ ЩРХЛ ЛЕРНДНЛ
int CompressWithHeader (char *method, INOUT_FUNC *read_f, INOUT_FUNC *write_f, double *t);
// сОЮЙНБЮРЭ ДЮММШЕ Б ОЮЛЪРХ, ГЮОХЯЮБ Б БШУНДМНИ АСТЕП МЕ АНКЕЕ outputSize АЮИР.
// бНГБПЮЫЮЕР ЙНД НЬХАЙХ ХКХ ЙНКХВЕЯРБН АЮИР, ГЮОХЯЮММШУ Б БШУНДМНИ АСТЕП
int CompressMem           (char *method, void *input, int inputSize, void *output, int outputSize, double *t);
int CompressMemWithHeader (char *method, void *input, int inputSize, void *output, int outputSize, double *t);
// бШБЕЯРХ Б out_method ЙЮМНМХВЕЯЙНЕ ОПЕДЯРЮБКЕМХЕ ЛЕРНДЮ ЯФЮРХЪ in_method (БШОНКМХРЭ ParseCompressionMethod + ShowCompressionMethod)
int CanonizeCompressionMethod (char *in_method, char *out_method);
// хМТНПЛЮЖХЪ Н ОЮЛЪРХ, МЕНАУНДХЛНИ ДКЪ СОЮЙНБЙХ/ПЮЯОЮЙНБЙХ, ПЮГЛЕПЕ ЯКНБЮПЪ Х ПЮГЛЕПЕ АКНЙЮ.
MemSize GetCompressionMem   (char *method);
MemSize GetDecompressionMem (char *method);
MemSize GetDictionary       (char *method);
MemSize GetBlockSize        (char *method);
// бНГБПЮРХРЭ Б out_method МНБШИ ЛЕРНД ЯФЮРХЪ, МЮЯРПНЕММШИ МЮ ХЯОНКЭГНБЮМХЕ
// ЯННРБЕРЯРБСЧЫЕЦН ЙНКХВЕЯРБЮ ОЮЛЪРХ/ЯКНБЮПЪ/ПЮГЛЕПЮ АКНЙЮ
int SetCompressionMem   (char *in_method, MemSize mem,  char *out_method);
int SetDecompressionMem (char *in_method, MemSize mem,  char *out_method);
int SetDictionary       (char *in_method, MemSize dict, char *out_method);
int SetBlockSize        (char *in_method, MemSize bs,   char *out_method);
// бНГБПЮРХРЭ Б out_method МНБШИ ЛЕРНД ЯФЮРХЪ, СЛЕМЭЬХБ, ЕЯКХ МЕНАУНДХЛН,
// ХЯОНКЭГСЕЛСЧ ЮКЦНПХРЛНЛ ОЮЛЪРЭ / ЕЦН ЯКНБЮПЭ / ПЮГЛЕП АКНЙЮ
int LimitCompressionMem   (char *in_method, MemSize mem,  char *out_method);
int LimitDecompressionMem (char *in_method, MemSize mem,  char *out_method);
int LimitDictionary       (char *in_method, MemSize dict, char *out_method);
int LimitBlockSize        (char *in_method, MemSize bs,   char *out_method);
#endif

// тСМЙЖХЪ "(ПЮЯ)ОЮЙНБЙХ", ЙНОХПСЧЫЮЪ ДЮММШЕ НДХМ Б НДХМ
int copy_data   (INOUT_FUNC *read_f, INOUT_FUNC *write_f);



#ifdef __cplusplus

// юАЯРПЮЙРМШИ ХМРЕПТЕИЯ Й ОПНХГБНКЭМНЛС ЛЕРНДС ЯФЮРХЪ
class COMPRESSION_METHOD
{
public:
  // тСМЙЖХХ ПЮЯОЮЙНБЙХ Х СОЮЙНБЙХ
  virtual int decompress (INOUT_FUNC *read_f, INOUT_FUNC *write_f) = 0;
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (INOUT_FUNC *read_f, INOUT_FUNC *write_f) = 0;

  // гЮОХЯЮРЭ Б buf[MAX_METHOD_STRLEN] ЯРПНЙС, НОХЯШБЮЧЫСЧ ЛЕРНД ЯФЮРХЪ Х ЕЦН ОЮПЮЛЕРПШ (ТСМЙЖХЪ, НАПЮРМЮЪ Й ParseCompressionMethod)
  virtual void ShowCompressionMethod (char *buf) = 0;

  // хМТНПЛЮЖХЪ Н ОЮЛЪРХ, МЕНАУНДХЛНИ ДКЪ СОЮЙНБЙХ/ПЮЯОЮЙНБЙХ,
  // ПЮГЛЕПЕ ЯКНБЮПЪ (РН ЕЯРЭ МЮЯЙНКЭЙН ДЮКЕЙН ГЮЦКЪДШБЮЕР ЮКЦНПХРЛ Б ОНХЯЙЕ ОНУНФХУ ДЮММШУ - ДКЪ lz/bs ЯУЕЛ),
  // Х ПЮГЛЕПЕ АКНЙЮ (РН ЕЯРЭ ЯЙНКЭЙН ЛЮЙЯХЛСЛ ДЮММШУ ХЛЕЕР ЯЛШЯК ОНЛЕЫЮРЭ Б НДХМ ЯНКХД-АКНЙ - ДКЪ bs ЯУЕЛ Х lzp)
  virtual MemSize GetCompressionMem   (void)         = 0;
  virtual MemSize GetDecompressionMem (void)         = 0;
  virtual MemSize GetDictionary       (void)         = 0;
  virtual MemSize GetBlockSize        (void)         = 0;
  // мЮЯРПНХРЭ ЛЕРНД ЯФЮРХЪ МЮ ХЯОНКЭГНБЮМХЕ ГЮДЮММНЦН ЙНК-БЮ ОЮЛЪРХ, ЯКНБЮПЪ ХКХ ПЮГЛЕПЮ АКНЙЮ
  virtual void    SetCompressionMem   (MemSize mem)  = 0;
  virtual void    SetDecompressionMem (MemSize mem)  = 0;
  virtual void    SetDictionary       (MemSize dict) = 0;
  virtual void    SetBlockSize        (MemSize bs)   = 0;
  // нЦПЮМХВХРЭ ХЯОНКЭГСЕЛСЧ ОПХ СОЮЙНБЙЕ/ПЮЯОЮЙНБЙЕ ОЮЛЪРЭ, ХКХ ЯКНБЮПЭ / ПЮГЛЕП АКНЙЮ
  void LimitCompressionMem   (MemSize mem)  {if (GetCompressionMem()   > mem)   SetCompressionMem(mem);}
  void LimitDecompressionMem (MemSize mem)  {if (GetDecompressionMem() > mem)   SetDecompressionMem(mem);}
  void LimitDictionary       (MemSize dict) {if (GetDictionary()       > dict)  SetDictionary(dict);}
  void LimitBlockSize        (MemSize bs)   {if (GetBlockSize()        > bs)    SetBlockSize(bs);}
#endif
//  Debugging code:  char buf[100]; ShowCompressionMethod(buf); printf("%s : %u => %u\n", buf, GetCompressionMem(), mem);
};

// яЙНМЯРПСХПНБЮРЭ НАЗЕЙР ЙКЮЯЯЮ - МЮЯКЕДМХЙЮ COMPRESSION_METHOD,
// ПЕЮКХГСЧЫХИ ЛЕРНД ЯФЮРХЪ, ГЮДЮММШИ Б БХДЕ ЯРПНЙХ `method`
COMPRESSION_METHOD *ParseCompressionMethod (char* method);


typedef COMPRESSION_METHOD* (*CM_PARSER) (char** parameters);
int AddCompressionMethod (CM_PARSER parser);  // дНАЮБХРЭ ОЮПЯЕП МНБНЦН ЛЕРНДЮ Б ЯОХЯНЙ ОНДДЕПФХБЮЕЛШУ ЛЕРНДНБ ЯФЮРХЪ
MemSize parseInt (char *param, int *error);   // еЯКХ ЯРПНЙЮ param ЯНДЕПФХР ЖЕКНЕ ВХЯКН - БНГБПЮРХРЭ ЕЦН, ХМЮВЕ СЯРЮМНБХРЭ error=1
MemSize parseMem (char *param, int *error);   // юМЮКНЦХВМН, РНКЭЙН ЯРПНЙЮ param ЛНФЕР ЯНДЕПФЮРЭ ЯСТТХЙЯШ b/k/m/g/^, ВРН НГМЮВЮЕР ЯННРБЕРЯРБСЧЫХЕ ЕДХМХЖШ ОЮЛЪРХ (ОН СЛНКВЮМХЧ - '^', Р.Е. ЯРЕОЕМЭ ДБНИЙХ)
#ifndef FREEARC_DECOMPRESS_ONLY
void showMem (MemSize mem, char *result);     // бНГБПЮЫЮЕР РЕЙЯРНБНЕ НОХЯЮМХЕ НАЗ╦ЛЮ ОЮЛЪРХ
#endif
void strncopy( char *to, char *from, int len );
void split (char *str, char splitter, char **result);

// пЕЮКХГЮЖХЪ ЛЕРНДЮ "ЯФЮРХЪ" STORING
class STORING_METHOD : public COMPRESSION_METHOD
{
public:
  // тСМЙЖХХ ПЮЯОЮЙНБЙХ Х СОЮЙНБЙХ
  virtual int decompress (INOUT_FUNC *read_f, INOUT_FUNC *write_f);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (INOUT_FUNC *read_f, INOUT_FUNC *write_f);

  // гЮОХЯЮРЭ Б buf[MAX_METHOD_STRLEN] ЯРПНЙС, НОХЯШБЮЧЫСЧ ЛЕРНД ЯФЮРХЪ (ТСМЙЖХЪ, НАПЮРМЮЪ Й parse_STORING)
  virtual void ShowCompressionMethod (char *buf);

  // оНКСВХРЭ/СЯРЮМНБХРЭ НАЗ╦Л ОЮЛЪРХ, ХЯОНКЭГСЕЛНИ ОПХ СОЮЙНБЙЕ/ПЮЯОЮЙНБЙЕ, ПЮГЛЕП ЯКНБЮПЪ ХКХ ПЮГЛЕП АКНЙЮ
  virtual MemSize GetCompressionMem   (void)    {return BUFFER_SIZE;}
  virtual MemSize GetDecompressionMem (void)    {return BUFFER_SIZE;}
  virtual MemSize GetDictionary       (void)    {return 0;}
  virtual MemSize GetBlockSize        (void)    {return 0;}
  virtual void    SetCompressionMem   (MemSize) {}
  virtual void    SetDecompressionMem (MemSize) {}
  virtual void    SetDictionary       (MemSize) {}
  virtual void    SetBlockSize        (MemSize) {}
#endif
};

// пЮГАНПЫХЙ ЯРПНЙХ ЛЕРНДЮ ЯФЮРХЪ STORING
COMPRESSION_METHOD* parse_STORING (char** parameters);

}       // extern "C"
#endif  // __cplusplus


/******************************************************************************
** A few commonly used functions **********************************************
******************************************************************************/

// Whole part of number's binary logarithm (please ensure that n>0)
static MemSize lb (MemSize n)
{
  MemSize i;
  for (i=0; n>1; i++, n/=2);
  return i;
}

// щРЮ ОПНЖЕДСПЮ НЙПСЦКЪЕР ВХЯКН Й АКХФЮИЬЕИ ЯБЕПУС ЯРЕОЕМХ
// АЮГШ, МЮОПХЛЕП f(13,2)=16
static MemSize roundup_to_power_of (MemSize n, MemSize base)
{
    MemSize result;
    if (n==1)  return 1;
    for (result=base, n--; (n/=base) != 0; result *= base);
    return result;
}

#endif
