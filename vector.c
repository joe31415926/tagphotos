#include <stdio.h>
#include <assert.h>

#define NUM_BATCH (36L)
#define FILES_PER_BATCH (26713L)
#define BYTES_PER_FILE (196623L)

int main()
{
    int fd = open("matchall", O_RDONLY);
    assert(fd > 0);
    
    char *buf = malloc(FILES_PER_BATCH * BYTES_PER_FILE);
    assert(buf);
    
    int batch;
    for (batch = 0; batch < NUM_BATCH; batch++)
    {
        char *tohere = buf;
        ssize_t toread = FILES_PER_BATCH * BYTES_PER_FILE;
        while (toread)
        {
            ssize_t readthistime = toread;
            if (readthistime > 1024 * 1024 * 1024)
                readthistime = 1024 * 1024 * 1024;
            readthistime = read(fd, tohere, readthistime);
            assert(readthistime > 0);
            toread -= readthistime;
            tohere += readthistime;
        }
        
        long file;
        for (file = 0; file < FILES_PER_BATCH; file++)
        {
            char *beginning_of_file = buf + file * BYTES_PER_FILE;
            uint_8 *d = beginning_of_file + 15;
            long t1 = 0;
            long t2 = 0;
            long t3 = 0;
            long i;
            for (i = 0; i < 256 * 256; i++)
            {
                t1 += *d++;
                t2 += *d++;
                t3 += *d++;
            }
            printf("%ld %ld %ld\n", t1, t2, t3);
        }
    }
}