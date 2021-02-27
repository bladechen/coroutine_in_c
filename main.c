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

void timer_cb(void* argv)
{
    resume_coro((struct coroutine*)(argv));
    return;
}
void coro_sleep(void* argv,int interval)
{
    struct coroutine* co = (struct coroutine*)(argv);
    time_t local = time(NULL);
    registe_timer(timer_cb, co, local + interval);
    yield_coro();
    return;
}
void func1(void* argv)
{
    printf ("start func1\n");
    for (int i = 0;i < 5; i ++)
    {
        printf ("%p, func1 [%d]: %llu\n",&i, i, (long long unsigned int)(time_stamp));
        coro_sleep(argv, 2);
    }
    printf ("end func1\n");
}

void func2(void* argv)
{
    printf ("start func2\n");

    for (int i = 0;i < 10; i ++)
    {
        printf ("%p, func2 [%d]: %llu\n",&i , i, (long long unsigned int)(time_stamp));
        coro_sleep(argv,4 );

    }
    printf ("end func2\n");
}


int sfd  = 0;

struct FDInfo {
    int fd;
    struct coroutine* coro;
};


int main(int argc, char** argv)
{
	bootstrap_coro_env();
	int s;
	int efd;
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
	if(efd == -1)
	{
		perror("epoll_create");
		abort();
	}

    struct FDInfo* fd_info = (struct FDInfo*)(malloc(sizeof(struct FDInfo)));
    fd_info->fd = sfs;
    fd_info->coro =


	event.data.fd = sfd;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
	if(s == -1)
	{
		perror("epoll_ctl");
		abort();
	}

	while (1) {
		int n, i;
		fprintf(stdout, "Blocking and waiting for epoll event...\n");
		n = epoll_wait(efd, events, MAXEVENTS, -1);
		fprintf(stdout, "Received epoll event\n");
		assert(n >= 0);
		for (i=0; i<n; i++) {


		}
	}


	if (events) free(events);
    close(sfd);

	shutdown_coro_env();
	return 0;
}
