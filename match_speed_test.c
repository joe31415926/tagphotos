#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <math.h>

#define MOUNTED_DISK "/home/joeruff/m"

#define NUM_THREADS (36)

double lut[6][3] = {
    { 5.00, 19.21, 34.37, },
    { 0.83,  1.26,  0.36, },
    { 1.01,  0.44,  0.45, },
    { 0.52,  0.53,  0.14, },
    { 0.47,  0.28,  0.18, },
    { 0.30,  0.14,  0.27, },
};

struct {
    char filename[32];
    int32_t param[3][16];
} *dat = NULL;
    
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

void *worker1(void *arg)
{
    int32_t *buf = *((int32_t **) arg);
    int32_t thread_number = buf[0];
    long size = 1000;
    long num = 0;
    buf = realloc(buf, sizeof(buf[0]) * size);
    assert(buf);
    buf[0] = 0;
    
    printf("%d/%d %ld\n", thread_number, NUM_THREADS, 961668L);

    uint64_t files_per_thread = 961668 / NUM_THREADS;
    if (961668 % NUM_THREADS)
        files_per_thread++;
    
    long i;
    for (i = files_per_thread * thread_number; i < files_per_thread * thread_number + files_per_thread; i++)
    {        
        int32_t *Y1 = dat[i].param[0];
        int32_t *I1 = dat[i].param[1];
        int32_t *Q1 = dat[i].param[2];
        

        long j;
        for (j = i + 1; j < 961668; j++)
        {
            int32_t *Y2 = dat[j].param[0];
            int32_t *I2 = dat[j].param[1];
            int32_t *Q2 = dat[j].param[2];
            
            double score = 0.0;
            score =+ lut[0][0] * fabs(Y1[0] - Y2[0]) / 255.0;
            score =+ lut[0][1] * fabs(I1[0] - I2[0]) / 255.0;
            score =+ lut[0][2] * fabs(Q1[0] - Q2[0]) / 255.0;
            
            score += adjustscore(0, Y1 + 1, Y2 + 1);
            score += adjustscore(1, I1 + 1, I2 + 1);
            score += adjustscore(2, Q1 + 1, Q2 + 1);
            
            if (score < -12.0)
            {
                while (num + 1 > size)
                {
                    size *= 2;
                    buf = realloc(buf, sizeof(buf[0]) * size);
                    assert(buf);
                }
                buf[num++] = score;
                buf[num] = 0;
            }
        }
    }
}

void *worker2(void *arg)
{
    int32_t *buf = *((int32_t **) arg);
    int32_t thread_number = buf[0];
    long size = 1000;
    long num = 0;
    buf = realloc(buf, sizeof(buf[0]) * size);
    assert(buf);
    buf[0] = 0;
    
    printf("%d/%d %ld\n", thread_number, NUM_THREADS, 961668L);

    uint64_t files_per_thread = 961668 / NUM_THREADS;
    if (961668 % NUM_THREADS)
        files_per_thread++;
    
    long i;
    for (i = files_per_thread * thread_number; i < files_per_thread * thread_number + files_per_thread; i++)
    {        
        int32_t *Y1 = dat[i].param[0];
        int32_t *I1 = dat[i].param[1];
        int32_t *Q1 = dat[i].param[2];
        

        long j;
        for (j = i + 1; j < 961668; j++)
        {
            int32_t *Y2 = dat[j].param[0];
            int32_t *I2 = dat[j].param[1];
            int32_t *Q2 = dat[j].param[2];
            
            double score = 0.0;
            score =+ lut[0][0] * fabs(Y1[0] - Y2[0]) / 255.0;
            score =+ lut[0][1] * fabs(I1[0] - I2[0]) / 255.0;
            score =+ lut[0][2] * fabs(Q1[0] - Q2[0]) / 255.0;
            
            score += adjustscore(0, Y1 + 1, Y2 + 1);
            score += adjustscore(1, I1 + 1, I2 + 1);
            score += adjustscore(2, Q1 + 1, Q2 + 1);
            
            if (score < -12.0)
            {
                while (num + 1 > size)
                {
                    size *= 2;
                    buf = realloc(buf, sizeof(buf[0]) * size);
                    assert(buf);
                }
                buf[num++] = score;
                buf[num] = 0;
            }
        }
    }
}

#define BATCHSIZE (961668)


int main()
{
    printf("sizeof(double) %ld\n", sizeof(double));
    
    char fullpath[128];
    sprintf(fullpath, "%s/%s", MOUNTED_DISK, "match.bin");

    int fd = open(fullpath, O_RDONLY);
    assert(fd > 0);
    
    struct stat statbuf;
    assert(fstat(fd, &statbuf) == 0);
    
    assert(statbuf.st_size == sizeof(dat[0]) * 961668);
    dat = (void *) malloc(statbuf.st_size);
    assert(dat);
    
    char *writehere = (char *) dat;
    ssize_t toread = statbuf.st_size;
    while (toread)
    {
        ssize_t trythismuch = toread;
        if (trythismuch > 100000000)
            trythismuch = 100000000;
        ssize_t readthistime = read(fd, writehere, trythismuch);
        assert(readthistime > 0);
        toread -= readthistime;
        writehere += readthistime;
    }
    close(fd);
    
    int32_t *ll = malloc(sizeof(int32_t) * (131072L + BATCHSIZE * 15 * 2));
    assert(ll);
    
    int32_t *color_idxs[3][5];
    long i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 5; j++)
        {
            color_idxs[i][j] = malloc(sizeof(int32_t) * (131072L + BATCHSIZE * 15));
            assert(color_idxs[i][j]);
        }
    }
    
    for (j = 0; j < 3; j++)
    {
        memset(ll, 0, sizeof(int32_t) * 131072L);
        int32_t ll_size = 131072L;

        for (i = 0; i < BATCHSIZE; i++)
        {
            int k;
            for (k = 1; k < 16; k++)
            {
                int32_t idx = 65536L + dat[i].param[j][k];
                assert(idx >= 0 && idx < 131072L);
                
                ll[ll_size] = i + 1;
                ll[ll_size + 1] = ll[idx];
                ll[idx] = ll_size;
                ll_size += 2;
            }
        }
        assert(ll_size == 131072L + BATCHSIZE * 15 * 2);
        
        int32_t color_idxs_count[5] = {0};
        for (i = 0; i < 131072L; i++)
        {
            int32_t to_match = i - 65536L;
            if (to_match < 0)
                to_match = -to_match;
            
            int32_t x = to_match & 0xFF;
            int32_t y = (to_match >> 8) & 0xFF;
            if (y > x)
                x = y;
            if (x > 5)
                x = 5;
            
            if (x == 0)
                assert(ll[i] == 0);
            else
            {
                x--;
                int32_t scan_idx = ll[i];
                while (scan_idx)
                {
                    color_idxs[j][x][color_idxs_count[x]++] = ll[scan_idx];
                    assert(ll[scan_idx] > 0 && ll[scan_idx] < BATCHSIZE + 1);
                    scan_idx = ll[scan_idx + 1];
                }
                if (ll[i])
                    color_idxs[j][x][color_idxs_count[x]++] = 0;
            }
        }
        for (i = 0; i < 5; i++)
        {
            color_idxs[j][i][color_idxs_count[i]++] = 0;
            color_idxs[j][i] = realloc(color_idxs[j][i], color_idxs_count[i] * sizeof(int32_t));
            assert(color_idxs[j][i]);
            
            printf("%ld %ld %d\n", j, i, color_idxs_count[i]);
            assert(color_idxs_count[i] <= 131072L + BATCHSIZE * 15);
        }
    }
    
    free(ll);
    ll = NULL;
    
    int32_t *buffers[NUM_THREADS];
    pthread_t thread[NUM_THREADS];
    
    printf("%ld about to start\n", (long) time(NULL));
    for (i = 0; i < NUM_THREADS; i++)
    {
        buffers[i] = malloc(sizeof(buffers[0][0]));
        assert(buffers[i]);
        buffers[i][0] = i;
        assert(pthread_create(thread + i, NULL, worker1, (void *) (buffers + i)) == 0);
    }
    
    for (i = 0; i < NUM_THREADS; i++)
        assert(pthread_join(thread[i], NULL) == 0);
    printf("%ld done\n", (long) time(NULL));
    
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 5; j++)
        {
            free(color_idxs[i][j]);
            color_idxs[i][j] = NULL;
        }
    }

    free(dat);
    dat = NULL;    
}