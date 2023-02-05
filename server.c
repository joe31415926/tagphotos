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
#include <openssl/md5.h>

// gcc -o server server.c -lcrypto

#define CACHE_DIR "/home/joeruff/server/cache"
#define MOUNTED_DISK "/home/joeruff/m"
typedef enum {
    FILES = 0,
    MD5S,
    NUM_CACHES,
} buffer_type;
const char cachefile[NUM_CACHES][8] = {"FILES", "MD5S"};


char logfile_path[100];

void write_a_log_line(const char *mess)
{
    FILE *logfile = fopen(logfile_path, "a+");
    assert(logfile);
    fprintf(logfile, "%ld %s\n", time(NULL), mess);
    fclose(logfile);
}

void my_assert(int cond, const char *mess)
{
    if (!cond)
    {
        char logline[100];
        sprintf(logline, "ASSERT FAIL: %s", mess);
        write_a_log_line(logline);
        exit(-1);
    }
}

#define MAXIMUM_NUMBER_OF_CLIENT_LISTENERS (5)
char *inbuffers[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {NULL};
char *outbuffers[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {NULL};
int bytes_to_send[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
int bytes_received[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
int inbuffer_size[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
int outbuffer_size[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
struct pollfd fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 1];

void getspace(int i, ssize_t size)
{
    if (outbuffers[i] == NULL || bytes_to_send[i] + size > outbuffer_size[i])
    {        
        outbuffer_size[i] = bytes_to_send[i] + size;
        outbuffers[i] = realloc(outbuffers[i], outbuffer_size[i]);
        my_assert(outbuffers[i] != NULL, "realloc outbuffers");
    }
}

void incorporate_buffer(int i, ssize_t size)
{
    my_assert(bytes_to_send[i] + size <= outbuffer_size[i], "not enough space to incorporate");
    bytes_to_send[i] += size;
}

void sendout(int i, const char *message)
{
    int l = strlen(message);
    getspace(i, l);
    memcpy(outbuffers[i] + bytes_to_send[i], message, l);
    incorporate_buffer(i, l);
}

/*
char *indexfiles = NULL;
long lenindexfiles = 0;

struct {
    long lloffset;
    long offset;
} *ll = NULL;
long numll = 0;
long sizell = 0;

struct {
    long matcha;
    long matchb;
    double matchm;
} *matchll = NULL;
long nummatchll = 0;
long sizematchll = 0;

struct {
    char d_name[37];
    long lloffset;
    long matchlloffset;
} *files = NULL;
long numfiles = 0;
long sizefiles = 0;
*/

void cached_database_calcsignature_FILE(MD5_CTX *);
void cached_database_recalculate_FILE(void);
void cached_database_calcsignature_MD5S(MD5_CTX *);
void cached_database_recalculate_MD5S(void);

// #pragma pack(push, 1)

typedef struct {
    uint64_t l;
    uint64_t h;
} compacted_filename_t;

typedef struct {
    unsigned char md5[MD5_DIGEST_LENGTH];
    union {
            compacted_filename_t files[0];
            uint32_t md5s[0];
    };
} cached_database_t;

// #pragma pack(pop)
     

struct {
    buffer_type type;
    uint64_t size; // size for of (unit_size) items follow the (cached_database_t) header in the cached database
    uint64_t length; // number of (unit_size) items follow the (cached_database_t) header in the cached database
    uint32_t unit_size;
    cached_database_t *db;
    void (*cached_database_calcsignature)(MD5_CTX *);
    void (*cached_database_recalculate)(void);
} ds[NUM_CACHES] = {
    {FILES, 0, 0, sizeof(ds[FILES].db->files[0]), NULL, cached_database_calcsignature_FILE, cached_database_recalculate_FILE},
    {MD5S,  0, 0, sizeof(ds[MD5S].db->md5s[0]),   NULL, cached_database_calcsignature_MD5S, cached_database_recalculate_MD5S},
};


// see RFC 3986 for the list of characters which can be used in a URL
const char encode_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-._~";

void long_to_char(unsigned long a, char *b)
{
    char *remember = b;
    while (a)
    {
        *b++ = encode_chars[a % strlen(encode_chars)];
        a /= strlen(encode_chars);
    }
    *b = '\0';
    // reverse
    while (b - remember > 1)
    {
        char save = *remember;
        *remember = b[-1];
        b[-1] = save;
        remember++;
        b--;
    }
}

unsigned long char_to_long(char *b)
{
    unsigned long to_return = 0;
    char *c = strchr(encode_chars, *b++);
    while (c)
    {
        to_return = to_return * strlen(encode_chars) + c - encode_chars;
        c = NULL;
        if (*b)
            c = strchr(encode_chars, *b++);
    }    
    return to_return;
}


void cached_database_load(int bt)
{
    ds[bt].db = NULL;
    char fullpath[128];
    sprintf(fullpath, "%s/%s", CACHE_DIR, cachefile[bt]);
    int fd = open(fullpath, O_RDONLY);
    struct stat statbuf;
    if (fd > 0)
    {
        if ((fstat(fd, &statbuf) == 0) && (statbuf.st_size >= sizeof(cached_database_t)) && ((ds[bt].db = malloc(statbuf.st_size)) != NULL))
        {
            my_assert(ds[bt].unit_size > 0, "unit_size > 0");
            my_assert(((statbuf.st_size > sizeof(cached_database_t)) && ((statbuf.st_size - sizeof(cached_database_t)) % ds[bt].unit_size == 0)), "cached database size");
            ds[bt].size = (statbuf.st_size - sizeof(cached_database_t)) / ds[bt].unit_size;
            ds[bt].length = ds[bt].size;
            ssize_t toread = statbuf.st_size;
            while (toread)
            {
                ssize_t readthistime = read(fd, ((char *) ds[bt].db) + statbuf.st_size - toread, toread < 100000000 ? toread : 100000000);
                my_assert(readthistime > 0, "cache file read");
                toread -= readthistime;
            }
        }
        close(fd);
    }
    if (ds[bt].db == NULL)
    {
        ds[bt].db = calloc(1, sizeof(cached_database_t));
        my_assert(ds[bt].db != NULL, "malloc sizeof(cached_database_t)");
        ds[bt].size = 0;
        ds[bt].length = 0;
    }
}

void cached_database_write(int bt)
{
    my_assert(ds[bt].length <= ds[bt].size, "length vs size");
    my_assert(ds[bt].unit_size > 0, "unit_size > 0");
    if (ds[bt].length < ds[bt].size)
    {
        ds[bt].size = ds[bt].length;    
        ds[bt].db = realloc(ds[bt].db, ds[bt].size * ds[bt].unit_size + sizeof(cached_database_t));
        my_assert(ds[bt].db != NULL, "realloc cached_database_write");
    }
    
    char fullpath[128];
    sprintf(fullpath, "%s/%s", CACHE_DIR, cachefile[bt]);
    int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    my_assert(fd > 0, "open cached_database_write");
    
    ssize_t towrite = ds[bt].size * ds[bt].unit_size + sizeof(cached_database_t);
    while (towrite)
    {
        ssize_t writtenthistime = write(fd, ((char *)ds[bt].db) + ds[bt].size * ds[bt].unit_size + sizeof(cached_database_t) - towrite, towrite < 100000000 ? towrite : 100000000);
        my_assert(writtenthistime > 0, "cached_database_write write");
        towrite -= writtenthistime;
    }
    close(fd);
}

void ensure_size_for_more_items(int bt, int count)
{
    my_assert(ds[bt].db != NULL, "ensure_size_for_more_items non-null");
    while (ds[bt].length + count > ds[bt].size)
    {
        my_assert(ds[bt].unit_size > 0, "ensure_size_for_more_items unit_size > 0");
        if (ds[bt].size)
            ds[bt].size *= 2;
        else
            ds[bt].size = 1024;
            
        ds[bt].db = realloc(ds[bt].db, ds[bt].size * ds[bt].unit_size + sizeof(cached_database_t));
        my_assert(ds[bt].db != NULL, "realloc ensure_size_for_more_items");
    }
}

int cmp_compacted_filename_t(const void *ap, const void *bp)
{
    const compacted_filename_t *a = (const compacted_filename_t *) ap;
    const compacted_filename_t *b = (const compacted_filename_t *) bp;
    if (a->h < b->h) return -1;
    if (a->h > b->h) return 1;
    if (a->l < b->l) return -1;
    if (a->l > b->l) return 1;    
    return 0;
}


int binary_search_for_filename(char *name)
{
    compacted_filename_t fn;
    name[32] = '\0';
    fn.l = strtoull(name + 16, NULL, 16);
    name[16] ='\0';
    fn.h = strtoull(name, NULL, 16);

    // first, a binary search We need to find the index 0..buf_len (inclusive)
    // for which all values less than name will be less than index
    // index *may* point to the same as name but all larger than name will be index or more
    int a = 0;
    int b = ds[FILES].length;
    
    if (a != b)    // is there an array to look in?
    {
        while (a != b)
        {
            int m = (a + b) / 2;
    
            if (cmp_compacted_filename_t(&ds[FILES].db->files[m], &fn) < 0)
                a = m + 1;
            else
                b = m;
        }
        
        // we found the best location in the array, but is it exactly the same key?
        if (cmp_compacted_filename_t(&ds[FILES].db->files[a], &fn) == 0)
            return a;   // we found it!
    }
    
    return -1;
}


void recurse_in_directory_for_md5_files(MD5_CTX *cp, int dirfd, const char *d_name, char **buf, uint64_t *buf_size)
{
    dirfd = openat(dirfd, d_name, O_RDONLY);
    my_assert(dirfd > 0, "recurse_in_directory_for_md5_files openat dir");

    DIR *dd = fdopendir(dirfd);
    my_assert(dd != NULL, "recurse_in_directory_for_md5_files fdopendir");
    
    struct dirent *de;
    while ((de = readdir(dd)) != NULL)
        if (de->d_name[0] != '.')
        {
            if (de->d_type == DT_REG && strstr(de->d_name, ".md5s"))
            {
                int fd = openat(dirfd, de->d_name, O_RDONLY);
                my_assert(fd > 0, "recurse_in_directory_for_md5_files openat file");
                struct stat statbuf;
                my_assert(fstat(fd, &statbuf) == 0, "recurse_in_directory_for_md5_files fstat");
                
                if (cp)
                {
                    MD5_Update(cp, (unsigned char*) (de->d_name), strlen(de->d_name));  // I hope these files are always parsed in the same order
                    MD5_Update(cp, (unsigned char*) (&statbuf.st_size), sizeof(off_t));
                    MD5_Update(cp, (unsigned char*) (&statbuf.st_mtim), sizeof(struct timespec));
                }
                else
                {
                    uint64_t orig_size = *buf_size;

                    *buf_size += statbuf.st_size;
                    *buf = realloc(*buf, *buf_size);
                    my_assert(*buf != NULL, "recurse_in_directory_for_md5_files realloc");
                    char *read_into_here = *buf + orig_size;

                    while (statbuf.st_size)
                    {
                        ssize_t bytesread = read(fd, read_into_here, statbuf.st_size < 100000000 ? statbuf.st_size : 100000000);
                        my_assert(bytesread > 0, "recurse_in_directory_for_md5_files read");
                        statbuf.st_size -= bytesread;
                        read_into_here += bytesread;
                    }
                }
                my_assert(close(fd) == 0, "recurse_in_directory_for_md5_files close");
            }

            if (de->d_type == DT_DIR)
                recurse_in_directory_for_md5_files(cp, dirfd, de->d_name, buf, buf_size);
        }
    my_assert(closedir(dd) == 0, "recurse_in_directory_for_md5_files closedir");
}

void cached_database_calcsignature_FILE(MD5_CTX *c)
{
    struct stat statbuf;
    char fullpath[128];
    sprintf(fullpath, "%s/%s", MOUNTED_DISK, "index");
    int stat_return = stat(fullpath, &statbuf);
    if (stat_return != 0)
        printf("index directory unreachable: if the disk mounted?\nsudo mount /dev/disk/by-id/usb-WD_Elements_25A3_5647483645314547-0:0 /home/joeruff/m\n");
    my_assert(stat_return == 0, "accessing index directory");
    MD5_Update(c, (unsigned char *) (&statbuf.st_mtime), sizeof(statbuf.st_mtime));
}

void cached_database_recalculate_FILE()
{
    char fullpath[128];
    sprintf(fullpath, "%s/%s", MOUNTED_DISK, "index");
    DIR *dd = opendir(fullpath);
    my_assert(dd != NULL, "opendir");
    
    struct dirent *de;
    while ((de = readdir(dd)) != NULL)
    if (de->d_name[0] != '.')
    {
        my_assert(strlen(de->d_name) == 36, "filename length");
        ensure_size_for_more_items(FILES, 1);
        ds[FILES].db->files[ds[FILES].length].l = strtoull(de->d_name + 16, NULL, 16);
        de->d_name[16] ='\0';
        ds[FILES].db->files[ds[FILES].length].h = strtoull(de->d_name, NULL, 16);
        ds[FILES].length++;
    }
    my_assert(closedir(dd) == 0, "closedir");
    
    qsort(&(ds[FILES].db->files[0]), ds[FILES].length, ds[FILES].unit_size, cmp_compacted_filename_t);
}

void cached_database_calcsignature_MD5S(MD5_CTX *cp)
{
    int dirfd = open(MOUNTED_DISK, O_RDONLY);
    my_assert(dirfd > 0, "open MOUNTED_DISK");    
    // The MD5s are linked to the filename. So, if the filenames change, the MD5s have to change, too...
    my_assert(ds[FILES].size == ds[FILES].length, "verify that FILES have been compacted");
    MD5_Update(cp, (unsigned char*) (ds[FILES].db), ds[FILES].size * ds[FILES].unit_size);
    recurse_in_directory_for_md5_files(cp, dirfd, "in", NULL, NULL);
}

void cached_database_recalculate_MD5S()
{
    int dirfd = open(MOUNTED_DISK, O_RDONLY);
    my_assert(dirfd > 0, "open MOUNTED_DISK 2");   
        
    assert(ds[MD5S].db != NULL);
    uint64_t actual_size_in_bytes = sizeof(cached_database_t) + ds[FILES].length * ds[MD5S].unit_size;
    ds[MD5S].db = realloc(ds[MD5S].db, actual_size_in_bytes);
    my_assert(ds[MD5S].db != NULL, "realloc cached_database_write");
    recurse_in_directory_for_md5_files(NULL, dirfd, "in", (char **)&ds[MD5S].db, &actual_size_in_bytes);
//    recurse_in_directory_for_md5_files(NULL, dirfd, "in/cefc5d7f36884d0c9d2a1887e8107704/", (char **)&ds[MD5S].db, &actual_size_in_bytes);
    
    assert(ds[MD5S].db != NULL);
    char *scanfrom = ((char *)ds[MD5S].db) + sizeof(cached_database_t) + ds[FILES].length * ds[MD5S].unit_size;
    char *scan_end = ((char *)ds[MD5S].db) + actual_size_in_bytes;
    ds[MD5S].length = ds[FILES].length;
    
    memset(((char *)ds[MD5S].db), 0, sizeof(cached_database_t) + ds[FILES].length * ds[MD5S].unit_size);
    while (scanfrom < scan_end)
    {
        char *nextnl = scanfrom;
        while ((nextnl < scan_end) && (*nextnl != '\n'))
            nextnl++;
        my_assert(nextnl != scan_end, "newlines");
        *nextnl = '\0';
        
        int idx = binary_search_for_filename(scanfrom);
        
        if (idx >= 0)
        {
            scanfrom += 32 + 1 + 24 + 1;    // skip md5 and file hdr
            while (isdigit(*scanfrom++));   // create_time
            while (isdigit(*scanfrom++));   // mod_time
            while (isdigit(*scanfrom++));   // size
            while (isdigit(*scanfrom++));   // ??
            while (isdigit(*scanfrom++));   // symlinks?
            
            // linked list
            ds[MD5S].db->md5s[ds[MD5S].length] = ds[MD5S].db->md5s[idx];
            ds[MD5S].db->md5s[idx] = ds[MD5S].length;
            ds[MD5S].length++;
            strcpy((char *) &ds[MD5S].db->md5s[ds[MD5S].length], scanfrom);
            ds[MD5S].length += (strlen(scanfrom) / sizeof(uint32_t)) + 1;
        }        
        scanfrom = nextnl + 1;
    }
    ds[MD5S].size = ds[MD5S].length;
    ds[MD5S].db = realloc(ds[MD5S].db, ds[MD5S].size * ds[MD5S].unit_size + sizeof(cached_database_t));
}

/*
int mylongcmp(void *ap, void *bp)
{
    long *a = (long *)ap;
    long *b = (long *)bp;
    if (*a < *b)
        return -1;
    if (*a > *b)
        return 1;
    double *da = (double *)(a + 2);
    double *db = (double *)(b + 2);
    if (*da < *db)
        return -1;
    if (*da > *db)
        return 1;
    return 0;
}
*/

int main()
{
    strcpy(logfile_path, "server.log");
    write_a_log_line("server starting");
    printf("server starting\n");

    srandom(time(NULL));

    int i;
    for (i = 0; i < NUM_CACHES; i++)
    {
        cached_database_load(i);
        MD5_CTX c;
        MD5_Init(&c);
        ds[i].cached_database_calcsignature(&c);
        unsigned char result[MD5_DIGEST_LENGTH];
        MD5_Final(result, &c);
        if (memcmp(ds[i].db->md5, result, MD5_DIGEST_LENGTH) != 0)
        {
            // invalidate cache
            printf("cached_database %d/%d invalid - recalculating\n", i, NUM_CACHES);
            ds[i].cached_database_recalculate();
            
            MD5_Init(&c);
            ds[i].cached_database_calcsignature(&c);
            unsigned char result_after[MD5_DIGEST_LENGTH];
            MD5_Final(result_after, &c);
            my_assert(memcmp(result, result_after, MD5_DIGEST_LENGTH) == 0, "changed while calculatin cached db");
            
            memcpy(ds[i].db->md5, result, MD5_DIGEST_LENGTH);
            cached_database_write(i);
        }
        printf("cached_database %d/%d loaded\n", i, NUM_CACHES);
    }
    
/*

    for (i = 0; i < 100; i++)
    {
        uint32_t idx = ds[MD5S].db->md5s[i];
        if (idx == 0)
            printf("%d %016lx%016lx == nothing ==\n", i, ds[FILES].db->files[i].h, ds[FILES].db->files[i].l);
        else while (idx)
        {
            printf("%d %016lx%016lx %d %s\n", i, ds[FILES].db->files[i].h, ds[FILES].db->files[i].l, idx, (char *) &ds[MD5S].db->md5s[idx + 1]);
            idx = ds[MD5S].db->md5s[idx];
        }
    }

    return -1;
        
    sprintf(logline, "link %ld bytes of md5s files", lenindexfiles);
    write_a_log_line(logline);


    long nummd5slines = 0;
    long off = 0;
    while (off < lenindexfiles)
    {
        char *nextnl = strchr(indexfiles + off, '\n');
        my_assert(nextnl != NULL, "newlines");
        *nextnl = '\0';
        nummd5slines++;
        long idx = binary_search_for_filename(indexfiles + off);
        if (idx >= 0)
        {
            if (ll == NULL || numll == sizell)
            {
                if (sizell)
                    sizell *= 2;
                else
                    sizell = 1000;
                ll = realloc(ll, sizell * sizeof(ll[0]));
                my_assert(ll != NULL, "realloc ll");
            }
            ll[numll].offset = off;
            ll[numll].lloffset = files[idx].lloffset;
            files[idx].lloffset = numll;
            numll++;
        }
        off = nextnl - indexfiles + 1;
    }
    
    sprintf(logline, "compute hist nummd5slines %ld numll %ld", nummd5slines, numll);
    write_a_log_line(logline);


    long hist[20] = {0};
    for (i = 0; i < numfiles; i++)
    {
        int count = 0;
        long lloffset = files[i].lloffset;
        while (lloffset)
        {
            count++;
            lloffset = ll[lloffset].lloffset;
            my_assert(lloffset >= 0 && lloffset < numll, "linked list bounds checking logic error");
        }
        if (count > 19) count = 19;
        hist[count]++;
    }
    for (i = 0; i < 19; i++)
    {
        sprintf(logline, "hist %d %ld", i, hist[i]);
        write_a_log_line(logline);
    }
    sprintf(logline, "hist >%d %ld", 19, hist[19]);
    write_a_log_line(logline);
            
    FILE *matched = fopen("/home/joeruff/server/parallel_matches/all.txt", "r");
    assert(matched);
    
    long matcha, matchb;
    double matchm;
    while (fscanf(matched, "%ld %ld %lg\n", &matcha, &matchb, &matchm) == 3)
    {
        if (nummatchll + 2 >= sizematchll)
        {
            if (sizematchll == 0)
                sizematchll = 1000000;
            else
                sizematchll *= 2;
            matchll = realloc(matchll, sizematchll * sizeof(matchll[0]));
            my_assert(matchll != NULL, "matchll realloc");
        }
        matchll[nummatchll].matcha = matcha;
        matchll[nummatchll].matchb = matchb;
        matchll[nummatchll].matchm = matchm;
        nummatchll++;
        matchll[nummatchll].matcha = matchb;
        matchll[nummatchll].matchb = matcha;
        matchll[nummatchll].matchm = matchm;
        nummatchll++;
    }

    qsort(matchll, nummatchll, sizeof(matchll[0]), (int (*)(const void *, const void *)) mylongcmp);

    for (i = 0; i < numfiles; i++)
        files[i].matchlloffset = -1;
        
    long scanmatchlloffset = matchll[0].matcha;
    files[scanmatchlloffset].matchlloffset = 0;
    for (i = 1; i < nummatchll; i++)
        if (matchll[i].matcha != scanmatchlloffset)
        {
            scanmatchlloffset = matchll[i].matcha;
            files[scanmatchlloffset].matchlloffset = i;
        }
    
    sprintf(logline, "nummatchll %ld", nummatchll);
    write_a_log_line(logline);
    sprintf(logline, "%ld %ld %g", matchll[0].matcha, matchll[0].matchb, matchll[0].matchm);
    write_a_log_line(logline);
    sprintf(logline, "%ld %ld %g", matchll[1].matcha, matchll[1].matchb, matchll[1].matchm);
    write_a_log_line(logline);
    sprintf(logline, "%ld %ld %g", matchll[2].matcha, matchll[2].matchb, matchll[2].matchm);
    write_a_log_line(logline);
    write_a_log_line("...");
    sprintf(logline, "%ld %ld %g", matchll[nummatchll - 3].matcha, matchll[nummatchll - 3].matchb, matchll[nummatchll - 3].matchm);
    write_a_log_line(logline);
    sprintf(logline, "%ld %ld %g", matchll[nummatchll - 2].matcha, matchll[nummatchll - 2].matchb, matchll[nummatchll - 2].matchm);
    write_a_log_line(logline);
    sprintf(logline, "%ld %ld %g", matchll[nummatchll - 1].matcha, matchll[nummatchll - 1].matchb, matchll[nummatchll - 1].matchm);
    write_a_log_line(logline);
    
    
    fclose(matched);

    /* =========               /home/joeruff/server/index.html              ========= */

    char logline[100];

    int fd = open("index.html", O_RDONLY);
    my_assert(fd > 0, "open index.html");
    struct stat statbuf;
    assert(fstat(fd, &statbuf) == 0);
    char *index_buf = malloc(statbuf.st_size + 1000);
    my_assert(index_buf != NULL, "malloc");
    sprintf(index_buf, "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", statbuf.st_size);
    int index_buf_len = strlen(index_buf);
    my_assert(read(fd, index_buf + index_buf_len, statbuf.st_size) == statbuf.st_size, "read index.html in one attempt");
    index_buf_len += statbuf.st_size;
    close(fd);
    
    for (i = 0; i < MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 1; i++)
        fds[i].fd = -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(80);
    
    fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].fd = socket(AF_INET, SOCK_STREAM, 0);
    my_assert(fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].fd != -1, "socket");

    int yes = 1;    
    my_assert(setsockopt(fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != -1, "setsockopt");

    my_assert(bind(fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].fd, (struct sockaddr *) &addr,  sizeof(addr)) == 0, "bind");
    my_assert(listen(fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].fd, 5) == 0, "listen");

    int dirfd_index = open("/home/joeruff/m/index/", O_RDONLY);
    my_assert(dirfd_index > 0, "open /home/joeruff/m/index/");
    int dirfd_i = open("/home/joeruff/m/i/", O_RDONLY);
    my_assert(dirfd_i > 0, "open /home/joeruff/m/i/");

    write_a_log_line("start loop");
    printf("start loop\n");
    while (1)
    {
        for (i = 0; i < MAXIMUM_NUMBER_OF_CLIENT_LISTENERS; i++)
            fds[i].events = POLLIN | (bytes_to_send[i] ? POLLOUT : 0);
        fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].events = POLLIN;

        my_assert(poll(fds, MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 1, -1) > 0, "poll");
        
        for (i = 0; i < MAXIMUM_NUMBER_OF_CLIENT_LISTENERS; i++)
        if (fds[i].fd != -1 && fds[i].revents)
        {
            if (fds[i].revents & POLLOUT)
            {
                ssize_t bytes_sent = send(fds[i].fd, outbuffers[i], bytes_to_send[i], 0);
                my_assert(bytes_sent > 0, "send");
                bytes_to_send[i] -= bytes_sent;
                memmove(outbuffers[i], outbuffers[i] + bytes_sent, bytes_to_send[i]);
            }
            if (fds[i].revents & POLLIN)
            {
                if (inbuffers[i] == NULL || bytes_received[i] + 1 >= inbuffer_size[i])
                {
                    if (inbuffer_size[i])
                        inbuffer_size[i] *= 2;
                    else
                        inbuffer_size[i] = 10000;
                    inbuffers[i] = realloc(inbuffers[i], inbuffer_size[i]);
                    my_assert(inbuffers[i] != NULL, "realloc inbuffers");
                }
                
                ssize_t bytes_recv = recv(fds[i].fd, inbuffers[i] + bytes_received[i], inbuffer_size[i] - bytes_received[i] - 1, 0);
                if (bytes_recv < 0)
                {
                    if (bytes_recv == 0)
                        write_a_log_line("remote client closed conection");
                    else
                    {
                        sprintf(logline, "remote client closed %ld", (long) bytes_recv);
                        write_a_log_line(logline);
                    }
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                else
                {
                    bytes_received[i] += bytes_recv;
                    inbuffers[i][bytes_received[i]] = '\0';
                    if (strstr(inbuffers[i], "\r\n\r\n"))
                    {
                        printf("->%s<-\n", inbuffers[i]);
                        // let's just assume we got the whole request. This is dangerous, of course....
                        bytes_received[i] = 0;
                        
                        if (strncmp(inbuffers[i], "GET / ", 6) == 0)
                        {
                            getspace(i, index_buf_len);
                            memcpy(outbuffers[i] + bytes_to_send[i], index_buf, index_buf_len);
                            incorporate_buffer(i, index_buf_len);
                        }
                        else if (strncmp(inbuffers[i], "GET /a ", 7) == 0)
                        {
                            int idx = random() % ds[FILES].length;
                            
                            /*
                            char pap1[2000] = "";
                            char pap[2000] = "";
                            long lloffset = files[idx].lloffset;
                            if (lloffset)
                            {
                                char *fullline = indexfiles + ll[lloffset].offset;
                                int i;
                                for (i = 0; i < 7; i++)
                                {
                                    while (*fullline != ' ') fullline++;
                                    fullline++;
                                }
                                strcpy(pap1, fullline);
                            }
                            while (lloffset)
                            {
                                char *fullline = indexfiles + ll[lloffset].offset;
                                strcat(pap, "<a onclick=myFunction('");
                                strcat(pap, fullline + strlen(fullline) - 40);
                                strcat(pap, "')>");
                                strcat(pap, fullline + strlen(fullline) - 40);
                                strcat(pap, "</a><br>");
                                lloffset = ll[lloffset].lloffset;
                            }
                            
                            sprintf(logline, "idx %d", idx);
                            write_a_log_line(logline);
                            sprintf(logline, "files[idx].matchlloffset %ld", files[idx].matchlloffset);
                            write_a_log_line(logline);
                            
                            long mymatches[3] = {0};
                            if (files[idx].matchlloffset != -1)
                            {

                                sprintf(logline, "matchll[files[idx].matchlloffset + 0].matcha %ld", matchll[files[idx].matchlloffset + 0].matcha);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 0].matchb %ld", matchll[files[idx].matchlloffset + 0].matchb);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 0].matchm %g", matchll[files[idx].matchlloffset + 0].matchm);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 1].matcha %ld", matchll[files[idx].matchlloffset + 1].matcha);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 1].matchb %ld", matchll[files[idx].matchlloffset + 1].matchb);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 1].matchm %g", matchll[files[idx].matchlloffset + 1].matchm);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 2].matcha %ld", matchll[files[idx].matchlloffset + 2].matcha);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 2].matchb %ld", matchll[files[idx].matchlloffset + 2].matchb);
                                write_a_log_line(logline);
                                sprintf(logline, "matchll[files[idx].matchlloffset + 2].matchm %g", matchll[files[idx].matchlloffset + 2].matchm);
                                write_a_log_line(logline);
                                sprintf(logline, "%ld %s", matchll[files[idx].matchlloffset + 0].matchb, files[matchll[files[idx].matchlloffset + 0].matchb].d_name);
                                write_a_log_line(logline);
                                sprintf(logline, "%ld %s", matchll[files[idx].matchlloffset + 1].matchb, files[matchll[files[idx].matchlloffset + 1].matchb].d_name);
                                write_a_log_line(logline);
                                sprintf(logline, "%ld %s", matchll[files[idx].matchlloffset + 2].matchb, files[matchll[files[idx].matchlloffset + 2].matchb].d_name);
                                write_a_log_line(logline);

                                mymatches[0] = matchll[files[idx].matchlloffset + 0].matchb;
                                mymatches[1] = matchll[files[idx].matchlloffset + 1].matchb;
                                mymatches[2] = matchll[files[idx].matchlloffset + 2].matchb;
                            }
                            char buf[5000];
                            sprintf(buf, "{\"a\":\"%s\", \"md5\":\"%s\", \"md51\":\"%s\", \"md52\":\"%s\", \"md53\":\"%s\", \"d\":\"%s\"}", pap1, files[idx].d_name, files[mymatches[0]].d_name, files[mymatches[1]].d_name, files[mymatches[2]].d_name, pap);
                            getspace(i, 5000);
                            sprintf(outbuffers[i] + bytes_to_send[i], "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n", strlen(buf));
                            incorporate_buffer(i, strlen(outbuffers[i] + bytes_to_send[i]));
                            strcpy(outbuffers[i] + bytes_to_send[i], buf);
                            incorporate_buffer(i, strlen(outbuffers[i] + bytes_to_send[i]));
                            */
                            
                            char buf[5000];
                            char tttbuf[100];
                            long_to_char(idx, tttbuf);

                            sprintf(buf, "{\"a\":\"%s\"}", tttbuf);
                            getspace(i, 5000);
                            sprintf(outbuffers[i] + bytes_to_send[i], "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n", strlen(buf));
                            incorporate_buffer(i, strlen(outbuffers[i] + bytes_to_send[i]));
                            strcpy(outbuffers[i] + bytes_to_send[i], buf);
                            incorporate_buffer(i, strlen(outbuffers[i] + bytes_to_send[i]));

                        }
                        else
                        {
                            if (strncmp(inbuffers[i], "GET /", 5) == 0)
                            {
                                char *space = strchr(inbuffers[i] + 5, ' ');
                                if (space)
                                {
                                    int fd = -1;

                                    unsigned long imageindex = char_to_long(inbuffers[i] + 6);
                                    if (imageindex < ds[FILES].length)
                                    {
                                        char filename_buffer[100];
                                        sprintf(filename_buffer, "%016lx%016lx", ds[FILES].db->files[imageindex].h, ds[FILES].db->files[imageindex].l);
                                        if (inbuffers[i][5] == 'i')
                                        {
                                            strcat(filename_buffer, ".jpg");
                                            printf("i: %s\n", filename_buffer);
                                            fd = openat(dirfd_index, filename_buffer, O_RDONLY);
                                        }
                                        else if (inbuffers[i][5] == 's')
                                        {
                                            printf("s %s\n", filename_buffer);
                                            fd = openat(dirfd_i, filename_buffer, O_RDONLY);
                                        }
                                    }

                                    if (fd > 0)
                                    {
                                        struct stat statbuf;
                                        assert(fstat(fd, &statbuf) == 0);
                                        getspace(i, 1000 + statbuf.st_size);
                                        sprintf(outbuffers[i] + bytes_to_send[i], "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nContent-Type: image/jpeg\r\nCache-Control: max-age=31536000, immutable\r\nContent-Length: %ld\r\n\r\n", statbuf.st_size);
                                        incorporate_buffer(i, strlen(outbuffers[i] + bytes_to_send[i]));
                                        while (statbuf.st_size)
                                        {
                                            ssize_t read_this_time = read(fd, outbuffers[i] + bytes_to_send[i], statbuf.st_size);
                                            my_assert(read_this_time > 0, "read file");
                                            incorporate_buffer(i, read_this_time);
                                            statbuf.st_size -= read_this_time;
                                        }
                                        close(fd);
                                    }
                                    else
                                        sendout(i, "HTTP/1.0 404 Not Found\r\nConnection: keep-alive\r\n\r\n");
                                }
                                else
                                    sendout(i, "HTTP/1.0 404 Not Found\r\nConnection: keep-alive\r\n\r\n");
                            }
                            else
                                sendout(i, "HTTP/1.0 404 Not Found\r\nConnection: keep-alive\r\n\r\n");
                        }
                    }            
                }
            }
            if (fds[i].revents & ( ~ (POLLOUT | POLLIN) ) )
                write_a_log_line("remote client conection revents something other than POLLIN or POLLOUT");
        }

        if (fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].revents)
        {
            if (fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].revents == POLLIN)
            {
                write_a_log_line("accept listener");
                for (i = 0; i < MAXIMUM_NUMBER_OF_CLIENT_LISTENERS; i++)
                    if (fds[i].fd == -1)
                        break;
                my_assert(i != MAXIMUM_NUMBER_OF_CLIENT_LISTENERS, "ran out of space for new listeners");
                        
                fds[i].fd = accept(fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 0].fd, NULL, NULL);
                my_assert(fds[i].fd > 1, "accept");
                
                bytes_to_send[i] = 0;
                bytes_received[i] = 0;
            }
            else
                write_a_log_line("listen revents != POLLIN");
        }
    }
}