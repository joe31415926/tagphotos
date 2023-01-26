#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>

#define NUM_BATCH (36L)
#define FILES_PER_BATCH (26713L)
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

void vectorfile(double *Aarr, int *idx, char *beginning_of_file)
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
        
        printf("%d ", (int) Aarr[0]);
        
        int num = 15;
        for (i = 0; i < num; i++)
            if (idx[i])
            {
                int sign = Aarr[idx[i]] < 0.0 ? -1 : 1;
                printf("%d ", sign * idx[i]);
            }
            else
                num++;
    }
    printf("\n");
}

int main()
{
    double *Aarr = (double *) malloc(256 * 256 * sizeof(double));
    assert(Aarr);
    
    int *idx = (int *) malloc(256 * 256 * sizeof(int));
    assert(idx);
    
    int fd = open("/home/joeruff/m/matchall", O_RDONLY);
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
            vectorfile(Aarr, idx, buf + file * BYTES_PER_FILE);
    }
}