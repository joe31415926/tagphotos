#include <stdio.h>
       #include <fcntl.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
       #include <unistd.h>
       #include <sys/stat.h>
       #include <stdlib.h>

void main()
{
    int fd = open("/home/joeruff/m/match.bin", O_RDONLY);
    assert(fd > 0);
    struct stat statbuf;
    assert(fstat(fd, &statbuf) == 0);
    
    struct {
        char filename[32];
        int32_t param[3][16];
    } *dat = NULL;

    assert(statbuf.st_size % sizeof(dat[0]) == 0);
    long num = statbuf.st_size / sizeof(dat[0]);
    
    dat = malloc(statbuf.st_size);
    assert(dat);

    char *writehere = (char *) dat;
    ssize_t toread = statbuf.st_size;
    while (toread)
    {
        ssize_t thistime = read(fd, writehere, toread);
        assert(thistime > 0);
        toread -= thistime;
        writehere += thistime;
    }
    close(fd);
    
    long i;
    for (i = 0; i < num; i++)
    {
        char path[33] = {0};
        strncpy(path, dat[i].filename, 32);
        
        
        
        int zero = 0;
        int j;
        for (j = 0; j < 3; j++)
        {
            int k;
            for (k = 1; k < 16; k++)
                if (dat[i].param[j][k] == 0)
                    zero = 1;
        }
        assert(!zero);
        
//        printf("%s", path);
        for (j = 0; j < 3; j++)
        {
            printf("%s %d\n", path, dat[i].param[j][0]);
            int k;
            for (k = 1; k < 16; k++)
            {
                assert(dat[i].param[j][k] > -65536 && dat[i].param[j][k] < 65536);
//                        printf(" %d", dat[i].param[j][k]);
            }
        }
//        printf("\n");
    }
}