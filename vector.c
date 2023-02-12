#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>
#include <dirent.h>
#include <pthread.h>

#define PPM_DIRECTORY ("/home/joeruff/m/match/")
#define OUTPUT_FILE_B ("/home/joeruff/m/match.bin")

#define NUM_THREADS (25)
long numfiles = 0;
long files_per_thread = 0;

struct {
    char filename[32];
    int32_t param[3][16];
} *dat = NULL;

#define BYTES_PER_FILE (196623L)

void DecomposeArray(double *A)
{
    long h = 256;
    while (h > 1)
    {
        h >>= 1;
        
        double Ap[256];
        long i;
        for (i = 0; i < h; i++)
        {
            Ap[i]=(A[2*i]+A[2*i+1])/M_SQRT2;
            Ap[h+i]=(A[2*i]-A[2*i+1])/M_SQRT2;
        }
        memcpy(A, Ap, sizeof(Ap));
    }
}

int compar(const void *ap, const void *bp, void *context)
{
    int *a = (int *) ap;
    int *b = (int *) bp;
    double *Aarr = (double *) context;
    
    if (fabs(Aarr[*a]) > fabs(Aarr[*b]))
        return -1;
    if (fabs(Aarr[*a]) < fabs(Aarr[*b]))
        return 1;
    return 0;
}

void vectorfile(double *Aarr, int *idx, char *beginning_of_file, int32_t param[3][16])
{
    assert(strncmp(beginning_of_file, "P6\n256 256\n255\n", 15) == 0);
    
    uint8_t *d = beginning_of_file + 15;
    
    long i;
    for (i = 0; i < 256L * 256L; i++)
    {
        uint8_t red = d[3L * i + 0];
        uint8_t green = d[3L * i + 1];
        uint8_t blue = d[3L * i + 2];
        
        double Y = 0.299*red + 0.587*green + 0.114*blue;
        double I = 0.596*red - 0.275*green - 0.321*blue;
        double Q = 0.212*red - 0.523*green + 0.311*blue;
        
        if (Y < 0.5)
            d[3L * i + 0] = 0;
        else if (Y > 254.5)
            d[3L * i + 0] = 255;
        else
            d[3L * i + 0] = Y;
            
        if (I < 0.5)
            d[3L * i + 1] = 0;
        else if (I > 254.5)
            d[3L * i + 1] = 255;
        else
            d[3L * i + 1] = I;
                
        if (Q < 0.5)
            d[3L * i + 2] = 0;
        else if (Q > 254.5)
            d[3L * i + 2] = 255;
        else
            d[3L * i + 2] = Q;
    }


    long color;
    for (color = 0; color < 3; color++)
    {
        for (i = 0; i < 256L * 256L; i++)
            Aarr[i] = ((double) d[3L * i + color]) / 16.0 / 16.0;
        
        for (i = 0; i < 256; i++)
            DecomposeArray(Aarr + 256 * i);
            
        // transpose
        for (i = 0; i < 256; i++)
        {
            long j;
            for (j = i + 1; j < 256; j++)
            {
                double t = Aarr[i * 256 + j];
                Aarr[i * 256 + j] = Aarr[j * 256 + i];
                Aarr[j * 256 + i] = t;
            }
        }

        for (i = 0; i < 256; i++)
            DecomposeArray(Aarr + 256 * i);
        
        for (i = 0; i < 256L * 256L; i++)
            idx[i] = i;
            

        qsort_r(idx, 256L * 256L, sizeof(idx[0]), compar, Aarr);
        
        
        uint32_t *p = param[color];
        *p++ = (int) Aarr[0];
        
        int num = 15;
        for (i = 0; i < num; i++)
            if (idx[i])
            {
                int sign = Aarr[idx[i]] < 0.0 ? -1 : 1;
                *p++ = sign * idx[i];
            }
            else
                num++;
    }
}

void *worker(void *arg)
{
    double *Aarr = (double *) malloc(256 * 256 * sizeof(double));
    assert(Aarr);
    
    int *idx = (int *) malloc(256 * 256 * sizeof(int));
    assert(idx);
    
    char *buf = malloc(BYTES_PER_FILE);
    assert(buf);

    long start_idx = (long) arg;
    long i;
    for (i = 0; i < files_per_thread; i++)
    {
        char filename[100] = {0};
        strcpy(filename, PPM_DIRECTORY);
        strncpy(filename + strlen(filename), dat[start_idx + i].filename, 32);
        strcat(filename, ".ppm");

        int fd = open(filename, O_RDONLY);
        assert(fd > 0);

        char *tohere = buf;
        ssize_t toread = BYTES_PER_FILE;
        while (toread)
        {
            ssize_t readthistime = read(fd, tohere, toread);
            assert(readthistime > 0);
            toread -= readthistime;
            tohere += readthistime;
        }
        close(fd);
        
        vectorfile(Aarr, idx, buf, dat[start_idx + i].param);
        
        if (i % 1000 == 0)
            printf("%ld %ld/%ld\n", start_idx, i, files_per_thread);
    }
}

int main()
{
    printf("counting the number of files...\n");

    int dirfd = open(PPM_DIRECTORY, O_RDONLY);
    assert(dirfd > 0);
    
    DIR *dd = fdopendir(dirfd);
    assert(dd);
    
    numfiles = 0;
    struct dirent *de;
    while ((de = readdir(dd)) != NULL)
        if (de->d_name[0] != '.')
        {
            if (de->d_type == DT_REG)
            {
                assert(strlen(de->d_name) == 36 && strcmp(de->d_name + 32, ".ppm") == 0);
                numfiles++;
            }
        }
    assert(closedir(dd) == 0);
    printf("numfiles: %ld\nreading the filenames....\n", numfiles);
    
    dat = malloc(sizeof(dat[0]) * numfiles);
    assert(dat);
        
    dirfd = open(PPM_DIRECTORY, O_RDONLY);
    assert(dirfd > 0);
    
    dd = fdopendir(dirfd);
    assert(dd);

    long i = 0;
    while ((de = readdir(dd)) != NULL)
        if (de->d_name[0] != '.')
        {
            if (de->d_type == DT_REG)
            {
                assert(strlen(de->d_name) == 36 && strcmp(de->d_name + 32, ".ppm") == 0);
                strncpy(dat[i++].filename, de->d_name, 32);
            }
        }
    assert(closedir(dd) == 0);
    printf("filenames read\n");

    files_per_thread = numfiles / NUM_THREADS;
    if (numfiles % NUM_THREADS)
        files_per_thread++;
    
    pthread_t thread[NUM_THREADS];
    for (i = 0; i < NUM_THREADS; i++)
        assert(pthread_create(thread + i, NULL, worker, (void *) i) == 0);
        
    for (i = 0; i < NUM_THREADS; i++)
        assert(pthread_join(thread[i], NULL) == 0);
    
    int fd = open(OUTPUT_FILE_B, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd > 0);
    
    char *s = (char *) dat;
    ssize_t towrite = sizeof(dat[0]) * numfiles;
    while (towrite)
    {
        ssize_t thistime = write(fd, s, towrite);
        assert(thistime > 0);
        towrite -= thistime;
        s += thistime;
    }
    close(fd);
}