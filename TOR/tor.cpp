// GPL'ed code of Tornado - fast LZ77-based compression algorithm.
// (c) Bulat Ziganshin <Bulat.Ziganshin@gmail.com>

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "../Compression.h"
#include "bitio.cpp"
#include "huffman.cpp"
#include "MatchFinder.cpp"

// Compression method parameters
struct PackMethod
{
    int encoding_method;   // Coder (0 - storing, 1 - bytecoder, 2 - bitcoder, 3 - huffman, 4 - arithmetic)
    int hash_row_width;    // Length of hash row
    int hashlog;           // Hash size log
    int caching_finder;    // Force/prohibit using caching match finder
    int buffer;            // Block size

    // Return recommendations - whether to use caching match finder for this method
    int use_caching_finder()
    {
        return (caching_finder==1  ||  caching_finder!=-1 && hash_row_width>=4 && buffer>=8*mb);
    }
};

// Preconfigured compression modes
PackMethod std_method[] = { { 0, 0,   0,  0, 1*mb   }
                          , { 1, 1,   12, 0, 1*mb   }
                          , { 2, 2,   14, 0, 2*mb   }
                          , { 4, 2,   14, 0, 4*mb   }
                          , { 4, 2,   15, 0, 8*mb   }
                          , { 4, 2,   18, 0, 16*mb  }
                          , { 4, 4,   20, 0, 32*mb  }
                          , { 4, 8,   21, 0, 64*mb  }
                          , { 4, 16,  22, 0, 128*mb }
                          , { 4, 32,  23, 0, 128*mb }
                          , { 4, 64,  24, 0, 128*mb }
                          , { 4, 128, 25, 0, 128*mb }
                          , { 4, 256, 26, 0, 128*mb }
                          };


// tor_compress template parameterized by MatchFinder and Coder
template <class MatchFinder, class Coder>
int tor_compress3 (PackMethod m, BYTE *buf, int bufsize, BYTE *outbuf, int outbufsize)
{
    if (m.encoding_method) {
        // Use first output bytes to store encoding method and first input byte
        outbuf[0] = m.encoding_method;
        outbuf[1] = *buf;
        // Initialize match finder and coder
        MatchFinder mf (buf, m.hashlog, m.hash_row_width);
        Coder coder (outbuf+2, outbufsize-2);
        void *bufend = buf + bufsize;

        // Find and encode matches until buffer end
        for (BYTE *p=buf+1; p<bufend; ) {
            // Find match length and position
            UINT len = mf.find_matchlen (p, bufend, m.hash_row_width);
            BYTE *q  = mf.get_matchptr();
            // Check that outbuf still has free room (+flush flags if needed)
            if (coder.overflow())
                goto storing;                               // no more space in outbuf
            // Encode either match or literal
            if (!coder.encode (len, p, q, MINLEN)) {
                print_literal (p-buf, *p); p++; continue;   // literal encoded
            }
            // Update hash and skip matched data
            check_match (p, q, len);                        // match encoded
            print_match (p-buf, len, p-q);
            mf.update_hash (p, len, m.hash_row_width);
            p += len;
        }
        return coder.finish()+2;
    } else {
storing:  // Either storing requested or data turn out to be incompressible - store them anyway
        outbuf[0] = 0;
        memcpy (outbuf+1, buf, bufsize);
        return bufsize+1;
    }
}


// tor_compress template parameterized by Coder
template <class Coder>
int tor_compress2 (PackMethod m, BYTE *buf, int bufsize, BYTE *outbuf, int outbufsize)
{
    if (m.use_caching_finder())   // Use caching match finder if suggested
        return tor_compress3 <CachingMatchFinderN, Coder> (m, buf, bufsize, outbuf, outbufsize);
    else switch (m.hash_row_width) {
    case 1: return tor_compress3 <MatchFinder1, Coder> (m, buf, bufsize, outbuf, outbufsize);
    case 2: return tor_compress3 <MatchFinder2, Coder> (m, buf, bufsize, outbuf, outbufsize);
    default:return tor_compress3 <MatchFinderN, Coder> (m, buf, bufsize, outbuf, outbufsize);
    }
}

// Compress data from buf to outbuf using compression method m
int tor_compress (PackMethod m, BYTE *buf, int bufsize, BYTE *outbuf, int outbufsize)
{
    switch (m.encoding_method) {
    case 0: // Storing - go to any tor_compress2 call
    case 1: // Byte-aligned encoding
            return tor_compress2 <LZ77_ByteCoder>  (m, buf, bufsize, outbuf, outbufsize);
    case 2: // Bit-precise encoding
            return tor_compress2 <LZ77_BitCoder>   (m, buf, bufsize, outbuf, outbufsize);
    case 4: // Arithmetic encoding
            return tor_compress2 <LZ77_ArithCoder> (m, buf, bufsize, outbuf, outbufsize);
    }
}


// LZ77 decompressor ******************************************************************************
int tor_decompress (BYTE *buf, int bufsize, BYTE *outbuf, int outsize)
{
    void *bufend = outbuf + outsize;
    BYTE *input = buf, *output = outbuf;
    // First two bytes of compressed data are encoding method and first input char
    uint encoding_method = *input++;
    *output++ = *input++;

    switch (encoding_method) {
    case 0: // Stored data
            memcpy (output, input, bufsize-2);
            output += bufsize-2;
            break;

    case 1: // Bytecode
            while (output<bufend) {
                UINT flags = value32(input);  input+=4;
                for (int i=17; --i; flags>>=2) {
                    unsigned len, dist;
                    switch (flags&3) {
                    case 0:  print_literal (output-outbuf, *input); *output++ = *input++; continue;
                    case 1:  dist = value16(input); input+=2; len = (dist>>12)+MINLEN; dist %= 1<<12; break;
                    case 2:  dist = value24(input); input+=3; len = (dist>>18)+MINLEN; dist %= 1<<18; break;
                    case 3:  len = *input++;  int addlen = 0;
                             if (len==255)  { dist = (*input++) << 24, len = *input++; }  else dist = 0;
                             while (len==254)  { len = *input++; addlen += 254; }
                             len += addlen+MINLEN;
                             dist += value24(input); input+=3;
                             break;
                    }
                    print_match (output-outbuf, len, dist);
                    BYTE *p = output-dist;
                    do   *output++ = *p++;
                    while (--len);
                }
            }
            break;

    case 2: // Bitcode
            {
            LZ77_BitDecoder decoder (input, bufsize-2);
            while (output<bufend) {
                // Check next input element and decode it either as literal or match
                if (decoder.is_literal()) {
                    byte c = decoder.getchar();
                    print_literal (output-outbuf, c);
                    *output++ = c;
                } else {
                    uint len  = decoder.getlen(MINLEN);
                    uint dist = decoder.getdist();
                    print_match (output-outbuf, len, dist);
                    BYTE *p = output-dist;
                    do   *output++ = *p++;
                    while (--len);
                }
            }
            break;
            }

    case 4: // Arithmetic code
            {
            LZ77_ArithDecoder decoder (input, bufsize-2);
            while (output<bufend) {
                // Check next input element and decode it either as literal or match
                if (decoder.is_literal()) {
                    byte c = decoder.getchar();
                    print_literal (output-outbuf, c);
                    *output++ = c;
                } else {
                    uint len  = decoder.getlen(MINLEN);
                    uint dist = decoder.getdist();
                    print_match (output-outbuf, len, dist);
                    BYTE *p = output-dist;
                    do   *output++ = *p++;
                    while (--len);
                }
            }
            break;
            }
    }
    return output - outbuf;
}


#ifndef TOR_LIBRARY
// DRIVER ************************************************************************

#include "../Standalone.h"

// Coder names for humans
const char *encoding_name[] = {"storing", "bytecoder", "bitcoder", "hufcoder", "aricoder"};

// Returns human-readable method description
char *name (PackMethod method)
{
    static char namebuf[100];
    int c = method.encoding_method;
    int l = method.hash_row_width;
    int h = method.hashlog;
    int b = method.buffer;
    int x = method.use_caching_finder();
    sprintf (namebuf, "%d%cb hash%d%s, block %dmb, %s",
                     h>17? 1<<(h-18) : 1<<(h-8), h>17? 'm':'k',
                     l, x?"x":"", b/mb, encoding_name[c]);
    return namebuf;
}

// Structure for recording compression statistics and zero record of this type
struct Results {int insize, outsize; double time;} r0;

// Print compression statistics gathered in r
void print_stats (char *model_name, Results r)
{
    char title[500];
    sprintf (title, " %-11s: %5d kb (%3.1lf%%)", model_name, r.outsize/1000,
                                                 (double)r.outsize*100/r.insize);
    printf( "%s: %.3lf seconds\n", title, r.time);
}

int main (int argc, char **argv)
{
    // Decompress instead of compress?
    int unpack = 0;

    // Benchmark mode?
    int benchmark = 0;

    // Default compression parameters are equivalent to option -5
    PackMethod method = std_method[5];

    if (argv[1] && stricmp(argv[1],"-d")==0) {
        unpack=1;
        argv++, argc--;
    } else {
        while (argv[1] && argv[1][0] == '-') {
                 if (stricmp(argv[1]+1,"mem")==0)    benchmark=1, method.buffer=INT_MAX;
            else if (stricmp(argv[1]+1,"b")==0)      method.buffer = INT_MAX;
            else if (stricmp(argv[1]+1,"x")==0)      method.caching_finder = 1;
            else if (stricmp(argv[1]+1,"x+")==0)     method.caching_finder = 1;
            else if (stricmp(argv[1]+1,"x-")==0)     method.caching_finder = -1;
            else if (isdigit(argv[1][1]))            method = std_method [atoi(argv[1]+1)];
            else switch( tolower(argv[1][1]) ) {
                case 'c':   method.encoding_method = atoi(argv[1]+2); break;
                case 'l':   method.hash_row_width  = atoi(argv[1]+2); break;
                case 'h':   method.hashlog         = atoi(argv[1]+2); break;
                case 'b':   method.buffer          = atoi(argv[1]+2) * mb; break;
                default :   printf( "\n Unknown option '%s'\n", argv[1]);
                            exit(1);
            }
            argv++, argc--;
        }
    }
    if (argc != 2  &&  argc != 3) {
        printf( "Tornado compressor v0.1 (c) Bulat.Ziganshin@gmail.com  16.04.2007");
        printf( "\n" );
        printf( "\n Usage: tor [options] original-file [packed-file]");
        printf( "\n   -#    --  select predefined compression profile (1..9)");
        printf( "\n   -c#   --  coder (1-bytes,2-bits,4-arith), default %d", method.encoding_method);
        printf( "\n   -l#   --  length of hash row (1..9999), default %d", method.hash_row_width);
        printf( "\n   -h#   --  hash elements log (8..28), default %d", method.hashlog);
        printf( "\n   -b#   --  block size in mb, default %d", method.buffer/mb);
        printf( "\n   -x[-] --  force/prohibit using caching match finder");
        printf( "\n" );
        printf( "\n For decompression: tor -d packed-file [unpacked-file]");
        printf( "\n" );
        printf( "\n Just for fun: tor -mem large-file");
        printf( "\n" );
        exit(1);
    }

    FILE *fin = fopen( argv[1], "rb" );
    if (fin == NULL) {
        printf( "\n Can't open %s for read\n", argv[1]);
        exit(2);
    }

    // Construct output filename if no one is given on cmdline:
    // on compressing - add .tor
    // on unpacking   - remove .tor (and add .untor if file already exists)
    char outname[1000];
    if (argv[2]) {
        strcpy (outname, argv[2]);
    } else {
        if (!unpack) {
            sprintf(outname, "%s.tor", argv[1]);
        } else {
            strcpy (outname, argv[1]);
            if (end_with (outname, ".tor"))
                outname [strlen(outname)-4] = '\0';
            if (file_exists (outname))
                strcat(outname, ".untor");
        }
    }

    // Open output file
    FILE *fout = fopen (benchmark? "nul" : outname, "wb");
    if (fout == NULL) {
        printf( "\n Can't open %s for write\n", argv[2]);
        exit(5);
    }

    int filesize = filelength(fileno(fin));
    int bufsize  = min (method.buffer, filesize);
    method.buffer = bufsize;
    // Alloc input and output buffers
    BYTE *buf    = (BYTE*) malloc(bufsize+1000);
    BYTE *outbuf = (BYTE*) malloc(bufsize+1000);
    if (!buf || !outbuf) {
        printf( "\n Can't alloc %u mbytes\n", bufsize>>19);
        exit(8);
    }

    // Print header
    if (benchmark) {
        printf( "Benchmarking %d kb, %s:  0%%", filesize/1000, encoding_name[method.encoding_method]);
    } else if (!unpack) {
        printf ("%s: compressing %d kb:  0%%", name(method), filesize/1000);
    } else {
        printf("Decompressing %d kb:  0%%", filesize/1000);
    }


    // MAIN COMPRESSOR CYCLE THAT PROCESS DATA BLOCK BY BLOCK
    const int MINL=1, MAXL=32, MINH=12, MAXH=22;  // Range of compression parameters values tested in benchmarking mode
    Results r = r0, benchres[MAXL+1][MAXH+1];     // Accumulators for compression statistics
    for (int first_pass=TRUE; TRUE; first_pass=FALSE) {
        // Read next input block unless we are in unpacking mode
        unsigned bytes = !unpack? fread (buf, 1, bufsize, fin) : 1;
        if (bytes==0)  break;
        if (bytes<0) {
            printf( "\n Error %d while reading input file", bytes);
            exit(7);
        }

        if (benchmark) {
        // Benchmark mode
            if (r.insize+bytes == filesize)  printf( "\rBenchmarked %d kb, %s       \n", filesize/1000, encoding_name[method.encoding_method]);
            for (int l=MINL; l<=MAXL; l*=2) {
                for (int h=(l>2? 18:MINH); h<=(l<2? 14:MAXH); h++, h>18 && h++) {
                    // Compress data block with given l/h and save interim model results to benchres[l][h]
                    method.hash_row_width = l;
                    method.hashlog        = h;
                    r = first_pass? r0 : benchres[l][h];
                    init_timer();
                    int compressed = tor_compress (method, buf, bytes, outbuf, bytes);
                    r.time    += timer();
                    r.insize  += bytes;
                    r.outsize += compressed;
                    benchres[l][h] = r;

                    // Print either a progress indicator or (for last block) total model stats
                    if (r.insize<filesize) {
                        int bytes_processed = r.insize-bytes + bytes/((MAXL-MINL+1)*(MAXH-MINH+1)) * ((l-MINL)*(MAXH-MINH+1)+h-MINH+1);
                        printf("\b\b\b%2d%%", int(double(bytes_processed)*100/filesize));
                    } else {
                        char title[100];
                        sprintf (title, "%3d%cb hash%d%s", h>17? 1<<(h-18) : 1<<(h-8), h>17? 'm':'k', l, method.use_caching_finder()? "x":"");
                        print_stats (title, r);
                    }
                }
                if (r.insize==filesize && l!=MAXL)  printf ("\n");
            }


        } else if (!unpack) {
        // Compression mode
            init_timer();
            int compressed = tor_compress (method, buf, bytes, outbuf+8, bytes);
            r.time += timer();
            // Save sizes of original & compressed block for decompressor
            r.insize  += *(int*)outbuf     = bytes;
            r.outsize += *(int*)(outbuf+4) = compressed;
            write (fout, outbuf, compressed+8);


        } else {
        // Decompression mode
            // Read header of next block of compressed data
            unsigned bytes = read (fin, buf, 8);
            if (bytes==0)  break;
            if (bytes!=8) {
                printf( bytes>0? "Truncated input file" : "\n Error %d while reading input file", bytes);
                exit(7);
            }
            unsigned outsize   = *(int*)buf;
            unsigned blocksize = *(int*)(buf+4);
            if (max(blocksize,outsize) > bufsize) {
                free (buf);  free (outbuf);
                // Alloc larger input/output buffers
                bufsize = max(blocksize,outsize);
                buf    = (BYTE*) malloc(bufsize+1000);
                outbuf = (BYTE*) malloc(bufsize+1000);
                if (!buf || !outbuf) {
                    printf( "\n Can't alloc %u mbytes\n", bufsize>>19);
                    exit(8);
                }
            }

            // Read contents of compressed data block
            bytes = read (fin, buf, blocksize);
            if (bytes!=blocksize) {
                printf( bytes>0? "Error in input data" : "\n Error %d while reading input file", bytes);
                exit(7);
            }

            // Decompress block
            init_timer();
            tor_decompress (buf, blocksize, outbuf, outsize);
            r.time    += timer();
            r.insize  += blocksize;
            r.outsize += outsize;
            write (fout, outbuf, outsize);
        }
        if (r.insize!=filesize)  printf("\b\b\b%2d%%", int(double(r.insize)*100/filesize));
    }

    // Print (de)compression stats
    if (benchmark) {
    } else if (!unpack) {
        char title[100];
        sprintf(title, "\r%s: compressed %d -> %d kb (%3.1lf%%)", name(method), r.insize/1000, r.outsize/1000, (double)r.outsize*100/r.insize);
        printf( "%s: %.3lf seconds\n", title, r.time);
    } else {
        char title[100];
        sprintf(title, "\rDecompressed %d -> %d kb (%3.1lf%%)", r.insize/1000, r.outsize/1000, (double)r.insize*100/r.outsize);
        printf( "%s: %.3lf seconds\n", title, r.time);
    }
    fclose (fin);
    fclose (fout);
    free (buf);
    free (outbuf);
    return 0;
}

#endif

/*
+l8... - добавило 1 лишнюю секунду на обработку каждых 280 мб
+compare with ideal hash function crc+crc+..
    (((CRCTab[(x)&255] ^ _rotr(CRCTab[((x)>>8)&255],8) ^ _rotr(CRCTab[((x)>>16)&255],16) ^ _rotr(CRCTab[((x)>>24)&255],24)) >> HashShift) & HashMask)
+store unused hash bits + a few more chars in hash   (1.5x speedup)
    491->367 (340 for hash4x), 91->68, 51->43 secs
    +использовать первый байт под хеш 4х байтов
    +отдельные циклы для len=3,4,5,6
    +используя t, быстро проверять матчи длины до 7 в циклах len3..5 и при проверке первой строки
    проверить заново длины совпадений строк в хеш-цепочке

+fast arithmetics! total=2^n
    отдельный буфер для чтения битовых полей; или лучше bit_arith в одном потоке данных
lazy matches                                        (+10% compression?)
    ush good_length; - reduce lazy search above this match length
    ush max_lazy;    - do not perform lazy search above this match length
    ush nice_length; - quit search above this match length
arith / huffman.cpp / bitio                         (+10% compresion for bit i/o, +20% for huffman)
    byte i/o -> class: +0.3 sec on !all
2/3-byte strings
выкидывать короткие далёкие строки
инициализировать hash нулями вместо buf и выходить из цикла поиска при достижении нулей
    вполне возможно, что это позволит удалить нынешнее ограничение (buffer>=8mb) для использования
      caching MF и тем самым значительно упростить логику программы (выбор MF будет задаваться
      непосредственно в профиле в соответствии с другими параметрами)
caching MF для -l2

better hash multiplier
sliding window
use only one bit in bytecoder
    large len - a few bytes representation to ensure no overflows
опускать слишком далёкие 4-байтовые строки
-h1mb in cmdline
mask[] for selecting lower bits
bitcoder: 30-bit length encoding - make it a part of 8-bit encoding
-mem должно демонстрировать режимы сжатия от -1 до -9?  -bench для моих внутренних тестов
заполнить конец буфера случайными данными и убрать проверки p+len<bufend
rolz 1+2+3+4
при достаточно длинном и далёком матче выкидывать его из хеша в предположении, что текущая строка его прекрасно заменит
    делать сдвиг отдельно, после цикла поиска матчей
add custom MF for l=4/8 (3/6?) what means -1 sec. on !all
    don't have much meaning because caching MF isn't any worser
*/
