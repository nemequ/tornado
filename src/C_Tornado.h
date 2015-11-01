#include "../Compression.h"

int tor_compress   (PackMethod m, CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
int tor_decompress (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);


#ifdef __cplusplus

// пЕЮКХГЮЖХЪ ЯРЮМДЮПРМНЦН ХМРЕПТЕИЯЮ ЛЕРНДНБ ЯФЮРХЪ COMPRESSION_METHOD
class TORNADO_METHOD : public COMPRESSION_METHOD
{
public:
  struct PackMethod m;      // оЮПЮЛЕРПШ ЩРНЦН ЛЕРНДЮ ЯФЮРХЪ

  // йНМЯРПСЙРНП, ОПХЯБЮХБЮЧЫХИ ОЮПЮЛЕРПЮЛ ЛЕРНДЮ ЯФЮРХЪ ГМЮВЕМХЪ ОН СЛНКВЮМХЧ
  TORNADO_METHOD();
  // сМХБЕПЯЮКЭМШИ ЛЕРНД: ДЮ╦Л ОНКНФХРЕКЭМШИ НРБЕР МЮ ГЮОПНЯШ "VeryFast?" ДКЪ ПЕФХЛНБ ЯФЮРХЪ 1-4
  virtual int doit (char *what, int param, void *data, CALLBACK_FUNC *callback)
  {
      if (strequ (what,"VeryFast?"))  return m.hash_row_width<=2;
      else return COMPRESSION_METHOD::doit (what, param, data, callback);
  }

  // тСМЙЖХХ ПЮЯОЮЙНБЙХ Х СОЮЙНБЙХ
  virtual int decompress (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);
#ifndef FREEARC_DECOMPRESS_ONLY
  virtual int compress   (CALLBACK_FUNC *callback, VOID_FUNC *auxdata);

  // гЮОХЯЮРЭ Б buf[MAX_METHOD_STRLEN] ЯРПНЙС, НОХЯШБЮЧЫСЧ ЛЕРНД ЯФЮРХЪ Х ЕЦН ОЮПЮЛЕРПШ (ТСМЙЖХЪ, НАПЮРМЮЪ Й parse_TORNADO)
  virtual void ShowCompressionMethod (char *buf);

  // оНКСВХРЭ/СЯРЮМНБХРЭ НАЗ╦Л ОЮЛЪРХ, ХЯОНКЭГСЕЛНИ ОПХ СОЮЙНБЙЕ/ПЮЯОЮЙНБЙЕ, ПЮГЛЕП ЯКНБЮПЪ ХКХ ПЮГЛЕП АКНЙЮ
  virtual MemSize GetCompressionMem     (void)         {return m.hashsize + m.buffer + tornado_compressor_outbuf_size(m.buffer);}
  virtual MemSize GetDecompressionMem   (void)         {return m.buffer;}
  virtual MemSize GetDictionary         (void)         {return m.buffer;}
  virtual MemSize GetBlockSize          (void)         {return 0;}
  virtual void    SetCompressionMem     (MemSize mem)  {if (mem>0)   m.hashsize = 1<<lb(mem/3), m.buffer=mem-m.hashsize;}
  virtual void    SetDecompressionMem   (MemSize mem)  {SetDictionary (mem);}
  virtual void    SetDictionary         (MemSize dict);
  virtual void    SetBlockSize          (MemSize bs)   {}
#endif
};

// пЮГАНПЫХЙ ЯРПНЙХ ЛЕРНДЮ ЯФЮРХЪ TORNADO
COMPRESSION_METHOD* parse_TORNADO (char** parameters);

#endif  // __cplusplus
