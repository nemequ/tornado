// (c) Bulat Ziganshin <Bulat.Ziganshin@gmail.com>
// (c) Joachim Henke
// GPL'ed code of Tornado - fast LZ77 compression algorithm.
#include "Compression.h"
#include "MatchFinder.cpp"
#include "EntropyCoder.cpp"
#include "LZ77_Coder.cpp"
#include "DataTables.cpp"

// Compression method parameters
struct PackMethod
{
    int  number;            // Preset number
    int  encoding_method;   // Coder (0 - storing, 1 - bytecoder, 2 - bitcoder, 3 - huffman, 4 - arithmetic)
    bool find_tables;       // Enable searching for MM tables
    int  hash_row_width;    // Length of hash row
    uint hashsize;          // Hash size
    int  caching_finder;    // Force/prohibit using caching match finder
    uint buffer;            // Buffer (dictionary) size
    int  match_parser;      // Match parser (1 - greedy, 2 - lazy, 3 - flexible, 4 - optimal, 5 - even better)
    int  hash3;             // 2/3-byte hash presence and type
    int  shift;             // How much bytes to shift out/keep when window slides
    int  update_step;       // How much bytes are skipped in mf.update()
    uint auxhash_size;      // Auxiliary hash size
    int  auxhash_row_width; // Length of auxiliary hash row
};

extern "C" {
// Main compression and decompression routines
int tor_compress   (PackMethod m, CALLBACK_FUNC *callback, void *auxdata);
int tor_decompress (CALLBACK_FUNC *callback, void *auxdata);
}

enum { STORING=0, BYTECODER=1, BITCODER=2, HUFCODER=3, ARICODER=4, ROLZ_HUFCODER=13 };
enum { GREEDY=1, LAZY=2 };

// Preconfigured compression modes
PackMethod std_Tornado_method[] =
    //                 tables row  hashsize  caching buffer parser  hash3 shift update   auxhash
    { {  0, STORING,   false,   0,        0, 0,      1*mb,  0     ,   0,    0,  999,       0,    0 }
    , {  1, BYTECODER, false,   1,    16*kb, 0,      1*mb,  GREEDY,   0,    0,  999,       0,    0 }
    , {  2, BITCODER,  false,   1,    64*kb, 0,      2*mb,  GREEDY,   0,    0,  999,       0,    0 }
    , {  3, HUFCODER,  true,    2,   128*kb, 0,      4*mb,  GREEDY,   0,    0,  999,       0,    0 }
    , {  4, HUFCODER,  true,    2,     2*mb, 1,      8*mb,  GREEDY,   0,    0,  999,       0,    0 }
    , {  5, ARICODER,  true,    4,     8*mb, 1,     16*mb,  LAZY  ,   1,    0,  999,       0,    0 }
    , {  6, ARICODER,  true,    8,    32*mb, 1,     64*mb,  LAZY  ,   1,    0,    4,       0,    0 }
    , {  7, ARICODER,  true,   32,   128*mb, 5,    256*mb,  LAZY  ,   2,    0,    1,  128*kb,    4 }
    , {  8, ARICODER,  true,  128,   512*mb, 5,   1024*mb,  LAZY  ,   2,    0,    1,  128*kb,    4 }
    , {  9, ARICODER,  true,  256,  2048*mb, 5,   1024*mb,  LAZY  ,   2,    0,    1,  512*kb,    4 }
    , { 10, ARICODER,  true,  256,  2048*mb, 6,   1024*mb,  LAZY  ,   2,    0,    1,    2*mb,   32 }
    , { 11, ARICODER,  true,  200,  1600*mb, 7,   1024*mb,  LAZY  ,   2,    0,    1,  512*mb,  256 }
    };

// Default compression parameters are equivalent to option -5
const int default_Tornado_method = 5;

// If data table was not encountered in last table_dist bytes, don't check next table_shift bytes in order to make things faster
const int table_dist=256*1024, table_shift=128;

// Minimum lookahead for next match which compressor tries to guarantee.
// Also minimum amount of allocated space after end of buf (this allows to use things like p[11] without additional checks)
#define LOOKAHEAD 256


// нЙПСЦКХЛ ПЮГЛЕП УЕЬЮ Я СВ╦РНЛ hash_row_width
uint round_to_nearest_hashsize (LongMemSize hashsize, uint hash_row_width)
{return mymin (round_to_nearest_power_of(mymin(hashsize,2*gb-1) / hash_row_width, 2) * hash_row_width, 2*gb-1);}

// Dictionary size depending on memory available for dictionary+outbuf (opposite to tornado_compressor_outbuf_size)
uint tornado_compressor_calc_dict (uint mem)
{return compress_all_at_once?  mem/9*4
                            :  mem>2*LARGE_BUFFER_SIZE ? mem-LARGE_BUFFER_SIZE : mem/2;}

// Output buffer size for compressor (the worst case is bytecoder that adds 2 bits per byte on incompressible data)
uint tornado_compressor_outbuf_size (uint buffer, int bytes_to_compress = -1)
{return bytes_to_compress!=-1? bytes_to_compress+(bytes_to_compress/4)+512 :
        compress_all_at_once?  buffer+(buffer/4)+512 :
                               mymin (buffer+512, LARGE_BUFFER_SIZE);}

// Output buffer size for decompressor
uint tornado_decompressor_outbuf_size (uint buffer)
{return compress_all_at_once?  buffer+(buffer/8)+512 :
                               mymax (buffer, LARGE_BUFFER_SIZE);}


#ifndef FREEARC_DECOMPRESS_ONLY

// Check for data table with N byte elements at current pos
#define CHECK_FOR_DATA_TABLE(N)                                                                         \
{                                                                                                       \
    if (p[-1]==p[N-1]                                                                                   \
    &&  uint(p[  N-1] - p[2*N-1] + 4) <= 2*4                                                            \
    &&  uint(p[2*N-1] - p[3*N-1] + 4) <= 2*4                                                            \
    &&  !val32equ(p+2*N-4, p+N-4))                                                                      \
    {                                                                                                   \
        int type, items;                                                                                \
        if (check_for_data_table (N, type, items, p, bufend, table_end, buf, offset, last_checked)) {   \
            coder.encode_table (type, items);                                                           \
            /* If data table was diffed, we should invalidate match cached by lazy match finder */      \
            mf.invalidate_match();                                                                      \
            goto found;                                                                                 \
        }                                                                                               \
    }                                                                                                   \
}


// Read next datachunk into buffer, shifting old contents if required
template <class MatchFinder, class Coder>
int read_next_chunk (PackMethod &m, CALLBACK_FUNC *callback, void *auxdata, MatchFinder &mf, Coder &coder, byte *&p, byte *buf, BYTE *&bufend, BYTE *&table_end, BYTE *&last_found, BYTE *&read_point, int &bytes, int &chunk, uint64 &offset, byte *(&last_checked)[MAX_TABLE_ROW][MAX_TABLE_ROW])
{
    if (bytes==0 || compress_all_at_once)  return 0;     // All input data was successfully compressed
    // If we can't provide 256 byte lookahead then shift data toward buffer beginning,
    // freeing space at buffer end for the new data
    if (bufend-buf > m.buffer-LOOKAHEAD) {
        int sh;
        if (m.shift==-1) {
            sh = p-(buf+2);  // p should become buf+2 after this shift
            memcpy (buf, buf+sh, bufend-(buf+sh));
            mf.clear_hash (buf);
        } else {
            sh = m.shift>0? m.shift : bufend-buf+m.shift;
            memcpy (buf, buf+sh, bufend-(buf+sh));
            mf.shift (buf, sh);
        }
        p      -= sh;
        bufend -= sh;
        offset += sh;
        if (coder.support_tables && m.find_tables)
            table_end  = table_end >buf+sh? table_end -sh : buf,
            last_found = last_found>buf+sh? last_found-sh : buf;
        iterate_var(i,MAX_TABLE_ROW)  iterate_var(j,MAX_TABLE_ROW)  last_checked[i][j] = buf;
        mf.invalidate_match();  // invalidate match stored in lazy MF; otherwise it may fuck up the NEXT REPCHAR checking
        coder.shift_occurs();   // Tell to the coder what shift occurs
        debug (printf ("==== SHIFT %08x: p=%08x ====\n", sh, p-buf));
    }
    bytes = callback ("read", bufend, mymin (chunk, buf+m.buffer-bufend), auxdata);
    debug (printf ("==== read %08x ====\n", bytes));
    if (bytes<0)  return bytes;    // Return errcode on error
    bufend += bytes;
    read_point = bytes==0? bufend:bufend-LOOKAHEAD;
    coder.flush();          // Sometimes data should be written to disk :)
    return p<bufend? 1 : 0; // Result code: 1 if we still have bytes to compress, 0 otherwise
}


// Compress one chunk of data
template <class MatchFinder, class Coder>
int tor_compress_chunk (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, byte *buf, int bytes_to_compress)
{
    // Read data in these chunks
    int chunk = compress_all_at_once? m.buffer : mymin (m.shift>0? m.shift:m.buffer, LARGE_BUFFER_SIZE);
    uint64 offset = 0;                        // Current offset of buf[] contents relative to file (increased with each shift() operation)
    int bytes = bytes_to_compress!=-1? bytes_to_compress : callback ("read", buf, chunk, auxdata);   // Number of bytes read by last "read" call
    if (bytes<0)  return bytes;               // Return errcode on error
    BYTE *bufend = buf + bytes;               // Current end of real data in buf[]
    BYTE *matchend = bufend - mymin (MAX_HASHED_BYTES, bufend-buf);   // Maximum pos where match may finish (less than bufend in order to simplify hash updating)
    BYTE *read_point = compress_all_at_once || bytes_to_compress!=-1? bufend : bufend-mymin(LOOKAHEAD,bytes); // Next point where next chunk of data should be read to buf
    // Match finder will search strings similar to current one in previous data
    MatchFinder mf (buf, m.hashsize, m.hash_row_width, m.auxhash_size, m.auxhash_row_width);
    if (mf.error() != FREEARC_OK)  return mf.error();
    // Coder will encode LZ output into bits and put them to outstream
    // Data should be written in HUGE_BUFFER_SIZE chunks (at least) plus chunk*2 bytes should be allocated to ensure that no buffer overflow may occur (because we flush() data only after processing each 'chunk' input bytes)
    Coder coder (m.encoding_method, callback, auxdata, tornado_compressor_outbuf_size (m.buffer, bytes_to_compress), compress_all_at_once? 0:chunk*2);
    if (coder.error() != FREEARC_OK)  return coder.error();
    BYTE *table_end  = coder.support_tables && m.find_tables? buf : buf+m.buffer+LOOKAHEAD;    // The end of last data table processed
    BYTE *last_found = buf;                             // Last position where data table was found
    byte *last_checked[MAX_TABLE_ROW][MAX_TABLE_ROW];   // Last position where data table of size %1 with offset %2 was tried
    if(coder.support_tables)  {iterate_var(i,MAX_TABLE_ROW)  iterate_var(j,MAX_TABLE_ROW)  last_checked[i][j] = buf;}
    // Use first output bytes to store encoding_method, minlen and buffer size
    coder.put8 (m.encoding_method);
    coder.put8 (mf.min_length());
    coder.put32(m.buffer);
    // Encode first four bytes directly (at least 2 bytes should be saved directly in order to avoid problems with using p-2 in MatchFinder.update())
    for (BYTE *p=buf; p<buf+4; p++) {
        if (p>=bufend)  goto finished;
        coder.encode (0, p, buf, mf.min_length());
    }

    // ========================================================================
    // MAIN CYCLE: FIND AND ENCODE MATCHES UNTIL DATA END
    for (BYTE *p=buf+4; TRUE; ) {
        // Read next chunk of data if all data up to read_point was already processed
        if (p >= read_point) {
            if (bytes_to_compress!=-1)  goto finished;  // We shouldn't read/write any data!
            byte *p1=p;  // This trick allows to not take address of p and this buys us a bit better program optimization
            int res = read_next_chunk (m, callback, auxdata, mf, coder, p1, buf, bufend, table_end, last_found, read_point, bytes, chunk, offset, last_checked);
            p=p1, matchend = bufend - mymin (MAX_HASHED_BYTES, bufend-buf);
            if (res==0)  goto finished;    // All input data were successfully compressed
            if (res<0)   return res;       // Error occured while reading data
        }

        // Check for data table that may be subtracted to improve compression
        if (coder.support_tables  &&  p > table_end) {
            if (mf.min_length() < 4)                      // increase speed by skipping this check in faster modes
              CHECK_FOR_DATA_TABLE (2);
            CHECK_FOR_DATA_TABLE (4);
            if (p-last_found > table_dist)  table_end = p + table_shift;
            goto not_found;
            found: last_found=table_end;
            not_found:;
        }

        // Find match length and position
        UINT len = mf.find_matchlen (p, matchend, 0);
        BYTE *q  = mf.get_matchptr();
        // Encode either match or literal
        coder.set_context(p[-1]);
        if (!coder.encode (len, p, q, mf.min_length())) {      // literal encoded
            print_literal (p-buf+offset, *p); p++;
        } else {                                               // match encoded
            // Update hash and skip matched data
            check_match (p, q, len);
            print_match (p-buf+offset, len, p-q);
            mf.update_hash (p, len, m.update_step);
            p += len;
        }
    }
    // END OF MAIN CYCLE
    // ========================================================================

finished:
    stat_only (printf("\nTables %d * %d = %d bytes\n", int(table_count), int(table_sumlen/mymax(table_count,1)), int(table_sumlen)));
    // Return mf/coder error code or mark data end and flush coder
    if (mf.error()    != FREEARC_OK)   return mf.error();
    if (coder.error() != FREEARC_OK)   return coder.error();
    coder.encode (IMPOSSIBLE_LEN, buf, buf-IMPOSSIBLE_DIST, mf.min_length());
    coder.finish();
    return coder.error();
}

// tor_compress template parameterized by MatchFinder and Coder
template <class MatchFinder, class Coder>
int tor_compress0 (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, void *buf0, int bytes_to_compress)
{
    //SET_JMP_POINT( FREEARC_ERRCODE_GENERAL);
    // Make buffer at least 32kb long and round its size up to 4kb chunk
    m.buffer = bytes_to_compress==-1?  (mymax(m.buffer, 32*kb) + 4095) & ~4095  :  bytes_to_compress;
    // If hash is too large - make it smaller
    if (m.hashsize/8     > m.buffer)  m.hashsize     = 1<<lb(m.buffer*8);
    if (m.auxhash_size/8 > m.buffer)  m.auxhash_size = 1<<lb(m.buffer*8);
    // >0: shift data in these chunks, <0: how many old bytes should be kept when buf shifts,
    // -1: don't slide buffer, fill it with new data instead
    m.shift = m.shift?  m.shift  :  (m.hash_row_width>4? m.buffer/4   :
                                     m.hash_row_width>2? m.buffer/2   :
                                     m.hashsize>=512*kb? m.buffer/4*3 :
                                                         -1);
    // Alocate buffer for input data
    void *buf = buf0? buf0 : BigAlloc (m.buffer+LOOKAHEAD);       // use calloc() to make Valgrind happy :)  or can we just clear a few bytes after fread?
    if (!buf)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;

#if 0
    // Create compression threads
    for (int i=0; i<NumThreads; i++)
    {
        CThread t; t.Create(TornadoCompressionThread, &job[i]);
    }
    // Perform I/O and assign compression jobs
    for (int i=0; ; i=(i+1)%NumThreads)
    {
        // Save results of previous compression job
        job[i].Finished.Wait();
        callback ("write", job[i].outbuf, job[i].outsize, auxdata);

        // Read next chunk of data
        int bytes = callback ("read", buf, m.buffer, auxdata);   // Number of bytes read by last "read" call
        if (bytes<0)  return bytes;               // Return errcode on error

        // Send signal to start compression
        job[i].Compress.Signal();
    }
#endif

    // MAIN COMPRESSION FUNCTION
    int result = tor_compress_chunk<MatchFinder,Coder> (m, callback, auxdata, (byte*) buf, bytes_to_compress);

    if (!buf0)  BigFree(buf);
    return result;
}


#ifdef ROLZ_ONLY
// ОНОПНАНБЮРЭ:
// len - ЙНДХПНБЮМХЕ ОН ЯКНРЮЛ
// МЕЯЙНКЭЙН РПЕДНБ match + НДХМ РПЕД encode
// -СВХРШБЮРЭ order0 ЯРЮРХЯРХЙС (РН ЕЯРЭ ДНАЮБКЪРЭ Б ЙЮФДСЧ huftable ЦКНАЮКЭМСЧ ЯРЮР-ЙС Я БЕЯНЛ ЯЙЮФЕЛ 1/128.
//    ДКЪ ЩРНЦН БЕЯРХ ЦКНЧ ЯВ╦РВХЙХ Х ЙНОХПНБЮРЭ ХУ Б КНЙЮКЭМСЧ РЮАКХЖС ОПХ ЙЮФДНЛ ОЕПЕЯВ╦РЕ
//    РНЦДЮ ПЮГМХЖЮ ЦКНА. ЯВ╦РВХЙНБ Х ХУ КНЙЮКЭМНИ ЙНОХХ ДЮЯР ЯВ╦РВХЙХ ДКЪ ОНЯКЕДМЕЦН АКНЙЮ)

#include "../MultiThreading.h"
#include "../MultiThreading.cpp"

#define BUF_SIZE (16*mb)

const int ROLZ_EOF_CODE=256;  // code used to indicate EOF (the same as match with length 0)
const int MATCH_SIZE=64;
const int ENTRIES2 = 64*kb, ROW_SIZE2=4, BASE2=0;
const int ENTRIES3 =  0*kb, ROW_SIZE3=0, BASE3=ENTRIES2*ROW_SIZE2;
const int ENTRIES4 =  0*kb, ROW_SIZE4=0, BASE4=ENTRIES3*ROW_SIZE3+BASE3;
const int TOTAL_ENTRIES = ENTRIES2*ROW_SIZE2 + ENTRIES3*ROW_SIZE3 + ENTRIES4*ROW_SIZE4;
const int MINLEN=1;
inline uint rolz_hashing(BYTE *p, int BYTES, int ENTRIES)
{
    return ENTRIES>=64*kb && BYTES==2?   *(uint16*)(p-BYTES)
         : ENTRIES>=256   && BYTES==1?   *(uint8*) (p-BYTES)
         : ((*(uint*)(p-BYTES) & (((1<<BYTES)<<(BYTES*7))-1)) * 123456791) / (gb/(ENTRIES/4));
}

#define HUFFMAN_TREES                          520
#define HUFFMAN_ELEMS                          (256+(ROW_SIZE2+ROW_SIZE3+ROW_SIZE4)*MATCH_SIZE)
#define ROLZ_ENCODE_DIRECT(context, symbol)    coder.encode(context, symbol)

// Save match to buffer, encoding performed in another thread
#define ROLZ_ENCODE(context, symbol)    (*x++ = ((context)<<16) + (symbol))
// Do actual encode of data item saved in buffer
#define ROLZ_ENCODE2(data)              ROLZ_ENCODE_DIRECT((data)>>16, (data)&0xFFFF)

struct  entry {BYTE *ptr; uint32 cache;};
static  entry hash [TOTAL_ENTRIES];

struct Buffers {uint32 *x0;  int bufsize;};
#define NUMBUFS 4
struct Param {PackMethod m; CALLBACK_FUNC *callback; void *auxdata;
              Buffers             bufarr[NUMBUFS];
              SyncQueue<Buffers*> EncoderJobs;
              SyncQueue<Buffers*> FreeBuffers;
              Event               Finished;
             };

static THREAD_FUNC_RET_TYPE THREAD_FUNC_CALL_TYPE rolz_encode_thread (void *paramPtr)
{
    Param *pa = (Param*)paramPtr;
    HuffmanEncoderOrder1 <HUFFMAN_TREES, HUFFMAN_ELEMS>   coder (pa->callback, pa->auxdata, BUF_SIZE, 4*BUF_SIZE, HUFFMAN_ELEMS+1);
    if (coder.error() != FREEARC_OK)  return coder.error();
    coder.put8 (ROLZ_HUFCODER);
    coder.put8 (0);
    coder.put32(0);

    for(;;)
    {
        Buffers *job = pa->EncoderJobs.Get();  if (job==NULL)  break;
        for (int i=0; i < job->bufsize; i++)
        {
            ROLZ_ENCODE2( job->x0[i]);
        }
        pa->FreeBuffers.Put(job);
        coder.flush();
    }
    coder.finish();
    pa->Finished.Signal();
    return coder.error();
}

int tor_compress_rolz (PackMethod m, CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode = FREEARC_OK;
    zeroArray(hash);
    BYTE *buf = (BYTE*) BigAlloc (BUF_SIZE),  last_char = 0;

    Param pa; pa.m=m, pa.callback=callback, pa.auxdata=auxdata;
    pa.EncoderJobs.SetSize(NUMBUFS+1);  if (pa.EncoderJobs.Error())   return pa.EncoderJobs.Error();
    pa.FreeBuffers.SetSize(NUMBUFS+1);  if (pa.FreeBuffers.Error())   return pa.FreeBuffers.Error();
    for(int i=0; i<NUMBUFS; i++)
    {
        pa.bufarr[i].x0  = (uint32*) BigAlloc (BUF_SIZE*sizeof(uint32));
        pa.FreeBuffers.Put(&(pa.bufarr[i]));
    }
    Thread thread;  thread.Create (rolz_encode_thread, &pa);

    for(;;)
    {
        int bufsize;
        READ_LEN_OR_EOF(bufsize, buf, BUF_SIZE);
        iterate_var(i, TOTAL_ENTRIES)
            hash[i].ptr   = buf,
            hash[i].cache = 0;
        Buffers *job = pa.FreeBuffers.Get();
        uint32 *x0=job->x0, *x=x0;
        BYTE *p=buf;
        ROLZ_ENCODE( last_char, *p);        // First 4 and last MATCH_SIZE bytes of buffer are encoded as literals
        for (; ++p < buf+4; )
            ROLZ_ENCODE( p[-1], *p);
        for (; p<buf+bufsize-MATCH_SIZE; )
        {
            entry *h;  BYTE *q;  uint32 data = *(uint32*)p,  cache;

#define CHECK_MATCH(OFFSET,CUM_OFFSET)                                                               \
          {                                                                                          \
            uint32 t = data ^ cache;                                                                 \
            int len;                                                                                 \
                 if (t&0xffff)   goto try_next##CUM_OFFSET;                                          \
            else if (t&0xff0000) len=2;                                                              \
            else if (t)          len=3;                                                              \
            else for (len=4; len<MATCH_SIZE && p[len]==q[len]; len++);  /* check match length */     \
            ROLZ_ENCODE( p[-1], CUM_OFFSET*MATCH_SIZE + len+256);  p += len;  continue;  try_next##CUM_OFFSET:;    \
          }

#define TRY_FIRST(BYTES,CUM_OFFSET)                                                                  \
            h = &hash[rolz_hashing(p,BYTES,ENTRIES##BYTES)*ROW_SIZE##BYTES + BASE##BYTES];           \
            q = h->ptr;        h->ptr = p;                                                           \
            cache = h->cache;  h->cache = data;                                                      \
            CHECK_MATCH(0,CUM_OFFSET)                                                                \

#define TRY_NEXT(OFFSET,CUM_OFFSET)                                                                  \
          {                                                                                          \
            BYTE *saved_q = q;          uint32 saved_cache = cache;                                  \
            q = (h+OFFSET)->ptr;        (h+OFFSET)->ptr   = saved_q;                                 \
            cache = (h+OFFSET)->cache;  (h+OFFSET)->cache = saved_cache;                             \
            CHECK_MATCH(OFFSET,CUM_OFFSET)                                                           \
          }

            TRY_FIRST(2,0)
            TRY_NEXT(1,1)
            TRY_NEXT(2,2)
            TRY_NEXT(3,3)

            ROLZ_ENCODE( p[-1], *p);  p++;
        }
        for (; p < buf+bufsize; p++)
            ROLZ_ENCODE( p[-1], *p);
        last_char = buf[bufsize-1];
        job->bufsize = x-x0;
        pa.EncoderJobs.Put(job);
    }
finished:

    // Encode EOF
    Buffers *job = pa.FreeBuffers.Get();
    job->x0[0] = (last_char<<16) + ROLZ_EOF_CODE;
    job->bufsize = 1;
    pa.EncoderJobs.Put(job);

    // Finish encoding thread
    pa.EncoderJobs.Put(NULL);
    pa.Finished.Wait();
    for(int i=0; i<NUMBUFS; i++)  BigFree(pa.bufarr[i].x0);  BigFree(buf);
    return 0;
}

int tor_decompress0_rolz (CALLBACK_FUNC *callback, void *auxdata)
{
    int errcode = FREEARC_OK;                             // Error code of last "write" call
    zeroArray(hash);
    static HuffmanDecoderOrder1 <HUFFMAN_TREES, HUFFMAN_ELEMS>   decoder (callback, auxdata, BUF_SIZE, HUFFMAN_ELEMS+1);
    if (decoder.error() != FREEARC_OK)  return decoder.error();
    BYTE  buf[BUF_SIZE+MATCH_SIZE],  *p=buf,  last_char=0;
    BYTE *cycle_h[MATCH_SIZE],  dummy[MATCH_SIZE];  iterate(MATCH_SIZE, cycle_h[i] = dummy);  int i=0;
    static BYTE *hash_ptr[TOTAL_ENTRIES];

    for (;;)
    {
        uint idx = rolz_hashing(p,3,ENTRIES3);         // ROLZ entry
        entry *h = &hash[idx];

        uint c = decoder.decode(last_char);
        if (c < 256) {
            *p++ = c;
        } else if (c==ROLZ_EOF_CODE) {
            break;
        } else {
            BYTE *match  =  hash_ptr[idx] >= p-MATCH_SIZE? hash_ptr[idx] : (BYTE *)&h[0];
            int len      = c-256;
            do   *p++ = *match++;
            while (--len);
        }

        // Save match bytes to ROLZ hash (ЯН ЯДБХЦНЛ МЮ 16 ЛЮРВЕИ МЮГЮД ОНЯЙНКЭЙС АЮИРШ МЕ ЯПЮГС ОНЪБКЪЧРЯЪ ;)
        memcpy (cycle_h[i], p, MATCH_SIZE);
        hash_ptr[idx] = p;         // ОНЯКЕ WRITE ЯРЮМЕР МЕЙНППЕЙРМШЛ, МСФЕМ hash_ptr_level ОНЙЮГШБЮЧГХИ ЯЙНКЭЙН БУНФДЕМХИ ДЮММНЦН idx ЯЕИВЮЯ ЯНДЕПФХРЯЪ Б cycle_h
        cycle_h[i]    = (BYTE *)&h[0];
        i = (i+1) % MATCH_SIZE;

        // Save context for order-1 coder and flush buffer if required
        last_char = p[-1];
        if (p-buf >= BUF_SIZE)
            {WRITE (buf, BUF_SIZE);  p=buf;}
    }
    WRITE (buf, p-buf);            // Flush outbuf
finished:
    return errcode;
}
#endif  // ROLZ_ONLY


template <class MatchFinder, class Coder>
int tor_compress4 (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, void *buf, int bytes_to_compress)
{
    switch (m.match_parser) {
    case GREEDY: return tor_compress0 <             MatchFinder,  Coder> (m, callback, auxdata, buf, bytes_to_compress);
    case LAZY:   return tor_compress0 <LazyMatching<MatchFinder>, Coder> (m, callback, auxdata, buf, bytes_to_compress);
    }
}

template <class MatchFinder, class Coder>
int tor_compress3 (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, void *buf, int bytes_to_compress)
{
    switch (m.hash3) {
    case 0: return tor_compress4 <MatchFinder, Coder> (m, callback, auxdata, buf, bytes_to_compress);
    case 1: return tor_compress4 <Hash3<MatchFinder,12,10,FALSE>, Coder> (m, callback, auxdata, buf, bytes_to_compress);
    case 2: return tor_compress4 <Hash3<MatchFinder,16,12,TRUE >, Coder> (m, callback, auxdata, buf, bytes_to_compress);
    }
}

template <class MatchFinder>
int tor_compress2 (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, void *buf, int bytes_to_compress)
{
    switch (m.encoding_method) {
    case STORING:   // Storing - go to any tor_compress2 call
    case BYTECODER: // Byte-aligned encoding
                    return tor_compress3 <MatchFinder, LZ77_ByteCoder>                          (m, callback, auxdata, buf, bytes_to_compress);
    case BITCODER:  // Bit-precise encoding
                    return tor_compress3 <MatchFinder, LZ77_BitCoder>                           (m, callback, auxdata, buf, bytes_to_compress);
    case HUFCODER:  // Huffman encoding
                    return tor_compress3 <MatchFinder, LZ77_Coder <HuffmanEncoder<EOB_CODE> > > (m, callback, auxdata, buf, bytes_to_compress);
    case ARICODER:  // Arithmetic encoding
                    return tor_compress3 <MatchFinder, LZ77_Coder <ArithCoder<EOB_CODE> >     > (m, callback, auxdata, buf, bytes_to_compress);
    }
}

template <class MatchFinder>
int tor_compress2d (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, void *buf, int bytes_to_compress)
{
    return tor_compress3 <MatchFinder, LZ77_DynamicCoder> (m, callback, auxdata, buf, bytes_to_compress);
}

// Compress data using compression method m and callback for i/o
int tor_compress (PackMethod m, CALLBACK_FUNC *callback, void *auxdata, void *buf, int bytes_to_compress)
{
#ifdef ROLZ_ONLY
    return tor_compress_rolz (m, callback, auxdata);
#else
// When FULL_COMPILE is defined, we compile all the 4*8*3*2=192 possible compressor variants
// Otherwise, we compile only 8 variants actually used by -0..-11 predefined modes
#ifdef FULL_COMPILE
    switch (m.caching_finder) {
    case 7:  if (m.hash_row_width > 256)  return FREEARC_ERRCODE_INVALID_COMPRESSOR;
             return tor_compress2d <CombineMF <CycledCachingMatchFinder<7>, CycledCachingMatchFinder<4> > > (m, callback, auxdata, buf, bytes_to_compress);
    case 6:  if (m.hash_row_width > 256)  return FREEARC_ERRCODE_INVALID_COMPRESSOR;
             return tor_compress2d <CombineMF <CycledCachingMatchFinder<6>, CycledCachingMatchFinder<4> > > (m, callback, auxdata, buf, bytes_to_compress);
    case 5:  if (m.hash_row_width > 256)  return FREEARC_ERRCODE_INVALID_COMPRESSOR;
             return tor_compress2d <CombineMF <CycledCachingMatchFinder<5>, ExactMatchFinder<4> > >         (m, callback, auxdata, buf, bytes_to_compress);
    case 2:  if (m.hash_row_width > 256)  return FREEARC_ERRCODE_INVALID_COMPRESSOR;
             return tor_compress2d <CycledCachingMatchFinder<4> > (m, callback, auxdata, buf, bytes_to_compress);
    case 1:  return tor_compress2  <CachingMatchFinder<4> >       (m, callback, auxdata, buf, bytes_to_compress);

    default: switch (m.hash_row_width) {
             case 1:    return tor_compress2 <MatchFinder1>     (m, callback, auxdata, buf, bytes_to_compress);
             case 2:    return tor_compress2 <MatchFinder2>     (m, callback, auxdata, buf, bytes_to_compress);
             default:   return tor_compress2 <MatchFinderN<4> > (m, callback, auxdata, buf, bytes_to_compress);
             }
    }
#else
    // -1..-5(-6)
    if (m.encoding_method==BYTECODER && m.hash_row_width==1 && m.hash3==0 && !m.caching_finder && m.match_parser==GREEDY ||
        m.encoding_method==STORING ) {
        return tor_compress0 <MatchFinder1, LZ77_ByteCoder> (m, callback, auxdata, buf, bytes_to_compress);
    } else if (m.encoding_method==BITCODER && m.hash_row_width==1 && m.hash3==0 && !m.caching_finder && m.match_parser==GREEDY ) {
        return tor_compress0 <MatchFinder1, LZ77_BitCoder > (m, callback, auxdata, buf, bytes_to_compress);
    } else if (m.encoding_method==HUFCODER && m.hash_row_width==2 && m.hash3==0 && !m.caching_finder && m.match_parser==GREEDY ) {
        return tor_compress0 <MatchFinder2, LZ77_Coder< HuffmanEncoder<EOB_CODE> > > (m, callback, auxdata, buf, bytes_to_compress);
    } else if (m.encoding_method==HUFCODER && m.hash_row_width>=2 && m.hash3==0 && m.caching_finder && m.match_parser==GREEDY ) {
        return tor_compress0 <CachingMatchFinder<4>, LZ77_Coder< HuffmanEncoder<EOB_CODE> > > (m, callback, auxdata, buf, bytes_to_compress);
    } else if (m.encoding_method==ARICODER && m.hash_row_width>=2 && m.hash3==1 && m.caching_finder==1 && m.match_parser==LAZY ) {
        return tor_compress0 <LazyMatching<Hash3<CachingMatchFinder<4>,12,10,FALSE> >, LZ77_Coder<ArithCoder<EOB_CODE> > > (m, callback, auxdata, buf, bytes_to_compress);
    // -5 -c3 - used for FreeArc -m4$compressed
    } else if (m.encoding_method==HUFCODER && m.hash_row_width>=2 && m.hash3==1 && m.caching_finder==1 && m.match_parser==LAZY ) {
        return tor_compress0 <LazyMatching<Hash3<CachingMatchFinder<4>,12,10,FALSE> >, LZ77_Coder< HuffmanEncoder<EOB_CODE> > > (m, callback, auxdata, buf, bytes_to_compress);

    // -7..-9
    } else if (m.hash_row_width>=2 && m.hash3==2 && m.caching_finder==5 && m.match_parser==LAZY ) {
        return tor_compress0 <LazyMatching <CombineMF <CycledCachingMatchFinder<5>, Hash3<ExactMatchFinder<4>,16,12,TRUE> > >,
                              LZ77_DynamicCoder > (m, callback, auxdata, buf, bytes_to_compress);
    // -10 and -11
    } else if (m.hash_row_width>=2 && m.hash3==2 && m.caching_finder==6 && m.match_parser==LAZY ) {
        return tor_compress0 <LazyMatching <CombineMF <CycledCachingMatchFinder<6>, Hash3<CycledCachingMatchFinder<4>,16,12,TRUE> > >,
                              LZ77_DynamicCoder > (m, callback, auxdata, buf, bytes_to_compress);
    } else if (m.hash_row_width>=2 && m.hash3==2 && m.caching_finder==7 && m.match_parser==LAZY ) {
        return tor_compress0 <LazyMatching <CombineMF <CycledCachingMatchFinder<7>, Hash3<CycledCachingMatchFinder<4>,16,12,TRUE> > >,
                              LZ77_DynamicCoder > (m, callback, auxdata, buf, bytes_to_compress);

    } else {
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;
    }
#endif
#endif  // ROLZ_ONLY
}

#endif // FREEARC_DECOMPRESS_ONLY

// LZ77 decompressor ******************************************************************************

// If condition is true, write data to outstream
#define WRITE_DATA_IF(condition)                                                                  \
{                                                                                                 \
    if (condition) {                                                                              \
        if (decoder.error() != FREEARC_OK)  goto finished;                                        \
        tables.undiff_tables (write_start, output);                                               \
        debug (printf ("==== write %08x:%x ====\n", write_start-outbuf+offset, output-write_start)); \
        WRITE (write_start, output-write_start);                                                  \
        tables.diff_tables (write_start, output);                                                 \
        write_start = output;  /* next time we should start writing from this pos */              \
                                                                                                  \
        /* Check that we should shift the output pointer to start of buffer */                    \
        if (output >= outbuf + bufsize) {                                                         \
            offset_overflow |= (offset > (uint64(1) << 63));                                      \
            offset      += output-outbuf;                                                         \
            write_start -= output-outbuf;                                                         \
            write_end   -= output-outbuf;                                                         \
            tables.shift (output,outbuf);                                                         \
            output      -= output-outbuf;  /* output = outbuf; */                                 \
        }                                                                                         \
                                                                                                  \
        /* If we wrote data because write_end was reached (not because */                         \
        /* table list was filled), then set write_end into its next position */                   \
        if (write_start >= write_end) {                                                           \
            /* Set up next write chunk to HUGE_BUFFER_SIZE or until buffer end - whatever is smaller */ \
            write_end = write_start + mymin (outbuf+bufsize-write_start, HUGE_BUFFER_SIZE);       \
        }                                                                                         \
    }                                                                                             \
}


template <class Decoder>
int tor_decompress0 (CALLBACK_FUNC *callback, void *auxdata, int _bufsize, int minlen)
{
    //SET_JMP_POINT (FREEARC_ERRCODE_GENERAL);
    int errcode = FREEARC_OK;                             // Error code of last "write" call
    Decoder decoder (callback, auxdata, _bufsize);        // LZ77 decoder parses raw input bitstream and returns literals&matches
    if (decoder.error() != FREEARC_OK)  return decoder.error();
    uint bufsize = tornado_decompressor_outbuf_size (_bufsize);  // Size of output buffer
    BYTE *outbuf = (byte*) BigAlloc (bufsize+PAD_FOR_TABLES*2);  // Circular buffer for decompressed data
    if (!outbuf)  return FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;
    outbuf += PAD_FOR_TABLES;       // We need at least PAD_FOR_TABLES bytes available before and after outbuf in order to simplify datatables undiffing
    BYTE *output      = outbuf;     // Current position in decompressed data buffer
    BYTE *write_start = outbuf;     // Data up to this point was already writen to outsream
    BYTE *write_end   = outbuf + mymin (bufsize, HUGE_BUFFER_SIZE); // Flush buffer when output pointer reaches this point
    if (compress_all_at_once)  write_end = outbuf + bufsize + 1;    // All data should be written after decompression finished
    uint64 offset = 0;                    // Current outfile position corresponding to beginning of outbuf
    int offset_overflow = 0;              // Flags that offset was overflowed so we can't use it for match checking
    DataTables tables;                    // Info about data tables that should be undiffed
    for (;;) {
        // Check whether next input element is a literal or a match
        if (decoder.is_literal()) {
            // Decode it as a literal
            BYTE c = decoder.getchar();
            print_literal (output-outbuf+offset, c);
            *output++ = c;
            WRITE_DATA_IF (output >= write_end);  // Write next data chunk to outstream if required

        } else {
            // Decode it as a match
            UINT len  = decoder.getlen(minlen);
            UINT dist = decoder.getdist();
            print_match (output-outbuf+offset, len, dist);

            // Check for simple match (i.e. match not requiring any special handling, >99% of matches fall into this category)
            if (output-outbuf>=dist && write_end-output>len) {
                BYTE *p = output-dist;
                do   *output++ = *p++;
                while (--len);

            // Check that it's a proper match
            } else if (len<IMPOSSIBLE_LEN) {
                // Check that compressed data are not broken
                if (dist>bufsize || len>2*_bufsize || (output-outbuf+offset<dist && !offset_overflow))  {errcode=FREEARC_ERRCODE_BAD_COMPRESSED_DATA; goto finished;}
                // Slow match copying route for cases when output-dist points before buffer beginning,
                // or p may wrap at buffer end, or output pointer may run over write point
                BYTE *p  =  output-outbuf>=dist? output-dist : output-dist+bufsize;
                do {
                    *output++ = *p++;
                    if (p==outbuf+bufsize)  p=outbuf;
                    WRITE_DATA_IF (output >= write_end);
                } while (--len);

            // Check for special len/dist code used to encode EOF
            } else if (len==IMPOSSIBLE_LEN && dist==IMPOSSIBLE_DIST) {
                WRITE_DATA_IF (TRUE);  // Flush outbuf
                goto finished;

            // Otherwise it's a special code used to represent info about diffed data tables
            } else {
                len -= IMPOSSIBLE_LEN;
                if (len==0 || dist*len > 2*_bufsize)  {errcode=FREEARC_ERRCODE_BAD_COMPRESSED_DATA; goto finished;}
                stat_only (printf ("\n%d: Start %x, end %x, length %d      ", len, int(output-outbuf+offset), int(output-outbuf+offset+len*dist), len*dist));
                // Add new table to list: len is row length of table and dist is number of rows
                tables.add (len, output, dist);
                // If list of data tables is full then flush it by preprocessing
                // and writing to outstream already filled part of outbuf
                WRITE_DATA_IF (tables.filled() && !compress_all_at_once);
            }
        }
    }
finished:
    BigFree(outbuf-PAD_FOR_TABLES);
    // Return decoder error code, errcode or FREEARC_OK
    return decoder.error() < 0 ?  decoder.error() :
           errcode         < 0 ?  errcode
                               :  FREEARC_OK;
}


int tor_decompress (CALLBACK_FUNC *callback, void *auxdata, void *buf, int bytes_to_compress)
{
    int errcode;
    // First 6 bytes of compressed data are encoding method, minimum match length and buffer size
    BYTE header[2];       READ (header, 2);
   {uint encoding_method = header[0];
    uint minlen          = header[1];
    uint bufsize;         READ4 (bufsize);

    switch (encoding_method) {
#ifndef ROLZ_ONLY
    case BYTECODER:
            return tor_decompress0 <LZ77_ByteDecoder> (callback, auxdata, bufsize, minlen);

    case BITCODER:
            return tor_decompress0 <LZ77_BitDecoder>  (callback, auxdata, bufsize, minlen);

    case HUFCODER:
            return tor_decompress0 <LZ77_Decoder <HuffmanDecoder<EOB_CODE> > > (callback, auxdata, bufsize, minlen);

    case ARICODER:
            return tor_decompress0 <LZ77_Decoder <ArithDecoder<EOB_CODE> >   > (callback, auxdata, bufsize, minlen);

#else
    case ROLZ_HUFCODER:
            return tor_decompress0_rolz (callback, auxdata);
#endif

    default:
            errcode = FREEARC_ERRCODE_BAD_COMPRESSED_DATA;
    }}
finished: return errcode;
}


/*
LZ77 model:
    -no lz if len small and dist large: don't have much sense with our MINLEN=4
    -hash4+3: only 1% gain even on ghc.exe
    -hash5+4: 48->46.7 mb but 2x slower (22->46sec: 240mb compressed using 16mb hash)
    -0x65a8e9b4 for hash
    +combined len+dist encoding a-la cabarc - will make decoding a bit faster, but who cares? :)
    -save into hash records unused part of hash value in order to make
        fast check of usability of this hash slot (like it is already
        done in REP); would be especially helpful on larger hashes
    -save into hash record 4 bits of p[5] - would be useful to skip trying second..fourth hash records
    +save into hash record 4 bytes of data
    +lazy search (and return of 3-byte strings) for highest compression mode
+l8... - ДНАЮБХКН 1 КХЬМЧЧ ЯЕЙСМДС МЮ НАПЮАНРЙС ЙЮФДШУ 280 ЛА
+compare with ideal hash function crc+crc+..
    (((CRCTab[(x)&255] ^ _rotr(CRCTab[((x)>>8)&255],8) ^ _rotr(CRCTab[((x)>>16)&255],16) ^ _rotr(CRCTab[((x)>>24)&255],24)) >> HashShift) & HashMask)
+store unused hash bits + a few more chars in hash   (1.5x speedup)
    491->367 (340 for hash4x), 91->68, 51->43 secs
    +ХЯОНКЭГНБЮРЭ ОЕПБШИ АЮИР ОНД УЕЬ 4У АЮИРНБ
    +НРДЕКЭМШЕ ЖХЙКШ ДКЪ len=3,4,5,6
    +ХЯОНКЭГСЪ t, АШЯРПН ОПНБЕПЪРЭ ЛЮРВХ ДКХМШ ДН 7 Б ЖХЙКЮУ len3..5 Х ОПХ ОПНБЕПЙЕ ОЕПБНИ ЯРПНЙХ
    ОПНБЕПХРЭ ГЮМНБН ДКХМШ ЯНБОЮДЕМХИ ЯРПНЙ Б УЕЬ-ЖЕОНВЙЕ
+fast arithmetics! total=2^n
    НРДЕКЭМШИ АСТЕП ДКЪ ВРЕМХЪ АХРНБШУ ОНКЕИ; ХКХ КСВЬЕ bits+arith Б НДМНЛ ОНРНЙЕ ДЮММШУ
+lazy matches                                        (+3.5% compression)
    unsuccessfully tried:
      ush good_length; - reduce lazy search above this match length
      ush max_lazy;    - do not perform lazy search above this match length
      ush nice_length; - quit search above this match length
+arith / huffman / bitio                         (+10% compresion for bit i/o, +20% for huffman)
    byte i/o -> class: +0.3 sec on !all
+3-byte strings
+БШЙХДШБЮРЭ ЙНПНРЙХЕ ДЮК╦ЙХЕ ЯРПНЙХ
    +ЛНФМН СКСВЬХРЭ ЯФЮРХЕ МЮ 0.3% ЕЯКХ БШЙХДШБЮРЭ ЕЫ╦ Х 6-АЮИРНБШЕ ЯРПНЙХ
+better hash multiplier
-5% less compression of src (l4 h22) compared to mmdet. strange?
-several encoding tables: after char, after small string, large string
-add custom MF for l=4/8 (3/6?) what means -1 sec. on !all
    don't have much meaning because caching MF isn't any worser
+FIXED: MatchFinder2 МЕЯНБЛЕЯРХЛ Я 3-АЮИРНБШЛХ ЯКНБЮЛХ / lazy matching (update_hash ПЮЯЯВХРЮМН МЮ НАМНБКЕМХЪ ЙЮЙ ЛХМХЛСЛ Б 3 АЮИРЮ)
+FAST_COMPILE - only 4 models actually used by -1..-12
+ЯДЕКЮРЭ hash_row_width ВЮЯРЭЧ ЙКЮЯЯЮ MatchFinder
+FIXED: caching MF - МЕВ╦РМШЕ ЯКНБЮ ДНКФМШ ХМХЖХЮКХГХПНБЮРЭЯЪ ЯНДЕПФХЛШЛ МЮВЮКЮ АСТЕПЮ
+sliding window for higher modes (-4/-5 - m.buffer/2, -6 and up - m.buffer/4)
+write data to outstreams in 16mb chunks
+64k-1m non-sliding window for -1..-3
+improved caching MF - memory accesses only for matches>=7 or last check
-max_lazy may improve speed/ratio for -4..-6 modes
-don't check more than one real string (option? only for 2-element hash?)
    -skip checking second string if first is large enough
+[almost] full hash_update for highest modes
+IMPOSSIBLE_LEN/IMPOSSIBLE_DIST for EOF encoding, encode() for first 2 chars
+FIXED: -s- -p2 problem (was returning len==0 instead of MINLEN-1)
-ОПХ lazy ОНХЯЙЕ СВХРШБЮРЭ ДКХМС ОПЕД. ЛЮРВЮ, ОПНОСЯЙЮЪ 3-АЮИРНБШИ Х ВЮЯРЭ 4-АЮИРНБНЦН ОНХЯЙЮ
+TOO_FAR checks moved into caching MF
+output buffer now flushed only when reading next input chunk
+tor_(de)compress - returns error code or FREEARC_OK
+freearc: АКНЙХПНБЮРЭ РПЕД ВРЕМХЪ ОПХ ГЮОХЯХ ДЮММШУ
+7z's lazy heuristic
  +ОПХ ОНХЯЙЕ ЯРПНЙХ - if newlen=len+1 and newdist>dist*64 - ignore it
+2-byte strings, +repdist, +repboth, +repchar
+НАПЮАНРЙЮ ЛЮКЕМЭЙХУ ТЮИКНБ!
+БНЯЯРЮМНБХРЭ bytecoder
  +large len - a few bytes representation to ensure no overflows
+auto-decrease hash (and buf) for small files
+СДКХМЪРЭ МЮГЮД next match Б lazy matcher
-repdistN+-delta - 0.4% МЮ РЕЙЯРЮУ
+HuffmanEncoder::encode2
+fixed: ХЯОНКЭГНБЮМХЕ Б ОПНБЕПЙЕ МЮ REPCHAR ХМХЖХЮКХГЮЖХНММНЦН ГМЮВЕМХЪ repdist0=1
        ХЯОНКЭГНБЮМХЕ ОЯЕБДНДХЯРЮМЖХХ НР MMx ДКЪ ОПНБЕПЙХ МЮ REPCHAR (СВРХ: ДЕЙНДЕП ДНКФЕМ ХЛЕРЭ РС ФЕ НВЕПЕДЭ ОНЯКЕДМХУ ДХЯРЮМЖХИ)
        ОЕПЕУНД diffed table ВЕПЕГ ЯДБХЦ АСТЕПЮ
          БНЯЯРЮМНБКЕМХЕ ДЮММШУ ДНКФМН ДЕКЮРЭЯЪ ОНЯКЕ НАПЮРМНЦН diff, ХМЮВЕ ЩРНР diff ГЮОХЬЕР ЛСЯНП Б ЩКЕЛЕМР, ЯКЕДСЧЫХИ ГЮ БНЯЯРЮМНБКЕММШЛ
        ХЯОНКЭГНБЮМХЕ p->table_len БЛЕЯРН НАПЕГЮММНЦН len
        write_end ЛНЦ БШУНДХРЭ ГЮ ЦПЮМХЖС АСТЕПЮ
        read_next_chunk ДНКФЕМ БНГБПЮЫЮРЭ 0 ЕЯКХ АНКЭЬЕ ЯФХЛЮРЭ МЕВЕЦН (ОНЯКЕДМХИ ЛЮРВ ДНАХК ДН ЙНМЖЮ СФЕ ОПНВХРЮММШУ ДЮММШУ Х МНБШУ ОПНВЕЯРЭ МЕ СДЮКНЯЭ)
        101..104 МЕ ЯНБЯЕЛ ЮЙЙСПЮРМН ХЯОНКЭГНБЮКЯЪ ДКЪ data table codes
-context-based char encoding
  separate coder table after \0 or after \0..\31
+diffing tables
-repboth, repchar1..3
-split caching hash into two parts - pointers and data
  +cyclic hash for large N
+ChangePair in MFN
  -ChangePair for len1-len2>1
ОПХ ДНЯРЮРНВМН ДКХММНЛ Х ДЮК╦ЙНЛ ЛЮРВЕ БШЙХДШБЮРЭ ЕЦН ХГ УЕЬЮ Б ОПЕДОНКНФЕМХХ, ВРН РЕЙСЫЮЪ ЯРПНЙЮ ЕЦН ОПЕЙПЮЯМН ГЮЛЕМХР
  -ДЕКЮРЭ ЯДБХЦ НРДЕКЭМН, ОНЯКЕ ЖХЙКЮ ОНХЯЙЮ ЛЮРВЕИ (ОНОПНАНБЮМН ОПХ МЕПЮГДЕК╦ММНЛ CMF)
block-static arithmetic coder - may improve compression by 1-2%
? caching MF ДКЪ -l2
+ 5/6-byte main hash for highest modes (-7 and up)
hash3+lazy - ЯЙНЛАХМХПНБЮРЭ Б ДПСЦНЛ ОНПЪДЙЕ, ОНЯЙНКЭЙС МЕР ЯЛШЯКЮ ХЯЙЮРЭ 3-АЮИРНБСЧ ЯРПНЙС ОНЯКЕ ЛЮРВЮ?
ГЮОНКМХРЭ ЙНМЕЖ АСТЕПЮ ЯКСВЮИМШЛХ ДЮММШЛХ Х САПЮРЭ ОПНБЕПЙХ p+len<bufend
  ГЮЛЕМХРЭ ОПНБЕПЙХ p+len<=bufend НДМНИ Б compress0()
НЦПЮМХВХРЭ ОПНБЕПЪЕЛСЧ ДХЯРЮМЖХЧ Б -1/-2/-3? ВРНАШ МЕ БШКЕГЮРЭ ГЮ ПЮГЛЕП ЙЕЬЮ
rolz 1+2+3+4
minor thoughts:
  small outbuf for -5 and higher modes
  increase HUFBLOCKSIZE for -2/-3  (100k - -0.2sec)

text files -5/-6: disable 2/3-byte searching, repchar and use encode(..., MINLEN=4), switch to hufcoder(?)
hufcoder: disable REPDIST, +fast qsort<>
huf&ari: EOB, check for text-like stats, switch into text mode

use only one bit for flag in bytecoder
bitcoder: 30-bit length encoding - make it a part of 8-bit encoding
huf/ari - improve "first block" encoding, adaptation (currently, up to 1/64 of codespace is wasted),
  +EOB code
? БШБНДХРЭ ДЮММШЕ АКНЙЮЛХ, ЯННРБЕРЯРБСЧЫХЛХ БУНДМШЛ chunks, storing МЕЯФЮБЬХУЯЪ АКНЙНБ
    header = 1 byte flags + 3 bytes len
АНКЕЕ ДЕРЮКХГХПНБЮММШЕ disttables ДКЪ ЛЮКЕМЭЙХУ len
-1,-2,-3?: +no MM, no REP*
huf/ari: БЛЕЯРН cnt++ ДЕКЮРЭ cnt+=10 - ДНКФМН СБЕКХВХРЭ РНВМНЯРЭ ЙНДХПНБЮМХЪ (ЩРН СБЕКХВХБЮЕР ПЮГЛЕП РЮАКХЖ, ВРН ГЮЛЕДКЪЕР ЙНДХПНБЮМХЕ; БНГЛЯНФМН, ОПНАКЕЛС ЛНФМН ПЕЬХРЭ ХЯОНКЭГНБЮМХЕЛ 3-СПНБМЕБШУ РЮАКХЖ ЙНДХПНБЮМХЪ)
ST4/BWT sorting for exhaustive string searching

СЯЙНПЕМХЕ tor:5
  -СЯЙНПЕМХЕ lazy ОНХЯЙЮ (йЮДЮВ)
  СЯЙНПЕМХЕ ЯПЮБМЕМХЪ ЛЮРВЕИ (ХДЕЪ ЮБРНПЮ QuickLZ)
  -ХЯЙЮРЭ MM tables by rep* codes
  НОРХЛХГХПНБЮРЭ huf Х ОЕПЕИРХ МЮ МЕЦН
  ДКЪ РЕЙЯРНБ:
    МЕ ХЯОНКЭГНБЮРЭ 2/3-byte matches
    ХЯОНКЭГНБЮРЭ huf c АНКЭЬХЛ АКНЙНЛ БЛЕЯРН ЮПХТЛЕРХЙХ
    МЕ ОПНБЕПЪРЭ МЮ repchar/repdist/repboth
    МЕ ХЯЙЮРЭ MM tables

СЯЙНПЕМХЕ/СКСВЬЕМХЕ ЯФЮРХЪ tor:7-12
  +ХЯОНКЭГНБЮРЭ АЕЯЯДБХЦНБСЧ РЕУМНКНЦХЧ УЕЬХПНБЮМХЪ Х -u1
  +2/3hash: СБЕКХВХРЭ ПЮГЛЕП, БЯРЮБКЪРЭ БЯЕ ЯРПНЙХ
  +ХЯЙЮРЭ Б АНКЭЬНЛ УЕЬЕ ЯРПНЙХ ДКХМШ >=6/7, ЯОХУМСБ ЛЕМЭЬХЕ БН БЯОНЛНЦЮР. УЩЬ
  ОПНОСЯЙЮРЭ ЯХЛБНКШ 0/' ' ОПХ УЕЬХПНБЮМХХ
  check matches at repdist distances

http://encode.ru/threads/848-LZ77-speed-optimization-2-mem-accesses-per-quot-round-quot by Lasse Reinhold
    Nice, I tried almos the same thing. Of course caching byte 3...6 (and 1?) is mostly an advantage when you want to
    find the longest match out of N possibe because you don&#039;t save the match verification of byte 0..2.

    For finding best match out of N possible (N being 8 in this sample code), I once experimented with caching byte
    0...7 on x64 and looped trough them like:

    long long diff_best = 0, best_i = 0;
    for(i = 1; i < 8; i++)
    {
    long long diff = cache[hash][i] ^ *ptr_input;
    if (diff & -diff > diff_best)
    {
    best_i = i;
    best_diff = diff;
    }
    }

    It utilizes that x & -x returns a word where only the lowest bit in x is set (see http://www.jjj.de/bitwizardry/
    for more code snippets) and it&#039;s a good alternative to using shr/shb bit scan instructions of ARM, x86, x64,
    etc, which unfortunatly isn&#039;t standard in C.

    I just got a ~10% speedup compared to the more naive method of just comparing byte N of *ptr_>input with byte N of
    cache[hash][i] where N is the length of the best match so far, excluding worse matches immediately. I tough speedup
    would be greater and it&#039;s probably worth looking into again.

    "only lowest bit is set" should have been "only lowest set bit is set"


+-h1mb in cmdline
+-z/-d options, by default auto depending on file extension
+-h1m -9 == -9 -h1m (СВХРШБЮРЭ ЯМЮВЮКЮ БШАНП ОПЕЯЕРЮ, ГЮРЕЛ СРНВМЪЧЫХЕ ЕЦН НОЖХХ)
+-odir/ -odir\ -od:
+64-bit insize/outsize
+-b128k, m.hashsize БЛЕЯРН hashlog, print block/hashsize in help with k/m suffix
+CHECK mallocs
+dir_exists=file_exists(dir\.) || end_with(:/\)
+progress indicator in console title
-t, -f force overwrite, -k keep src files, stdin->stdout by default
make non-inline as much functions as possible (optimize .exe size): +MatchFinder.cpp +LZ77_Coder.cpp
****Tornado 0.2 compressing VC, 41243 kb     --noheader option disables this
****-1: 16kb hash1...: done 5%
****-1: 16kb hash1...: 17876 kb (12.7%), 23.333 sec, 88.6 mb/s
.tor signature, version, flags, crc
? ГЮОХЯШБЮРЭ ЯФЮРШЕ ДЮММШЕ ОЕПЕД ВРЕМХЕЛ ЯКЕДСЧЫЕЦН chunk Х ХЯОНКЭГНБЮРЭ storing ОПХ НРЯСРЯРБХХ ЯФЮРХЪ (НАМСКЪРЭ huf/ari-table)
? СЛЕМЭЬХРЭ УЕЬ МЮГЮД БДБНЕ (ЯМЮВЮКЮ ОПНБЕПХРЭ ЩТТЕЙР МЮ ДПСЦХУ ТЮИКЮУ, 200-300 kb МЮ all)
+print predefined methods definitions in help screen
-mem ДНКФМН ДЕЛНМЯРПХПНБЮРЭ ПЕФХЛШ ЯФЮРХЪ НР -1 ДН -9?  -bench ДКЪ ЛНХУ БМСРПЕММХУ РЕЯРНБ
tor_compress: ОПХ ЯФЮРХХ ТЮИКЮ ==buffer ОПНХЯУНДХР КХЬМХИ ОЕПЕМНЯ ДЮММШУ ОЕПЕД РЕЛ, ЙЮЙ ОПНВЕЯРЭ 0 АЮИР :)

Changes in 0.2:
    lazy parsing
    3-byte matches
    huffman coder
    sliding window

Changes in 0.3:
    repdist&repchar0 codes
    2-byte matches
    optimized lz parsing
    table preprocessing
    gzip-like cmdline interface?

    -1 thor e1, quicklz
    -2 thor e2, slug
    -3 thor e3, gzip -1
    -4 gzip, rar -m1
    -5 thor, 7zip -mx1
    -6 uharc -mz
    -7 bzip2, rar -m2

Changes in 0.4:
    Cyclic caching MF (makes -9..-11 modes faster)
    Full 2/3-byte hashing in -9..-11 modes which improved compression a bit
    Improved console output to provide more information

*/
