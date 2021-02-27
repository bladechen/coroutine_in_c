#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAXEVENTS 64

#include "coro.h"


static int make_socket_non_blocking(int sfd)
{
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if(flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if(s == -1)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}

static int create_and_bind(char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddrinfo(NULL, port, &hints, &result);
    if(s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for(rp=result;rp!=NULL;rp=rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sfd == -1)
        {
            continue;
        }

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if(s == 0)
        {
            break;
        }

        close(sfd);
    }

    if(rp == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(result);
    return sfd;
}


time_t time_stamp;


struct timer_obj
{
    void* argv;
    coroutine_func cb;
    time_t exe_time;
};

struct timer_obj timer_list[100] ;
int  timer_count = 0;
void registe_timer(coroutine_func cb, void * argv, time_t next)
{

    printf ("registe_timer coro: %p, next: %llu\n\n", argv, (unsigned long long)next);
    for (int i = 0; i < timer_count ; i ++)
    {
        if (timer_list[i].argv == argv)
        {
            timer_list[i].exe_time = next;
            return;
        }
    }
    timer_list[timer_count].exe_time = next;
    timer_list[timer_count].argv = argv;
    timer_list[timer_count].cb = cb;
    timer_count ++;
}

void detached_timer(void * argv)
{

    for (int i = 0; i < timer_count ; i ++)
    {
        if (timer_list[i].argv == argv)
        {
            timer_list[i].argv = NULL;
            return;
        }
    }
}



void timer_cb(void* argv) {
    printf ("resume coro %p\n", argv);
    resume_coro((struct coroutine*)(argv));
    return;
}
void coro_sleep(void* argv,int interval)
{
    struct coroutine* co = (struct coroutine*)(argv);
    time_t local = time(NULL);
    registe_timer(timer_cb, co, local + interval);
    yield_coro();
    detached_timer(co);
    return;
}

int sfd = 0;
int efd = 0;

struct FDInfo {
    int fd;
    struct coroutine* coro;
};

void coro_recv(void* argv) {
    int fd = ((struct FDInfo*)(argv))->fd;
    printf ("calling coro_recv on fd: %d\n", fd);
	ssize_t count;
	char buf[512];

    while(1)
    {
        count = read(fd, buf, sizeof(buf));
        printf ("read count %d %d\n", (int)count, errno);
        if(count == -1)
        {
            if(errno != EAGAIN)
            {
                perror("read");
                abort();
            }
            printf ("yield\n");
            yield_coro();
            continue;
        }
        else if(count == 0)
        {
            break;
        }
        coro_sleep(((struct FDInfo*)(argv))->coro, 5);
        printf ("sleep back read count %zu\n", count);
        buf[count] = '\n';
        int s = write(1, buf, count+1);
        if(s == -1)
        {
            perror("write");
            abort();
        }

        s = write(fd, buf, count+1);
        if(s == -1)
        {
            perror("socket write");
            abort();
        }
    }
}

void coro_listen(void* argv) {


    int fd =  ((struct FDInfo*)(argv))->fd;
    printf ("calling coro_listen on fd: %d\n", fd);
    while (1) {
        struct epoll_event event ;
        struct sockaddr in_addr;
        socklen_t in_len;
        int infd;
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

        in_len = sizeof(in_addr);
        infd = accept(fd, &in_addr, &in_len);
        if(infd == -1)
        {
            if((errno == EAGAIN) ||
               (errno == EWOULDBLOCK))
            {
                yield_coro();
                continue;
            }
            else
            {
                perror("accept");
                break;
            }
        }
        int s = getnameinfo(&in_addr, in_len,
                        hbuf, sizeof(hbuf),
                        sbuf, sizeof(sbuf),
                        NI_NUMERICHOST | NI_NUMERICSERV);
        if(s == 0)
        {
            printf("Accepted connection on descriptor %d"
                   "(host=%s, port=%s)\n", infd, hbuf, sbuf);
        }

        s = make_socket_non_blocking(infd);
        if(s == -1)
            abort();

        struct FDInfo* fd_info = (struct FDInfo*)(malloc(sizeof(struct FDInfo)));
        fd_info->fd = infd;
        fd_info->coro = create_coro(coro_recv, fd_info);
        printf ("recv coro %p\n", fd_info->coro);

        event.data.ptr = fd_info;
        event.events = EPOLLIN | EPOLLET;
        s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
        if(s == -1) {
            perror("epoll_ctl");
            abort();
        }

    }
}

int main(int argc, char** argv)
{
	bootstrap_coro_env();
	int s;
	struct epoll_event event ;
	struct epoll_event *events = NULL;

	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sfd = create_and_bind(argv[1]);
	if(sfd == -1) abort();

	s = make_socket_non_blocking(sfd);
	if(s == -1) abort();

	s = listen(sfd, SOMAXCONN);
	if(s == -1) {
		perror("listen");
		abort();
	}

	efd = epoll_create1(0);
	if(efd == -1) {
		perror("epoll_create");
		abort();
	}

    struct FDInfo* fd_info = (struct FDInfo*)(malloc(sizeof(struct FDInfo)));
    fd_info->fd = sfd;
    fd_info->coro = create_coro(coro_listen, fd_info);

	event.data.ptr = fd_info;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
	if(s == -1) {
		perror("epoll_ctl");
		abort();
	}
    /* Buffer where events are returned */
    events = calloc(MAXEVENTS, sizeof(event));

	while (1) {
        schedule_loop();
        time_stamp = time(NULL);
        for (int i = 0; i < timer_count; i ++)
        {
            /* printf("[%d] exe_time: %llu, cur time: %llu\n",i,(unsigned long long)timer_list[i].exe_time ,(unsigned long long)time_stamp); */
            if (timer_list[i].exe_time <= time_stamp && timer_list[i].argv)
            {
                timer_list[i].cb(timer_list[i].argv);
            }
        }
		int n, i;
		/* fprintf(stdout, "Blocking and waiting for epoll event...\n"); */
		n = epoll_wait(efd, events, MAXEVENTS, 1000);
		/* fprintf(stdout, "Received epoll event %d %d\n", n, errno); */
		/* assert(n >= 0); */
		for (i=0; i<n; i++) {
            struct FDInfo* cur_fd_info = (struct FDInfo*)(events[i].data.ptr);
             if((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP)) {
                 fprintf(stderr, "epoll error at fd: %d\n", cur_fd_info->fd);
                 destroy_coro(cur_fd_info->coro);
                 close(cur_fd_info->fd);
                 free(cur_fd_info);
                 continue;
             } else {
                 printf ("resume %p\n", cur_fd_info->coro);
                 resume_coro(cur_fd_info->coro);
             }
		}
	}


	if (events) free(events);
    close(sfd);

	shutdown_coro_env();
	return 0;
}
