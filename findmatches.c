#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>

double lut[6][3] = {
    { 5.00, 19.21, 34.37, },
    { 0.83,  1.26,  0.36, },
    { 1.01,  0.44,  0.45, },
    { 0.52,  0.53,  0.14, },
    { 0.47,  0.28,  0.18, },
    { 0.30,  0.14,  0.27, },
};

double adjustscore(int color, int32_t *one, int32_t *two)
{
    double to_return = 0.0;
    
    int i;
    for (i = 0; i < 15; i++)
    {
        int32_t to_match = one[i];
        
        int j;
        for (j = 0; j < 15; j++)
            if (to_match == two[j])
                break;
        if (j < 15)
        {
            assert(to_match == two[j]);
            assert(to_match != 0);
            
            if (to_match < 0)
                to_match = -to_match;
            
            int32_t x = to_match & 0xFF;
            int32_t y = (to_match >> 8) & 0xFF;
            if (y > x)
                x = y;
            if (x > 5)
                x = 5;
            
            to_return -= lut[x][color];
        }
    }
    return to_return;
}

int32_t *n;

void *worker(void *arg)
{
    long *starting_num_ptr = (long *) arg;
    long start_num = *starting_num_ptr;
    *starting_num_ptr = 0;
    
    char filename[100];
    sprintf(filename, "matches%02ld.txt", start_num / 26713);
    FILE *fil = fopen(filename, "w");
    assert(fil);
    
    long i, j;
    for (i = start_num; i < start_num + 26713; i++)
    {
        int32_t *Y1 = n + 16 * (i * 3 + 0);
        int32_t *I1 = n + 16 * (i * 3 + 1);
        int32_t *Q1 = n + 16 * (i * 3 + 2);
        

        for (j = i + 1; j < 961668; j++)
        {
            int32_t *Y2 = n + 16 * (j * 3 + 0);
            int32_t *I2 = n + 16 * (j * 3 + 1);
            int32_t *Q2 = n + 16 * (j * 3 + 2);
            
            double score = 0.0;
            score =+ lut[0][0] * fabs(Y1[0] - Y2[0]) / 255.0;
            score =+ lut[0][1] * fabs(I1[0] - I2[0]) / 255.0;
            score =+ lut[0][2] * fabs(Q1[0] - Q2[0]) / 255.0;
            
            score += adjustscore(0, Y1 + 1, Y2 + 1);
            score += adjustscore(1, I1 + 1, I2 + 1);
            score += adjustscore(2, Q1 + 1, Q2 + 1);
            
            if (score < -12.0)
            {
                fprintf(fil, "%ld %ld %g\n", i, j, score);
                fflush(fil);
            }
        }
        (*starting_num_ptr)++;
    }
    fclose(fil);
}

int main()
{
    int fd = open("/home/joeruff/server/data.bin", O_RDONLY);
    assert(fd > 0);
    
    n = calloc(961668 * 16 * 3, sizeof(int32_t));
    assert(n != NULL);
    
    char *s = (char *) n;
    ssize_t toread = 961668 * 16 * 3 * sizeof(int32_t);
    while (toread)
    {
        ssize_t thistime = read(fd, s, toread);
        assert(thistime > 0);
        toread -= thistime;
        s += thistime;
    }
    close(fd);

    
    long i;
    
    long starting_num[36];
    for (i = 0; i < 36; i++)
        starting_num[i] = i * 26713;

    pthread_t thread[36];
    for (i = 0; i < 36; i++)
        assert(pthread_create(thread + i, NULL, worker, (void *) (starting_num + i)) == 0);
    
    long total = 0;
    while (total != 961668)
    {
        total = 0;
        for (i = 0; i < 36; i++)
            total += starting_num[i];
        printf("%ld %6ld/%d %02ld%%\n", (long) time(NULL), total, 961668, (100 * total) / 961668);
        usleep(1000000);
    }
    
    for (i = 0; i < 36; i++)
        assert(pthread_join(thread[i], NULL) == 0);
}
