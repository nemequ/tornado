// GPL'ed code for byte/bit/huffman/arithmetic encoders for LZ77 output
// (c) Bulat Ziganshin <Bulat.Ziganshin@gmail.com>


// LZ77 literal/match bytecoder *******************************************************************

struct LZ77_ByteCoder : OutputByteStream
{
    // Used to save literal/match flags, grouped by 16 values = 32 bits
    uint32   *flagpos;    // Address where current flags should written once filled
    uint     flags;       // Flags word
    uint     flagbit;     // Current bit in flags word (flags are filled when flagbit==2^32)

    // Encoding statistics
    int chars, matches2, matches3, matches4;

    // Init and finish encoder, returning number of bytes written
    LZ77_ByteCoder (void *_buf, int _size);
    int finish (void);

    // Save final value of flags word into reserved place of outbuf when all 32 flag bits are filled
    // and return 1 if buffer overflowed
    int overflow (void)
    {
        if ((flagbit<<=2) == 0) {   // 1u<<32 for 64-bit systems
            // Flags are now filled, save the old value and start a new one
            *flagpos=flags, flags=0, flagbit=1;
            flagpos=(uint32*)output, output+=4;
            return OutputByteStream::overflow();
        }
        return 0;
    }

    // Writes match/literal into output. Returns 0 - literal encoded, 1 - match encoded
    int encode (int len, byte *current, byte *match, const int MINLEN)
    {
        if (len<MINLEN) {
            stat (chars++);
            putc (*current);
            return 0;
        }
        uint dist = current - match;
        if (len<MINLEN+16 && dist<(1<<12)) {
            stat (matches2++);
            put16 (((len-MINLEN)<<12) + dist);
            flags += flagbit;    // Mark this position as short match
        } else if (len<MINLEN+64 && dist<(1<<18)) {
            stat (matches3++);
            put24 (((len-MINLEN)<<18) + dist);
            flags += flagbit*2;  // Mark this position as medium-length match
        } else {
            stat (matches4++);
            if (dist >= (1<<24))      putc (255), putc (dist>>24);
            while (len>=MINLEN+254)   putc (254), len-=254;
            put32 ((len-MINLEN) + (dist<<8));
            flags += flagbit*3;  // Mark this position as long match
        }
        return 1;
    }
};

LZ77_ByteCoder::LZ77_ByteCoder (void *_buf, int _size) : OutputByteStream (_buf, _size)
{
    chars = matches2 = matches3 = matches4 = 0;
    // Start a flags business
    flags   = 0;
    flagbit = 0;
    flagpos = (uint32*)_buf;
}

int LZ77_ByteCoder::finish (void)
{
#ifdef STAT
    printf ("\rLiterals %d, matches %d = %d + %d + %d                   \n",
        chars/1000, (matches2+matches3+matches4)/1000, matches2/1000, matches3/1000, matches4/1000);
#endif
    *flagpos = flags;
    return (byte*)output - (byte*)buf;
}


// Variable-length data coder *********************************************************************

// We support up to a 256 codes, which encodes values
// up to a 2^30 (using encoding one can find in DistanceCoder)
#define MAX_CODE 256
#define VLE_SIZE (1024+16384)
struct VLE
{
    uchar xcode       [VLE_SIZE];     // Code for each (preprocessed) value
    uint  xextra_bits [MAX_CODE];     // Amount of extra bits for each code
    uint  xbase_value [MAX_CODE];     // Base (first) value for each code

    VLE (uint _extra_bits[], uint extra_bits_size);

    uint code (uint value)
    {
        return xcode[value];
    }
    uint extra_bits (uint code)
    {
        return xextra_bits[code];
    }
    uint base_value (uint code)
    {
        return xbase_value[code];
    }
};

// Inits array used for the fast value->code mapping.
// Each entry in extra_bits[] corresponds to exactly one code
// and tells us how many additional bits are used with this code.
// So, extra_bits_size == number of codes used
VLE::VLE (uint _extra_bits[], uint extra_bits_size)
{
    // Initialize the mappings value -> code and code -> base value
    uint value = 0;
    for (uint code = 0; code < extra_bits_size; code++) {
        xextra_bits[code] = _extra_bits[code];
        xbase_value[code] = value;
        for (uint n = 0; n < (1<<xextra_bits[code]); n++) {
            if (value>=VLE_SIZE)  break;
            xcode[value++] = (uchar)code;
        }
    }
}


// Extra bits for each length code (for bitcoder and aricoder, accordingly)
uint extra_lbits [8]  = {0,0,0,1,2,4,8,30};
uint extra_lbits2[16] = {0,0,0,0,0,0,0,1,1,2,2,3,3,4,8,30};

// Variable-length encoder for LZ match lengths
template <unsigned ELEMENTS>
struct LengthCoder : VLE
{
    LengthCoder (uint _extra_bits[])  :  VLE (_extra_bits, ELEMENTS) {};

    uint code (uint length)
    {
        return length>600?  ELEMENTS-1  :  VLE::code(length);
    }
};

LengthCoder <elements(extra_lbits)>   lc  (extra_lbits);
LengthCoder <elements(extra_lbits2)>  lc2 (extra_lbits2);


// Extra bits for each distance code
//uint extra_dbits[16] = {6,6,7,8,9,10,11,12,13,14,15,16,17,19,22,30};
uint extra_dbits[32] = {4,4,5,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,17,18,19,21,23,30};

// Variable-length encoder for LZ match distances up to 1Gb
struct DistanceCoder : VLE
{
    DistanceCoder();

    uint code (uint distance)
    {
        return distance < 512     ? xcode[distance] :
               distance < 512*256 ? xcode[512+(distance>>8)]
                                  : xcode[1024+(distance>>16)];
    }
} dc;

// Distance coder has its own init routine which take accout of
// wide range of encoded values and therefore more complex
// code() routine having 3 branches
DistanceCoder::DistanceCoder () : VLE (0,0)
{
    /* Initialize the mapping dist (0..1G) -> dist code (0..15) */
    uint dist = 0, code;
    for (code = 0; dist < 512; code++) {
        xextra_bits[code] = extra_dbits[code];
        xbase_value[code] = dist;
        for (uint n = 0; n < (1<<extra_dbits[code]); n++) {
            xcode[dist++] = (uchar)code;
        }
    }
    dist >>= 8; /* from now on, all distances are divided by 256 */
    for ( ; dist < 512; code++) {
        xextra_bits[code] = extra_dbits[code];
        xbase_value[code] = dist << 8;
        for (uint n = 0; n < (1<<(extra_dbits[code]-8)); n++) {
            xcode[512 + dist++] = (uchar)code;
        }
    }
    dist >>= 8; /* from now on, all distances are divided by 65536 */
    for ( ; code < elements(extra_dbits); code++) {  // distances up to 1G
        xextra_bits[code] = extra_dbits[code];
        xbase_value[code] = dist << 16;
        for (uint n = 0; n < (1<<(extra_dbits[code]-16)); n++) {
            if (dist>=elements(xcode)-1024)  break;
            xcode[1024 + dist++] = (uchar)code;
        }
    }
}


// LZ77 literal/match bit-precise coder ***********************************************************

// It's the coder
struct LZ77_BitCoder
{
    OutputBitStream b;   // Bit stream to where all data go
    int chars, matches, lencnt[8], distcnt[32];  // Encoding statistics

    // Init and finish encoder, returning number of bytes written
    LZ77_BitCoder (void *_buf, int _size);
    int finish (void);

    // Check if buffer is overflowed
    uint overflow (void)  {return b.overflow();}

    // Writes match/literal into buffer. Returns 0 - literal encoded, 1 - match encoded
    int encode (int len, byte *current, byte *match, const int MINLEN)
    {
        // Encode a literal if match is too short
        if ((len-=MINLEN) < 0)  {
            stat (chars++);
            b.putbits (9, *current);
            return 0;
        }

        // It's a match
        stat (matches++);
        uint dist = current - match;

        // Find len code
        uint lcode = lc.code(len);
        uint lbits = lc.extra_bits(lcode);
        uint lbase = lc.base_value(lcode);
        stat (lencnt[lcode]++);

        // Find dist code
        uint dcode = dc.code(dist);
        uint dbits = dc.extra_bits(dcode);
        uint dbase = dc.base_value(dcode);
        stat (distcnt[dcode]++);

        // Send combined len/dist code and remaining bits
        b.putbits (9, 256 + (lcode<<5) + dcode);
        b.putlowerbits (lbits, len-lbase);
        b.putlowerbits (dbits, dist-dbase);
        return 1;
    }
};

LZ77_BitCoder::LZ77_BitCoder (void *_buf, int _size) : b(_buf,_size)
{
    iterate_var(i,8)   lencnt[i]  = 0;
    iterate_var(i,32)  distcnt[i] = 0;
    chars = matches = 0;
}

int LZ77_BitCoder::finish (void)
{
#ifdef STAT
    printf ("\rLiterals %d, matches %d. Length codes:", chars/1000, matches/1000);
    iterate_var(i,8)   printf (" %d", lencnt[i]/1000);
    printf ("\n");
    iterate_var(i,32)  printf (" %d", distcnt[i]/1000);
    printf ("\n");
#endif
    return b.finish();
}



// And that's the decoder
struct LZ77_BitDecoder
{
    InputBitStream b;   // Bit stream from where all data are read

    // Init decoder
    LZ77_BitDecoder (void *_buf, int _size) : b(_buf,_size) {};

    uint x;  // Temporary value used for storing first 9 bits of code

    // Decode next element and return true if it's a literal
    uint is_literal (void)
    {
        x = b.getbits(9);
        return (x < 256);
    }
    // Decode literal
    uint getchar (void)
    {
        return x;
    }
    // Decode length (should be called before getdist!)
    uint getlen (const uint MINLEN)
    {
        uint lcode = (x>>5)-8;
        uint lbits = lc.extra_bits(lcode);
        uint lbase = lc.base_value(lcode);
        return MINLEN + lbase + b.getbits(lbits);
    }
    // Decode distance
    uint getdist (void)
    {
        uint dcode = x & 31;
        uint dbits = dc.extra_bits(dcode);
        uint dbase = dc.base_value(dcode);
        return dbase + b.getbits(dbits);
    }
};


// LZ77 literal/match arithmetic coder ************************************************************

// It's the coder
struct LZ77_ArithCoder
{
    ArithCoder b;
    int chars, matches, lencnt[32], distcnt[32];  // Encoding statistics

    // Init and finish encoder, returning number of bytes written
    LZ77_ArithCoder (void *_buf, int _size);
    int finish (void);

    // Check if buffer is overflowed
    uint overflow (void)  {return b.overflow();}

    // Writes match/literal into buffer. Returns 0 - literal encoded, 1 - match encoded
    int encode (int len, byte *current, byte *match, const int MINLEN)
    {
        // Encode a literal if match is too short
        if ((len-=MINLEN) < 0)  {
            stat (chars++);
            b.encode (*current);
            return 0;
        }

        // It's a match
        stat (matches++);
        uint dist = current - match;

        // Find len code
        uint lcode = lc2.code(len);
        uint lbits = lc2.extra_bits(lcode);
        uint lbase = lc2.base_value(lcode);
        stat (lencnt[lcode]++);

        // Find dist code
        uint dcode = dc.code(dist);
        uint dbits = dc.extra_bits(dcode);
        uint dbase = dc.base_value(dcode);
        stat (distcnt[dcode]++);

        // Send combined len/dist code and remaining bits
        b.encode (256 + lcode*elements(extra_dbits) + dcode);
        b.putlowerbits (lbits, len-lbase);
        b.putlowerbits (dbits, dist-dbase);
        return 1;
    }
};

LZ77_ArithCoder::LZ77_ArithCoder (void *_buf, int _size) :
    b (_buf, _size, 256 + elements(extra_lbits2)*elements(extra_dbits))
{
    iterate_var(i,32)  lencnt[i]  = 0;
    iterate_var(i,32)  distcnt[i] = 0;
    chars = matches = 0;
}

int LZ77_ArithCoder::finish (void)
{
#ifdef STAT
    printf ("\rLiterals %d, matches %d. Length codes:", chars/1000, matches/1000);
    iterate_var(i,elements(extra_lbits2))  printf (" %d", lencnt[i]/1000);
    printf ("\n");
    iterate_var(i,32)                      printf (" %d", distcnt[i]/1000);
    printf ("\n");
#endif
    return b.finish();
}



// And that's the decoder
struct LZ77_ArithDecoder
{
    ArithDecoder b;

    // Init decoder
    LZ77_ArithDecoder (void *_buf, int _size) :
        b (_buf, _size, 256 + elements(extra_lbits2)*elements(extra_dbits)) {}

    uint x;  // Temporary value used for storing first 9 bits of code

    // Decode next element and return true if it's a literal
    uint is_literal (void)
    {
        x = b.decode();
        return (x < 256);
    }
    // Decode literal
    uint getchar (void)
    {
        return x;
    }
    // Decode length (should be called before getdist!)
    uint getlen (const uint MINLEN)
    {
        uint lcode = (x>>5)-8;
        uint lbits = lc2.extra_bits(lcode);
        uint lbase = lc2.base_value(lcode);
        return MINLEN + lbase + b.getbits(lbits);
    }
    // Decode distance
    uint getdist (void)
    {
        uint dcode = x & 31;
        uint dbits = dc.extra_bits(dcode);
        uint dbase = dc.base_value(dcode);
        return dbase + b.getbits(dbits);
    }
};


