#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd = open("/home/joeruff/server/nohup.out", O_RDONLY);
    assert(fd > 0);
    
    char *b = calloc(200000000, 1);
    assert(b != NULL);
    
    char *s = b;
    ssize_t thistime = 1;
    while (thistime)
    {
        thistime = read(fd, s, 200000000 - (s - b));
        assert(thistime >= 0);
        s += thistime;
    }
    close(fd);
    assert(s - b == 197763627);
    
    
    int32_t *n = calloc(961668 * 16 * 3, sizeof(int32_t));
    assert(n != NULL);
    
    int32_t *sn = n;
    s = b;
    long smallest = 0;
    long largest = 0;
    
    while (*s)
    {
        long ll = atol(s);
        if (ll < smallest) smallest = ll;
        if (ll > largest) largest = ll;
        *sn++ = ll;
        while ((*s >= '0' && *s <= '9') || *s == '-' || *s == '+')
            s++;
        while (*s == ' ' || *s == '\n')
            s++;
    }
    
    printf("%ld .. %ld\n", smallest, largest);

    free(b);

    fd = open("/home/joeruff/server/data.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd > 0);
    
    s = (char *) n;
    ssize_t towrite = 961668 * 16 * 3 * sizeof(int32_t);
    while (towrite)
    {
        thistime = write(fd, s, towrite);
        assert(thistime > 0);
        towrite -= thistime;
        s += thistime;
    }
    close(fd);
    
    free(n);
}