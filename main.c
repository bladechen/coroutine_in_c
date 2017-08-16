#include <stdio.h>
#include <unistd.h>
#include "coro.h"
#include <time.h>

time_t time_stamp ;


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

    /* printf ("registe_timer coro: %p, next: %llu\n", argv, (unsigned long long)next); */
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
        printf ("func1 [%d]: %llu\n",i, (long long unsigned int)(time_stamp));
        coro_sleep(argv, 2);
    }
    printf ("end func1\n");
}

void func2(void* argv)
{
    printf ("start func2\n");

    for (int i = 0;i < 10; i ++)
    {
        printf ("func2 [%d]: %llu\n",i, (long long unsigned int)(time_stamp));
        coro_sleep(argv,4 );

    }
    printf ("end func2\n");
}

int main()
{
    bootstrap_coro_env();

    struct coroutine* co1 = create_coro(func1, NULL);
    co1->_argv = co1;

    struct coroutine* co2 = create_coro(func2, NULL);
    printf ("co1 : 0x%p\n", co1);
    printf ("co2 : 0x%p\n", co2);
    co2->_argv = co2;
    /* func1(co1); */
    /* func2(co2); */

    while (1)
    {
        time_stamp = time(NULL);
        for (int i = 0; i < timer_count; i ++)
        {
            /* printf("[%d] exe_time: %llu, cur time: %llu\n",i,(unsigned long long)timer_list[i].exe_time ,(unsigned long long)time_stamp); */
            if (timer_list[i].exe_time <= time_stamp)
            {
                timer_list[i].cb(timer_list[i].argv);
            }
        }
        if (co1->_status == COROUTINE_INIT && co2->_status == COROUTINE_INIT)
        {
            break; //finish
        }
        /* loop_check(); */
        /* printf ("\n\n\nin daemon loop\n"); */
        schedule_loop();
        /* printf ("end  daemon loop\n\n"); */
        usleep(200000); // 200ms?
    }
    shutdown_coro_env();
    return 0;
}
