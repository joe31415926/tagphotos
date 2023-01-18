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

#define MAXIMUM_NUMBER_OF_CLIENT_LISTENERS (5)
char *inbuffers[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {NULL};
char *outbuffers[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {NULL};
int bytes_to_send[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
int bytes_received[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
int inbuffer_size[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
int outbuffer_size[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS] = {0};
struct pollfd fds[MAXIMUM_NUMBER_OF_CLIENT_LISTENERS + 1];

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
        write_a_log_line(mess);
        exit(-1);
    }
}

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

char *indexfiles = NULL;
long lenindexfiles = 0;

struct {
    long next;
    long offset;
} *ll = NULL;
long numll = 0;
long sizell = 0;

struct {
    char d_name[37];
    long lloffset;
} *files = NULL;
long numfiles = 0;
long sizefiles = 0;

int obj(const char *name)
{
    // first, a binary search We need to find the index 0..buf_len (inclusive)
    // for which all values less than name will be less than index
    // index *may* point to the same as name but all larger than name will be index or more
    int a = 0;
    int b = numfiles;
    
    if (a != b)    // is there an array to look in?
    {
        while (a != b)
        {
            int m = (a + b) / 2;
    
            if (strncmp(files[m].d_name, name, 32) < 0)
                a = m + 1;
            else
                b = m;
        }
        
        // we found the best location in the array, but is it exactly the same key?
        if (strncmp(files[a].d_name, name, 32) == 0)
            return a;   // we found it!
    }
    
    return -1;
}


void loadindexfile(int dirfd, const char *d_name)
{
    int fd = openat(dirfd, d_name, O_RDONLY);
    my_assert(fd > 0, "loadindexfile openat");
    
    struct stat statbuf;
    my_assert(fstat(fd, &statbuf) == 0, "loadindexfile fstat");
    
    lenindexfiles += statbuf.st_size;
    indexfiles = realloc(indexfiles, lenindexfiles + 1);
    my_assert(indexfiles != NULL, "loadindexfile realloc");
    indexfiles[lenindexfiles] = '\0';
    
    while (statbuf.st_size)
    {
        ssize_t bytesread = read(fd, indexfiles + lenindexfiles - statbuf.st_size, statbuf.st_size);
        my_assert(bytesread > 0, "loadindexfile read");
        statbuf.st_size -= bytesread;
    }
    my_assert(close(fd) == 0, "loadindexfile close");
}

void loadindexdir(int dirfd, const char *d_name)
{
    dirfd = openat(dirfd, d_name, O_RDONLY);
    my_assert(dirfd > 0, "loadindexdir openat");

    DIR *dd = fdopendir(dirfd);
    my_assert(dd != NULL, "loadindexdir fdopendir");
    
    struct dirent *de;
    while ((de = readdir(dd)) != NULL)
        if (de->d_name[0] != '.')
        {
            if (de->d_type == DT_REG && strstr(de->d_name, ".md5s"))
                loadindexfile(dirfd, de->d_name);

            if (de->d_type == DT_DIR)
                loadindexdir(dirfd, de->d_name);
        }
    my_assert(closedir(dd) == 0, "loadindexdir closedir");
}

int main()
{
    strcpy(logfile_path, "server.log");
    write_a_log_line("server starting. read index images");
    
    DIR *dd = opendir("/home/joeruff/m/index/");
    my_assert(dd != NULL, "opendir");
    
    struct dirent *de;
    while ((de = readdir(dd)) != NULL)
        if (de->d_name[0] != '.')
        {
            my_assert(strlen(de->d_name) == 36, "filename length");
            if (files == NULL || numfiles == sizefiles)
            {
                if (sizefiles)
                    sizefiles *= 2;
                else
                    sizefiles = 1000;
                files = realloc(files, sizefiles * sizeof(files[0]));
                my_assert(files != NULL, "realloc files");
            }
            strcpy(files[numfiles].d_name, de->d_name);
            files[numfiles].lloffset = 0;
            numfiles++;
        }
    my_assert(closedir(dd) == 0, "closedir");

    write_a_log_line("sort index images");

    qsort(files, numfiles, sizeof(files[0]), (int (*)(const void *, const void *)) strcmp);

    write_a_log_line("read md5s");

    int dirfd = open("/home/joeruff/m/", O_RDONLY);
    my_assert(dirfd > 0, "open /home/joeruff/m/");
    loadindexfile(dirfd, "pe.md5s");
    loadindexdir(dirfd, "in");
    
    write_a_log_line("link files");
    long off = 0;
    while (off < lenindexfiles)
    {
        char *nextnl = strchr(indexfiles + off, '\n');
        my_assert(nextnl != NULL, "newlines");
        *nextnl = '\0';
        long idx = obj(indexfiles + off);
        if (idx >= 0)
        {
            if (ll == NULL || numll == sizell)
            {
                if (sizell)
                    sizell *= 2;
                else
                    sizell = 1000;
                ll = realloc(ll, sizell * sizeof(files[0]));
                my_assert(ll != NULL, "realloc ll");
            }
            ll[numll].offset = off;
            ll[numll].next = files[idx].lloffset;
            files[idx].lloffset = numll;
            numll++;
        }
        off = nextnl - indexfiles + 1;
    }
    
            
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
    
    int i;
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

    write_a_log_line("start loop");
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
                if (bytes_recv == 0)
                {
                    write_a_log_line("remote client closed conection");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                else
                {
                    my_assert(bytes_recv > 0, "recv");
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
                            char buf[] = "{\"a\":\"<a href=a>a content</a>\", \"b\":\"b content\", \"c\":\"c content\"}";
                            getspace(i, 1000);
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
                                    *space = '\0';
                                    int fd = open(inbuffers[i] + 5, O_RDONLY);
                                    if (fd > 0)
                                    {
                                        struct stat statbuf;
                                        assert(fstat(fd, &statbuf) == 0);
                                        getspace(i, 1000 + statbuf.st_size);
                                        sprintf(outbuffers[i] + bytes_to_send[i], "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nContent-Type: image/jpeg\r\nContent-Length: %ld\r\n\r\n", statbuf.st_size);
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