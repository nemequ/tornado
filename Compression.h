#ifndef FREEARC_COMPRESSION_H
#define FREEARC_COMPRESSION_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "Common.h"


#ifdef __cplusplus
extern "C" {
#endif

//йНДШ НЬХАНЙ
#define FREEARC_OK                               0     /* ALL RIGHT */
#define FREEARC_ERRCODE_GENERAL                  (-1)  /* Some error when (de)compressing */
#define FREEARC_ERRCODE_INVALID_COMPRESSOR       (-2)  /* Invalid compression method or parameters */
#define FREEARC_ERRCODE_ONLY_DECOMPRESS          (-3)  /* Program builded with FREEARC_DECOMPRESS_ONLY, so don't try to use compress */
#define FREEARC_ERRCODE_OUTBLOCK_TOO_SMALL       (-4)  /* Output block size in (de)compressMem is not enough for all output data */
#define FREEARC_ERRCODE_NOT_ENOUGH_MEMORY        (-5)  /* Can't allocate memory needed for (de)compression */
#define FREEARC_ERRCODE_IO                       (-6)  /* Error when reading or writing data */
#define FREEARC_ERRCODE_BAD_COMPRESSED_DATA      (-7)  /* Data can't be decompressed */
#define FREEARC_ERRCODE_NOT_IMPLEMENTED          (-8)  /* Requested feature isn't supported */
#define FREEARC_ERRCODE_NO_MORE_DATA_REQUIRED    (-9)  /* Required part of data was already decompressed */


// йНМЯРЮМРШ ДКЪ СДНАМНИ ГЮОХЯХ НАЗ╦ЛНБ ОЮЛЪРХ
#define b_ 1
#define kb (1024*b_)
#define mb (1024*kb)
#define gb (1024*mb)

// йНКХВЕЯРБН АЮИР, ЙНРНПШЕ ДНКФМШ ВХРЮРЭЯЪ/ГЮОХЯШБЮРЭЯЪ ГЮ НДХМ ПЮГ БН БЯЕУ СОЮЙНБЫХЙЮУ
#define BUFFER_SIZE (64*kb)

// йНКХВЕЯРБН АЮИР, ЙНРНПШЕ ДНКФМШ ВХРЮРЭЯЪ/ГЮОХЯШБЮРЭЯЪ ГЮ НДХМ ПЮГ Б АШЯРПШУ ЛЕРНДЮУ Х ОПХ ПЮЯОЮЙНБЙЕ ЮЯХЛЛЕРПХВМШУ ЮКЦНПХРЛНБ
#define LARGE_BUFFER_SIZE (256*kb)

// йНКХВЕЯРБН АЮИР, ЙНРНПШЕ ДНКФМШ ВХРЮРЭЯЪ/ГЮОХЯШБЮРЭЯЪ ГЮ НДХМ ПЮГ Б НВЕМЭ АШЯРПШУ ЛЕРНДЮУ (storing, tornado Х РНЛС ОНДНАМНЕ)
// щРНР НАЗ╦Л ЛХМХЛХГХПСЕР ОНРЕПХ МЮ disk seek operations - ОПХ СЯКНБХХ, ВРН НДМНБПЕЛЕММН МЕ ОПНХЯУНДХР Б/Б Б ДПСЦНЛ ОНРНЙЕ ;)
#define HUGE_BUFFER_SIZE (8*mb)

// дНОНКМХРЕКЭМШЕ НОПЕДЕКЕМХЪ ДКЪ СДНАЯРБЮ ЯНГДЮМХЪ ОЮПЯЕПНБ ЯРПНЙ ЛЕРНДНБ ЯФЮРХЪ
#define COMPRESSION_METHODS_DELIMITER            '+'   /* пЮГДЕКХРЕКЭ ЮКЦНПХРЛНБ ЯФЮРХЪ Б ЯРПНЙНБНЛ НОХЯЮМХХ ЙНЛОПЕЯЯНПЮ */
#define COMPRESSION_METHOD_PARAMETERS_DELIMITER  ':'   /* пЮГДЕКХРЕКЭ ОЮПЮЛЕРПНБ Б ЯРПНЙНБНЛ НОХЯЮМХХ ЛЕРНДЮ ЯФЮРХЪ */
#define MAX_COMPRESSION_METHODS    200         /* дНКФМН АШРЭ МЕ ЛЕМЭЬЕ ВХЯКЮ ЛЕРНДНБ ЯФЮРХЪ, ПЕЦХЯРПХПСЕЛШУ Я ОНЛНЫЭЧ AddCompressionMethod */
#define MAX_PARAMETERS             200         /* дНКФМН АШРЭ МЕ ЛЕМЭЬЕ ЛЮЙЯХЛЮКЭМНЦН ЙНК-БЮ ОЮПЮЛЕРПНБ (ПЮГДЕК╦ММШУ ДБНЕРНВХЪЛХ), ЙНРНПНЕ ЛНФЕР ХЛЕРЭ ЛЕРНД ЯФЮРХЪ */
#define MAX_METHOD_STRLEN          2048        /* лЮЙЯХЛЮКЭМЮЪ ДКХМЮ ЯРПНЙХ, НОХЯШБЮЧЫЕИ ЛЕРНД ЯФЮРХЪ */
#define MAX_METHODS_IN_COMPRESSOR  100         /* лЮЙЯХЛЮКЭМНЕ ВХЯКН ЛЕРНДНБ Б НДМНЛ ЙНЛОПЕЯЯНПЕ */
#define MAX_EXTERNAL_COMPRESSOR_SECTION_LENGTH 2048  /* лЮЙЯХЛЮКЭМЮЪ ДКХМЮ ЯЕЙЖХХ [External compressor] */


// ****************************************************************************************************************************
// уекоепш времхъ/гюохях дюммшу б лерндюу яфюрхъ ******************************************************************************
// ****************************************************************************************************************************

// рХО ТСМЙЖХХ ДКЪ ВРЕМХЪ БУНДМШУ Х ГЮОХЯХ БШУНДМШУ ДЮММШУ СОЮЙНБЫХЙНБ/ПЮЯОЮЙНБЫХЙНБ
typedef int INOUT_FUNC (void *buf, int size);

// рХО ТСМЙЖХХ ДКЪ НАПЮРМШУ БШГНБНБ. callback МЮ ЯЮЛНЛ ДЕКЕ ХЛЕЕР РХО CALLBACK_FUNC* ;)
typedef void VOID_FUNC (void);
typedef int CALLBACK_FUNC (char *what, void *data, int size, VOID_FUNC *callback);

// лЮЙПНЯШ ДКЪ ВРЕМХЪ/ГЮОХЯХ Б(Ш)УНДМШУ ОНРНЙНБ Я ОПНБЕПЙНИ, ВРН ОЕПЕДЮМН ПНБМН ЯРНКЭЙН ДЮММШУ, ЯЙНКЭЙН АШКН ГЮОПНЬЕМН
#define checked_read(ptr,size)         if ((x = callback("read" ,ptr,size,auxdata)) != size) { x>=0 && (x=FREEARC_ERRCODE_IO); goto finished; }
#define checked_write(ptr,size)        if ((x = callback("write",ptr,size,auxdata)) != size) { x>=0 && (x=FREEARC_ERRCODE_IO); goto finished; }
// лЮЙПНЯ ДКЪ ВРЕМХЪ БУНДМШУ ОНРНЙНБ Я ОПНБЕПЙНИ МЮ НЬХАЙХ Х ЙНМЕЖ БУНДМШУ ДЮММШУ
#define checked_eof_read(ptr,size)     if ((x = callback("write",ptr,size,auxdata)) != size) { x>0  && (x=FREEARC_ERRCODE_IO); goto finished; }

// Auxiliary code to read/write data blocks and 4-byte headers
#define MALLOC(type, ptr, size)                                            \
{                                                                          \
    (ptr) = (type*) malloc ((size) * sizeof(type));                        \
    if ((ptr) == NULL) {                                                   \
        errcode = FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;                       \
        goto finished;                                                     \
    }                                                                      \
}

#define READ(buf, size)                                                    \
{                                                                          \
    void *localBuf = (buf);                                                \
    int localSize  = (size);                                               \
    if (localSize && (errcode=callback("read",localBuf,localSize,auxdata))!=size) { \
        if (errcode>0) (errcode=FREEARC_ERRCODE_IO);                       \
        goto finished;                                                     \
    }                                                                      \
}

#define READ_LEN(len, buf, size)                                           \
{                                                                          \
    if ((errcode=len=callback("read",buf,size,auxdata))<=0) {              \
        goto finished;                                                     \
    }                                                                      \
}

#define WRITE(buf, size)                                                   \
{                                                                          \
    void *localBuf = (buf);                                                \
    int localSize  = (size);                                               \
    if (localSize && (errcode=callback("write",localBuf,localSize,auxdata))<0)  \
        goto finished;                                                     \
}

#define READ4(var)                                                         \
{                                                                          \
    unsigned char header[4];                                               \
    errcode = callback ("read", header, 4, auxdata);                       \
    if (errcode != 4) {                                                    \
        if (errcode>0) (errcode=FREEARC_ERRCODE_IO);                       \
        goto finished;                                                     \
    }                                                                      \
    (var) = (((((header[3]<<8)+header[2])<<8)+header[1])<<8)+header[0];    \
}

#define WRITE4(value)                                                      \
{                                                                          \
    unsigned char header[4];                                               \
    header[3] = ((unsigned)value)>>24;                                     \
    header[2] = ((unsigned)value)>>16;                                     \
    header[1] = ((unsigned)value)>>8;                                      \
    header[0] = ((unsigned)value);                                         \
    WRITE (header, 4);                                                     \
}

#define QUASIWRITE(size)                                                   \
{                                                                          \
    int64 localSize = (size);                                              \
    callback ("quasiwrite", &localSize, 0, auxdata);                       \
}


// Buffered data output
#define FOPEN()   Buffer fbuffer(BUFFER_SIZE)
#define FWRITE(buf, size)                                                  \
{                                                                          \
    void *flocalBuf = (buf);                                               \
    int flocalSize = (size);                                               \
    int rem = fbuffer.remainingSpace();                                    \
    if (flocalSize>=4096) {                                                \
        FFLUSH();                                                          \
        WRITE(flocalBuf, flocalSize);                                      \
    } else if (flocalSize < rem) {                                         \
        fbuffer.put (flocalBuf, flocalSize);                               \
    } else {                                                               \
        fbuffer.put (flocalBuf, rem);                                      \
        FFLUSH();                                                          \
        fbuffer.put ((byte*)flocalBuf+rem, flocalSize-rem);                \
    }                                                                      \
}
#define FFLUSH()  { WRITE (fbuffer.buf, fbuffer.len());  fbuffer.empty(); }
#define FCLOSE()  { FFLUSH();  fbuffer.free(); }


// ****************************************************************************************************************************
// срхкхрш ********************************************************************************************************************
// ****************************************************************************************************************************

// юКЦНПХРЛ ЯФЮРХЪ/ЬХТПНБЮМХЪ, ОПЕДЯРЮБКЕММШИ Б БХДЕ ЯРПНЙХ
typedef char *CMETHOD;

// оНЯКЕДНБЮРЕКЭМНЯРЭ ЮКЦНПХРЛНБ ЯФЮРХЪ/ЬХТПНБЮМХЪ, ОПЕДЯРЮБКЕММЮЪ Б БХДЕ "exe+rep+lzma+aes"
typedef char *COMPRESSOR;

// гЮОПНЯХРЭ ЯЕПБХЯ what ЛЕРНДЮ ЯФЮРХЪ method
int CompressionService (char *method, char *what, DEFAULT(int param,0), DEFAULT(void *data,NULL), DEFAULT(CALLBACK_FUNC *callback,NULL));

// оПНБЕПХРЭ, ВРН ДЮММШИ ЙНЛОПЕЯЯНП БЙКЧВЮЕР ЮКЦНПХРЛ ЬХТПНБЮМХЪ
int compressorIsEncrypted (COMPRESSOR c);
// бШВХЯКХРЭ, ЯЙНКЭЙН ОЮЛЪРХ МСФМН ДКЪ ПЮЯОЮЙНБЙХ ДЮММШУ, ЯФЮРШУ ЩРХЛ ЙНЛОПЕЯЯНПНЛ
MemSize compressorGetDecompressionMem (COMPRESSOR c);

// Get/set number of threads used for (de)compression
int  GetCompressionThreads (void);
void SetCompressionThreads (int threads);

// Register/unregister temporary files that should be deleted on ^Break
void registerTemporaryFile   (char *name, DEFAULT(FILE* file, NULL));
void unregisterTemporaryFile (char *name);

// This function should cleanup Compression Library
void compressionLib_cleanup (void);


// ****************************************************************************************************************************
// яепбхяш яфюрхъ х пюяоюйнбйх дюммшу *****************************************************************************************
// ****************************************************************************************************************************

// пЮЯОЮЙНБЮРЭ ДЮММШЕ, СОЮЙНБЮММШЕ ГЮДЮММШЛ ЛЕРНДНЛ
int decompress (char *method, CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
// оПНВХРЮРЭ ХГ БУНДМНЦН ОНРНЙЮ НАНГМЮВЕМХЕ ЛЕРНДЮ ЯФЮРХЪ Х ПЮЯОЮЙНБЮРЭ ДЮММШЕ ЩРХЛ ЛЕРНДНЛ
int DecompressWithHeader (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
// пЮЯОЮЙНБЮРЭ ДЮММШЕ Б ОЮЛЪРХ, ГЮОХЯЮБ Б БШУНДМНИ АСТЕП МЕ АНКЕЕ outputSize АЮИР.
// бНГБПЮЫЮЕР ЙНД НЬХАЙХ ХКХ ЙНКХВЕЯРБН АЮИР, ГЮОХЯЮММШУ Б БШУНДМНИ АСТЕП
int DecompressMem (char *method, void *input, int inputSize, void *output, int outputSize);
int DecompressMemWithHeader     (void *input, int inputSize, void *output, int outputSize);

#ifndef FREEARC_DECOMPRESS_ONLY
// сОЮЙНБЮРЭ ДЮММШЕ ГЮДЮММШЛ ЛЕРНДНЛ
int compress   (char *method, CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
// гЮОХЯЮРЭ Б БШУНДМНИ ОНРНЙ НАНГМЮВЕМХЕ ЛЕРНДЮ ЯФЮРХЪ Х СОЮЙНБЮРЭ ДЮММШЕ ЩРХЛ ЛЕРНДНЛ
int CompressWithHeader (char *method, CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
// сОЮЙНБЮРЭ ДЮММШЕ Б ОЮЛЪРХ, ГЮОХЯЮБ Б БШУНДМНИ АСТЕП МЕ АНКЕЕ outputSize АЮИР.
// бНГБПЮЫЮЕР ЙНД НЬХАЙХ ХКХ ЙНКХВЕЯРБН АЮИР, ГЮОХЯЮММШУ Б БШУНДМНИ АСТЕП
int CompressMem           (char *method, void *input, int inputSize, void *output, int outputSize);
int CompressMemWithHeader (char *method, void *input, int inputSize, void *output, int outputSize);
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
int copy_data   (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);


// ****************************************************************************************************************************
// йкюяя, пеюкхгсчыхи хмрептеия й лерндс яфюрхъ *******************************************************************************
// ****************************************************************************************************************************

#ifdef __cplusplus

// юАЯРПЮЙРМШИ ХМРЕПТЕИЯ Й ОПНХГБНКЭМНЛС ЛЕРНДС ЯФЮРХЪ
class COMPRESSION_METHOD
{
public:
  // тСМЙЖХХ ПЮЯОЮЙНБЙХ Х СОЮЙНБЙХ
  virtual int decompress (CALLBACK_FUNC *callback, VOID_FUNC *auxdata) = 0;
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (CALLBACK_FUNC *callback, VOID_FUNC *auxdata) = 0;

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
  // сМХБЕПЯЮКЭМШИ ЛЕРНД. оЮПЮЛЕРПШ:
  //   what: "compress", "decompress", "setCompressionMem", "limitDictionary"...
  //   data: ДЮММШЕ ДКЪ НОЕПЮЖХХ Б ТНПЛЮРЕ, ГЮБХЯЪЫЕЛ НР ЙНМЙПЕРМНИ БШОНКМЪЕЛНИ НОЕПЮЖХХ
  //   param&result: ОПНЯРНИ ВХЯКНБНИ ОЮПЮЛЕРП, ВРН ДНЯРЮРНВМН ДКЪ ЛМНЦХУ ХМТНПЛЮЖХНММШУ НОЕПЮЖХИ
  // мЕХЯОНКЭГСЕЛШЕ ОЮПЮЛЕРПШ СЯРЮМЮБКХБЮЧРЯЪ Б NULL/0. result<0 - ЙНД НЬХАЙХ
  virtual int doit (char *what, int param, void *data, CALLBACK_FUNC *callback);

  double addtime;  // дНОНКМХРЕКЭМНЕ БПЕЛЪ, ОНРПЮВЕММНЕ МЮ ЯФЮРХЕ (БН БМЕЬМХУ ОПНЦПЮЛЛЮУ, ДНОНКМХРЕКЭМШУ threads Х Р.Д.)
  COMPRESSION_METHOD() {addtime=0;}
  virtual ~COMPRESSION_METHOD() {}
//  Debugging code:  char buf[100]; ShowCompressionMethod(buf); printf("%s : %u => %u\n", buf, GetCompressionMem(), mem);
};


// ****************************************************************************************************************************
// тюапхйю COMPRESSION_METHOD *************************************************************************************************
// ****************************************************************************************************************************

// яЙНМЯРПСХПНБЮРЭ НАЗЕЙР ЙКЮЯЯЮ - МЮЯКЕДМХЙЮ COMPRESSION_METHOD,
// ПЕЮКХГСЧЫХИ ЛЕРНД ЯФЮРХЪ, ГЮДЮММШИ Б БХДЕ ЯРПНЙХ `method`
COMPRESSION_METHOD *ParseCompressionMethod (char* method);

typedef COMPRESSION_METHOD* (*CM_PARSER) (char** parameters);
typedef COMPRESSION_METHOD* (*CM_PARSER2) (char** parameters, void *data);
int AddCompressionMethod         (CM_PARSER parser);  // дНАЮБХРЭ ОЮПЯЕП МНБНЦН ЛЕРНДЮ Б ЯОХЯНЙ ОНДДЕПФХБЮЕЛШУ ЛЕРНДНБ ЯФЮРХЪ
int AddExternalCompressionMethod (CM_PARSER2 parser2, void *data);  // дНАЮБХРЭ ОЮПЯЕП БМЕЬМЕЦН ЛЕРНДЮ ЯФЮРХЪ Я ДНОНКМХРЕКЭМШЛ ОЮПЮЛЕРПНЛ, ЙНРНПШИ ДНКФЕМ АШРЭ ОЕПЕДЮМ ЩРНЛС ОЮПЯЕПС
void ClearExternalCompressorsTable (void);                          // нВХЯРХРЭ РЮАКХЖС БМЕЬМХУ СОЮЙНБЫХЙНБ


// ****************************************************************************************************************************
// лернд "яфюрхъ" STORING *****************************************************************************************************
// ****************************************************************************************************************************

// пЕЮКХГЮЖХЪ ЛЕРНДЮ "ЯФЮРХЪ" STORING
class STORING_METHOD : public COMPRESSION_METHOD
{
public:
  // тСМЙЖХХ ПЮЯОЮЙНБЙХ Х СОЮЙНБЙХ
  virtual int decompress (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);

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

#endif  // __cplusplus


// ****************************************************************************************************************************
// ENCRYPTION ROUTINES *****************************************************************************************************
// ****************************************************************************************************************************

// Generates key based on password and salt using given number of hashing iterations
void Pbkdf2Hmac (const BYTE *pwd, int pwdSize, const BYTE *salt, int saltSize,
                 int numIterations, BYTE *key, int keySize);

int fortuna_size (void);


#ifdef __cplusplus
}       // extern "C"
#endif

#endif  // FREEARC_COMPRESSION_H
