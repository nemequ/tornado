// GPL'ed code for byte-aligned/bit-aligned/huffman/arith I/O streams
// (c) Bulat Ziganshin <Bulat.Ziganshin@gmail.com>


#define mask(x,n) ((x) & ((1<<(n))-1))

// Byte-aligned I/O streams ***********************************************************************

struct OutputByteStream
{
    void  *buf;        // Buffer start
    byte  *output;     // Current pos
    void  *bufend;     // Buffer end

    OutputByteStream (void *_buf, int _size)
    {
        buf    = _buf;
        output = (byte*)_buf;
        bufend = (byte*)_buf + _size;
    }
    int  finish()         {return (byte*)output - (byte*)buf;}    // Finish working, flush any pending data and return number of bytes written
    int  overflow()       {return output>bufend;}                 // Returns true if buffer is full
    void putc   (uint c)  {*output++ = c;}                        // Writes 8-64 bits to the buffer
    void put16  (uint c)  {*(uint16*)output = c; output+=2;}
    void put24  (uint c)  {*(uint32*)output = c; output+=3;}
    void put32  (uint c)  {*(uint32*)output = c; output+=4;}
    void put64  (uint c)  {*(uint64*)output = c; output+=8;}
    void putword(uint c)  {*(uint  *)output = c; output+=sizeof(uint);}  // Writes machine-size word
};


struct InputByteStream
{
    void  *buf;        // Buffer start
    byte  *input;      // Current pos
    void  *bufend;     // Buffer end

    InputByteStream (void *_buf, int _size)
    {
        buf    = _buf;
        input  = (byte*)_buf;
        bufend = (byte*)_buf + _size;
    }

    uint getc   ()  {return *input++;}                        // Reads 8-64 bits from the buffer
/*
    uint get16  ()  {*(uint16*)output = c; output+=2;}
    uint get24  ()  {*(uint32*)output = c; output+=3;}
    uint get32  ()  {*(uint32*)output = c; output+=4;}
    uint get64  ()  {*(uint64*)output = c; output+=8;}
    uint getword()  {*(uint  *)output = c; output+=sizeof(uint);}  // Reads machine-size word
*/
};


// Bit-aligned I/O streams ***********************************************************************

#define CHAR_BIT       8
#define CHUNK          (sizeof(uint))

// It's an output bit stream
struct OutputBitStream : OutputByteStream
{
    uint  bitbuf;      // Bit buffer - written to *p++ when filled
    int   bitcount;    // Count of lower bits already filled in current bitbuf

    // Init and finish bit stream, returning number of bytes written
    OutputBitStream (void *_buf, int _size);
    int finish (void);

    // Write n lower bits of x
    void putbits (int n, uint x)
    {
        //Tracevv((stderr,"\nPut %2d bits of %04x",n,x));
        bitbuf |= x << bitcount;
        bitcount += n;
        if( bitcount >= CHAR_BIT*CHUNK ) {
            putword (bitbuf);
            bitcount -= CHAR_BIT*CHUNK;
            bitbuf = x >> (n-bitcount);
        }
    }

    // Write n lower bits of x
    void putlowerbits (int n, uint x)
    {
        putbits (n, mask(x,n));
    }
};

// Skip a few bytes at the start and end of buffer in order to align it like it aligned on disk
#define eat_at_start 0
#define eat_at_end   0

OutputBitStream::OutputBitStream (void *_buf, int _size) : OutputByteStream (_buf, _size)
{
    bitbuf   = 0;
    bitcount = CHAR_BIT*eat_at_start;
}

int OutputBitStream::finish ()
{
    putword (bitbuf);
    return OutputByteStream::finish();
}


// And it's an input bit stream
struct InputBitStream
{
    void    *buf;        // Buffer start
    int     size;        // Buffer size
    uint32  *p;          // Current pos
    uint64  bitbuf;      // Bit buffer - filled by reading from *p++
    int     bitcount;    // Count of higher bits not yet filled in current bitbuf

    // Init and finish bit stream, returning number of bytes written
    InputBitStream (void *_buf, int _size);

    // Read next word from buffer
    uint32 getword (void)  {return *p++;}

    // Ensure that bitbuf contains at least n valid bits
    void needbits (int n)
    {
        if (bitcount<=32)
            bitbuf |= uint64(getword()) << bitcount, bitcount+=32;
    }
    // Throw out n used bits
    void dumpbits (int n)
    {
        bitbuf >>= n;
        bitcount -= n;
    }
    // Read next n bits of input
    uint getbits (int n)
    {
        needbits(n);
        uint x = mask(bitbuf,n);
        dumpbits(n);
        return x;
    }
};

InputBitStream::InputBitStream (void *_buf, int _size)
{
    buf  = _buf;
    size = _size;
    p    = (uint32*)buf;
    bitbuf   = 0;
    bitcount = 0;
}


// Shindler's rangecoder **************************************************************************

class TEncoder : public OutputByteStream
{
private:
  long long low; // 64-bit extended integer
  unsigned int range;
  unsigned int buffer;
  unsigned int help;

  inline void ShiftLow() {
    if ((low ^ 0xff000000) >= (1 << 24)) {
      unsigned int c = static_cast<unsigned int>(low >> 32);
      putc(buffer + c);
      c += 255;
      for (; help > 0; help--)
        putc(c);
      buffer = static_cast<unsigned int>(low) >> 24;
    }
    else
      help++;
    low = static_cast<unsigned int>(low) << 8;
  }

public:
  TEncoder (void *_buf, int _size):
    OutputByteStream (_buf, _size),
    low(0), range(0xffffffff), buffer(0), help(0) {}

  void Encode(unsigned int cum, unsigned int cnt, unsigned int bits) {
    low += (cum * (range >>= bits));
    range *= cnt;
    while (range < (1 << 24)) {
      range <<= 8;
      ShiftLow();
    }
  }

  // Finish working, flush any pending data and return number of bytes written
  int finish()
  {
    for (int i = 0; i < 5; i++)
      ShiftLow();
    return OutputByteStream::finish();
  }
};


class TDecoder : InputByteStream
{
private:
  unsigned int range;
  unsigned int buffer;

public:
  TDecoder (void *_buf, int _size) : InputByteStream (_buf, _size), range(0xffffffff)
  {
    for (int i = 0; i < 5; i++)
      buffer = (buffer << 8) + getc();
  }

  unsigned int GetCount(unsigned int bits)
  {
    unsigned int count = buffer / (range >>= bits);
    if (count >= (1<<bits))
      fprintf(stderr, "data error\n"), exit(1);
    return (count);
  }

  void Update(unsigned int cum, unsigned int cnt)
  {
    buffer -= (cum * range);
    range *= cnt;
    while (range < (1 << 24)) {
      range <<= 8;
      buffer = (buffer << 8) + getc();
    }
  }
};


// Semi-adaptive arithmetic coder ******************************************************************
//   This coder updates counters after (de)coding each symbol,
//   but keeps to use previous encoding. After some amount of
//   symbols encoded it recalculates encoding using counters
//   gathered to this moment. So, each time it uses for encoding
//   stats of previous block. Actually, counters for new block
//   are started from 3/4 of previous counters, so algorithm
//   is more resistant to temporary statistic changes

#define NUM        (256+32*32+2)    /* maximum number of symbols + 1 */
#define INDEXES    2048             /* amount of indexes used for fast searching in cum[] table */
#define RANGE      (1u<<RANGE_BITS) /* automagically, on each recalculation there are just RANGE symbols counted on livecnt[] */
#define RANGE_BITS 15               /* 14 improves executables compression */

struct TCounter
{
  UINT n, remainder;
  UINT cnt[NUM], cum[NUM], livecnt[NUM], index[INDEXES];

  TCounter (unsigned _n = NUM)
  {
      n = _n;
      // Initially, allot RANGE points equally to n symbols
      // (superfluous RANGE-RANGE/n*n points are assigned to first symbols)
      for (int s = 0; s < n; s++)
          livecnt[s] = RANGE/n + (s < RANGE-RANGE/n*n? 1 : 0);
      Rescale();
  }

  // Count one more occurence of symbol s
  // and recalculate encoding tables if enough symbols was counted since last recalculation
  // so that sum(livecnt[])==RANGE now
  void Inc (unsigned s)
  {
      livecnt[s]++;
      if (--remainder==0)  Rescale();
  }

  // Recalculate (de)coding tables according to last gathered stats
  // and prepare to collect stats on next block
  void Rescale()
  {
      UINT total = 0;
      remainder = RANGE;
      for (int s=0, ind=0; s < n; s++) {
          cnt[s]      = livecnt[s],
          cum[s]      = total,
          total      += cnt[s],
          livecnt[s] -= (livecnt[s]==2 || livecnt[s]==3)? 1 : livecnt[s]/4;
          remainder  -= livecnt[s];
          // While last element of this symbol range may be mapped to index[ind]
          // fill index[ind] with this symbol. Finally, each index[n]
          // will contain *first* possible symbol whose first N bits equal to n
          // (N = logb(INDEXES)), which is used for quick symbol decoding
          while (cum[s]+cnt[s]-1 >= RANGE/INDEXES*ind)
              index[ind++] = s;
      }
      debug (printf("total %d\n",total));
  }

  // Find symbol corresponding to code 'count'
  UINT Decode (UINT count)
  {
      // index[] gives us a quick hint and then we search for a first symbol
      // whose end of range is greater than or equal to 'count'
      UINT s = index [count/(RANGE/INDEXES)];
      while (cum[s]+cnt[s]-1 < count)  s++;
      debug (printf("symbol %x, count %x of %x-%x\n", s, count, cum[s], cum[s]+cnt[s]-1));
      return s;
  }
};


struct ArithCoder : TEncoder
{
    TCounter c;   // Provides stats about symbol frequencies

    ArithCoder (void *_buf, int _size, int elements)
        : TEncoder (_buf,_size), c(elements) {}

    // Encode symbol x using TCounter stats
    void encode (UINT x)
    {
        debug (printf("symbol %x, count %x-%x\n",x,c.cum[x], c.cum[x]+c.cnt[x]-1));
        Encode (c.cum[x], c.cnt[x], RANGE_BITS);
        c.Inc (x);
    }

    // Write n lower bits of x
    void putlowerbits (int n, UINT x)
    {
        debug (printf("putbits %d of %x\n",n,x));
        if (n>15) {
            Encode (mask(x,15), 1, 15);
            debug (printf("encoded %x, %x ==> %x\n",mask(x,15),mask(x>>15,n-15),x));
            x>>=15, n-=15;
        }
        Encode (mask(x,n), 1, n);
    }
};


struct ArithDecoder : TDecoder
{
    TCounter c;   // Provides stats about symbol frequencies

    ArithDecoder (void *_buf, int _size, int elements)
        : TDecoder (_buf,_size), c(elements) {}

    // Decode next symbol using TCounter stats
    UINT decode()
    {
        UINT count = GetCount (RANGE_BITS);
        UINT x = c.Decode (count);
        Update (c.cum[x], c.cnt[x]);
        c.Inc (x);
        return x;
    }

    // Read next n bits of input
    UINT getbits (int n)
    {
        debug (printf("getbits %d\n",n));
        if (n>15) {
            UINT x1 = GetCount (15);
            Update (x1, 1);
            UINT x2 = GetCount (n-15);
            Update (x2, 1);
            debug (printf("decoded %x, %x ==> %x\n",x1,x2,(x2<<15) + x1));
            return (x2<<15) + x1;
        } else {
            UINT x = GetCount (n);
            Update (x, 1);
            return x;
        }
    }
};


