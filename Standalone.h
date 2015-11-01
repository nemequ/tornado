// This definitions used for compilation of standalone executables
#ifndef STANDALONE_COMPRESSION_H
#define STANDALONE_COMPRESSION_H

#define file_exists(name)                       (access((name),0) == 0)
#define delete_file(name)                       remove(name)
#define rename_file(oldname,newname)            rename(oldname,newname)
#define create_dir(name)                        mkdir(name)
#define delete_dir(name)                        rmdir(name)
#define set_flen(stream,new_size)               chsize( file_no(stream), new_size )
#define get_flen(stream)                        filelength( file_no(stream) )
#define eof_arcfile()                           ( filelength(fileno(arcfile)) == ftell(arcfile) )
#define get_ftime(stream,tstamp)                getftime( file_no(stream), (struct ftime *) &tstamp )
#define set_ftime(stream,tstamp)                setftime( file_no(stream), (struct ftime *) &tstamp )
#define read(file, buf, size)                   fread  (buf, 1, size, file)
#define write(file, buf, size)                  fwrite (buf, 1, size, file)

// Вычисление времени работы алгоритма
#include <windows.h>
static LARGE_INTEGER Frequency, PerformanceCountStart, PerformanceCountEnd;
static void init_timer (void)
{
    QueryPerformanceFrequency (&Frequency);
    QueryPerformanceCounter (&PerformanceCountStart);
}

static double timer (void)
{
    QueryPerformanceCounter (&PerformanceCountEnd);
    return double(PerformanceCountEnd.QuadPart - PerformanceCountStart.QuadPart)/Frequency.QuadPart;
}
#endif
