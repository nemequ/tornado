// LZ77 model *************************************************************************************
// Already tried:
//     no lz if len small and dist large: don't have much sense with our MINLEN=4
//     hash4+3: only 1% gain even on ghc.exe
//     hash5+4: 48->46.7 mb but 2x slower (22->46sec: 240mb compressed using 16mb hash)
// To do:
//     0x65a8e9b4 for hash
//     combined len+dist encoding a-la cabarc - will make decoding a bit faster, but who cares? :)
//     save into hash records unused part of hash value in order to make
//         fast check of usability of this hash slot (like it is already
//         done in REP); would be especially helpful on larger hashes
//     save into hash record 4 bits of p[5] - would be useful to skip trying second..fourth hash records
//     save into hash record 4 bytes of data
//     lazy search (and return of 3-byte strings) for highest compression mode
//     ST4/BWT sorting for exhaustive string searching

#ifdef DEBUG
void check_match (BYTE *p, BYTE *q, int len)
{
    if (memcmp(p,q,len))   printf("Bad match:  ");
}
void print_literal (int pos, BYTE c)
{
    printf (isprint(c)? "%06x: '%c'\n" : "%06x: \\0x%02x\n", pos, c);
}
void print_match (int pos, int len, int dist)
{
    printf ("%06x: %3d %6d\n", pos, len, -dist);
}
#else
#define check_match(p,q,len)         (0)
#define print_literal(pos,c)         (0)
#define print_match(pos,len,dist)    (0)
#endif


// Settings for 4-byte hashing
#define value(p)     value32(p)
#define MINLEN       4

struct BaseMatchFinder
{
    UINT HashSize, HashShift, HashMask;
    BYTE **HTable, *q;

    BaseMatchFinder (BYTE *buf, int hashlog, int hash_row_width)
    {
        HashSize  = 1<<hashlog;
        HashShift = 32-hashlog;
        HashMask  = (HashSize-1) & ~(roundup_to_power_of(hash_row_width,2)-1);
        HTable    = (BYTE**) malloc (sizeof(BYTE*) * HashSize);
        for (int i=0; i<HashSize; i++)  HTable[i]=buf;
    }
    byte *get_matchptr (void)
    {
        return q;
    }
    // hash function
    uint hash (uint x)
    {
        return ((x*153191) >> HashShift) & HashMask;
    }
};
int alloced = 0;


struct MatchFinder1 : BaseMatchFinder
{
    MatchFinder1 (BYTE *buf, int hashlog, int hash_row_width)
        : BaseMatchFinder (buf, hashlog, hash_row_width) {alloced++;}
    ~MatchFinder1 () { alloced--; free(HTable); }

    uint find_matchlen (byte *p, void *bufend, int hash_row_width)
    {
        UINT HashMask=(UINT)-1;
        UINT h = hash(value(p));
        q = HTable[h];  HTable[h] = p;
        if (value(p) == value(q)) {
            uint len;
            for (len=MINLEN; p+len<bufend && value32(p+len)==value32(q+len); len+=4);
            for (; p+len<bufend && p[len]==q[len]; len++);
            return len;
        } else {
            return 0;
        }
    }
    void update_hash (byte *p, uint len, int hash_row_width)
    {
/*        uint h;
        h = hash(value(p+1)),  HTable[h] = p+1;
        h = hash(value(p+2)),  HTable[h] = p+2;
        p += len;
        h = hash(value(p-1)),  HTable[h] = p-1;
*/    }
};

struct MatchFinder2 : BaseMatchFinder
{
    MatchFinder2 (BYTE *buf, int hashlog, int hash_row_width)
        : BaseMatchFinder (buf, hashlog, hash_row_width) {alloced++;}
    ~MatchFinder2 () { alloced--; free(HTable); }

    uint find_matchlen (byte *p, void *bufend, int hash_row_width)
    {
        uint len;
        UINT h = hash(value(p));
        q = HTable[h]; BYTE *q1 = HTable[h+1];
        HTable[h+1] = q, HTable[h] = p;
        if (value(p) == value(q)) {
            for (len=MINLEN; p+len<bufend && value32(p+len)==value32(q+len); len+=4);
            for (; p+len<bufend && p[len]==q[len]; len++);

            if (p[len] == q1[len]) {
                UINT len1;
                for (len1=0; p+len1<bufend && p[len1]==q1[len1]; len1++);
                if (len1>len)  len=len1, q=q1;
            }
            return len;
        } else if (value(p) == value(q1)) {
            q=q1;
            for (len=MINLEN; p+len<bufend && value32(p+len)==value32(q+len); len+=4);
            for (; p+len<bufend && p[len]==q[len]; len++);
            return len;
        } else {
            return MINLEN-1;
        }
    }
    void update_hash (byte *p, uint len, int hash_row_width)
    {
        uint h;
        h = hash(value(p+1)),  HTable[h+1] = HTable[h],  HTable[h] = p+1;
        h = hash(value(p+2)),  HTable[h+1] = HTable[h],  HTable[h] = p+2;
        p += len;
        h = hash(value(p-1)),  HTable[h+1] = HTable[h],  HTable[h] = p-1;
    }
};


struct MatchFinderN : BaseMatchFinder
{
    MatchFinderN (BYTE *buf, int hashlog, int hash_row_width)
        : BaseMatchFinder (buf, hashlog, hash_row_width) {alloced++;}
    ~MatchFinderN () { alloced--; free(HTable); }

    uint find_matchlen (byte *p, void *bufend, int hash_row_width)
    {
        UINT h = hash(value(p));
        uint len = MINLEN-1;
        BYTE *q1, *q0 = q = HTable[h];  HTable[h] = p;
        // Start with checking first element of hash row
        if (value(p) == value(q)) {
            for (len=MINLEN;  p+len<bufend && value32(p+len)==value32(q+len); len+=4);
            for (; p+len<bufend && p[len]==q[len]; len++);
        }
        // Check remaining elements, searching for longer match,
        // and shift entire row toward end
        for (int j=1; j<hash_row_width; j++, q0=q1) {
            q1=HTable[h+j];  HTable[h+j]=q0;
            if (value(p+len+1-MINLEN) == value(q1+len+1-MINLEN)) {
                UINT len1;
                for (len1=0; p+len1<bufend && p[len1]==q1[len1]; len1++);
                if (len1>len)  len=len1, q=q1;
            }
        }
        return len;
    }
    void update_hash (byte *p, uint len, int hash_row_width)
    {
        uint h;
        h = hash(value(p+1));  for (int j=hash_row_width; --j; HTable[h+j] = HTable[h+j-1]);  HTable[h] = p+1;
        h = hash(value(p+2));  for (int j=hash_row_width; --j; HTable[h+j] = HTable[h+j-1]);  HTable[h] = p+2;
        p += len;
        h = hash(value(p-1));  for (int j=hash_row_width; --j; HTable[h+j] = HTable[h+j-1]);  HTable[h] = p-1;
    }
};


// This matchfinder caches 4 bytes of string (p[3..6]) in hash itself,
// providing faster search in case of high-populated hash rows.
// I suggest to use it only for hash_row_width>=4, buffer>=8mb and hashsize>=2mb.
// This class works only with MINLEN=4 and sizeof(void*)=4
struct CachingMatchFinderN : BaseMatchFinder
{
    CachingMatchFinderN (BYTE *buf, int hashlog, int hash_row_width)
        : BaseMatchFinder (buf, hashlog, hash_row_width*2) {alloced++;}
    ~CachingMatchFinderN () { alloced--; free(HTable); }

    // Key value stored in hash for each string
    uint key (BYTE *p)
    {
        return value32(p+3);
    }
    uint find_matchlen (byte *p, void *bufend, int hash_row_width)
    {
        // Length of largest string already found
        UINT len = MINLEN-1;
        UINT h = hash(value(p));
        // Pointers to the current hash element and end of hash row
        BYTE **table = HTable+h, **tabend = table + hash_row_width*2;
        // q0/v0 - first potentially matched string and its key.
        // q1/v1 used for subsequent matches (and q0/v0 turn into previous match attributes)
        BYTE *q1, *q0 = q = *table;  *table++ = p;
        UINT v1, v0 = (UINT)*table;  *table++ = (BYTE*) key(p);

        // Start with checking first element of hash row
        uint t = v0 ^ value32(p+3);
        if ((t&0xff) == 0  &&  value(p) == value(q)) {
            if (t==0) {
                for (len=7;  p+len<bufend && value32(p+len)==value32(q+len); len+=4);
                for (; p+len<bufend && p[len]==q[len]; len++);
                goto len7;
            } else if (t&0xff00) {
                len=4; goto len4;
            } else if (t&0xff0000) {
                len=5; goto len5;
            } else {
                len=6; goto len6;
            }
        }

        // Check remaining elements, searching for longer match,
        // and shift entire row contents towards end
        // (there five loops - one used before any string is found,
        //  three used when a string of size 4/5/6 already found,
        //  and one used when string of size 7+ already found)
len3:
        for (; table < tabend; q0=q1, v0=v1) {
            // Read next ptr/key from hash and save here previous pair (shifted from previous position)
            q1 = *table;         *table++ = q0;
            v1 = (UINT) *table;  *table++ = (BYTE*)v0;
            // t contains 0 in lower byte if p[3]==q1[3], in second byte if p[4]==q1[4] and so on
            uint t = v1 ^ value32(p+3);
            // Fast check using key that p[3]==q1[3] before actual memory access
            if ((t&0xff) == 0  &&  value(p) == value(q1)) {
                // At this moment we already know that at least MINLEN chars
                // are the same, checking the actual match length
                for (len=MINLEN; p+len<bufend && p[len]==q1[len]; len++);
                q=q1, q0=q1, v0=v1; goto len4;
            }
        }
        return len;

len4:   if (len>=5) goto len5;
        for (; table < tabend; q0=q1, v0=v1) {
            q1 = *table;         *table++ = q0;
            v1 = (UINT) *table;  *table++ = (BYTE*)v0;
            uint t = v1 ^ value32(p+3);
            if ((t&0xffff) == 0  &&  value(p) == value(q1)) {
                UINT len1;
                for (len1=len; p+len1<bufend && p[len1]==q1[len1]; len1++);
                len=len1, q=q1, q0=q1, v0=v1;  goto len5;
            }
        }
        return len;

len5:   if (len>=6) goto len6;
        for (; table < tabend; q0=q1, v0=v1) {
            q1 = *table;         *table++ = q0;
            v1 = (UINT) *table;  *table++ = (BYTE*)v0;
            uint t = v1 ^ value32(p+3);
            if ((t&0xffffff) == 0  &&  value(p) == value(q1)) {
                UINT len1;
                for (len1=len; p+len1<bufend && p[len1]==q1[len1]; len1++);
                len=len1, q=q1, q0=q1, v0=v1; goto len6;
            }
        }
        return len;

len6:   if (len>=7) goto len7;
        for (; table < tabend; q0=q1, v0=v1) {
            q1 = *table;         *table++ = q0;
            v1 = (UINT) *table;  *table++ = (BYTE*)v0;
            uint t = v1 ^ value32(p+3);
            if (t == 0  &&  value(p) == value(q1)) {
                UINT len1;
                for (len1=len; p+len1<bufend && p[len1]==q1[len1]; len1++);
                len=len1, q=q1, q0=q1, v0=v1; goto len7;
            }
        }
        return len;

len7:
        for (; table < tabend; q0=q1, v0=v1) {
            q1 = *table;         *table++ = q0;
            v1 = (UINT) *table;  *table++ = (BYTE*)v0;
            uint t = v1 ^ value32(p+3);
            if (t == 0  &&  p[len] == q1[len]) {
                UINT len1;
                for (len1=0; p+len1<bufend && p[len1]==q1[len1]; len1++);
                if (len1>len)  len=len1, q=q1;
            }
        }
        return len;
    }

    // Update one hash row corresponding to string pointed by p
    // (hash updated via this procedure only when skipping match contents)
    void update_hash1 (byte *p, int hash_row_width)
    {
        uint h;
        h = hash(value(p));
        for (int j=hash_row_width; --j; )
            HTable[h+j*2]   = HTable[h+(j-1)*2],
            HTable[h+j*2+1] = HTable[h+(j-1)*2+1];
        HTable[h] = p;
        HTable[h+1] = (BYTE*) key(p);
    }
    // Skip match starting from p with length len and particularly update hash
    // (only with string at the begin and end of match)
    void update_hash (byte *p, uint len, int hash_row_width)
    {
        update_hash1 (p+1, hash_row_width);
        update_hash1 (p+2, hash_row_width);
        p += len;
        update_hash1 (p-1, hash_row_width);
    }
};


