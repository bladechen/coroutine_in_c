#include "coro.h"
#include <stdio.h>
/* #define DEBUG_CORO 1 */
static struct schedule schedule_obj;
/**
 *  @brief 汇编实现保存上下文函数
 *  @param jbf jmpbuff数组指针
 */
extern   int save_context(struct context* jbf);

/**
 *  @brief 汇编实现恢复上下文函数
 *  @param jbf jmpbuff数组指针
 *  @param ret 切回的返回值, 默认1
 */
extern   void restore_context(struct context* jbf, int ret);

/**
 *  @brief 汇编实现替换调用栈函数
 *  @param jbf jmpbuff数组指针
 *  @param esp 堆栈指针
 */
extern   void replace_esp(struct context* jbf, void* esp);

static void make_coro_runnable(struct coroutine* coro);
static struct coroutine* new_coro();
static void set_running_coro(struct coroutine* co);
static void init_context(struct coroutine* coro);

void bootstrap_coro_env()
{
    schedule_obj._pending_list = (struct list*)(malloc(sizeof(struct list)));
    schedule_obj._block_list = (struct list*)(malloc(sizeof(struct list)));
    assert(schedule_obj._pending_list != NULL);
    assert(schedule_obj._block_list != NULL);
    list_init(schedule_obj._pending_list);
    list_init(schedule_obj._block_list);
    struct coroutine* co = new_coro();
    assert(co != NULL);
    schedule_obj._daemon = co;
    co->_status = COROUTINE_RUNNING;
    set_running_coro(co);
#ifdef DEBUG_CORO
    printf ("daemon: 0x%p\n", co);
#endif
}

static struct coroutine* current_running_coro()
{
    return schedule_obj._running;
}

static void set_running_coro(struct coroutine*co)
{
#ifdef DEBUG_CORO
    printf ("previous %p -> current running %p\n", schedule_obj._running, co);
#endif
    schedule_obj._running = co;
}

static struct coroutine* new_coro()
{
    struct coroutine* co = (struct coroutine*)(malloc(sizeof(struct coroutine)));
    if (co == NULL)
    {
        return NULL;
    }
    co->_status = COROUTINE_INIT;
    co->_entry = NULL;
    co->_argv = NULL;
    co->_stack_top = NULL;
    co->_stack_addr= NULL;
    co->_stack_sz = 0;
    link_init(&(co->_link));
    memset(&(co->_ctx), 0, sizeof(struct context));
    return co;
}

static bool init_stack(struct coroutine* coro)
{
    assert(coro != NULL);
    if (coro->_stack_addr != NULL)
    {
        return true;
    }
    assert(coro->_stack_top == NULL);
    coro->_stack_addr = malloc(STACK_SIZE);
    if (coro->_stack_addr == NULL)
    {
        return false;
    }
    coro->_stack_sz = STACK_SIZE;
    coro->_stack_top = (void*)((size_t)(coro->_stack_addr)  + coro->_stack_sz);
    return true;
}


void restart_coro(struct coroutine* coro,  coroutine_func func, void* argv)
{
    coro->_entry = func;
    coro->_argv = argv;
    /* if (initial_coro(coro) == false) */
    /* { */
    /*     destroy_coro(coro); */
    /*     return NULL; */
    /* } */
    make_coro_runnable(coro);
}

static void reset_coro(struct coroutine* coro)
{
    coro->_status = COROUTINE_INIT;
    coro->_argv = NULL;
    coro->_entry= NULL;
    init_context(coro);
}

static void schedule_start_coro(struct coroutine* coro)
{
    assert(coro == current_running_coro());
    assert(coro->_status == COROUTINE_RUNNING);

    /* assert() */
    if (coro->_entry)
    {
        coro->_entry(coro->_argv);
    }
    // finish running this coro, but still on this coro stack, we should not destroy the stack.
    reset_coro(coro);
    schedule_coro();
}

static void init_context(struct coroutine* coro)
{
#ifdef DEBUG_CORO
    printf("init_context %p\n", coro);
#endif
    if (save_context(&(coro->_ctx)) != 0)
    {
        /* assert(current_running_coro() == coro); */
        schedule_start_coro(current_running_coro());
    }
    assert(coro->_stack_top != NULL);
    // then we put our created stack into ctx
    if (coro->_stack_top )
    {
        replace_esp(&(coro->_ctx), coro->_stack_top);
    }
}

static void _restore_ctx(struct coroutine* coro)
{
    assert(current_running_coro() == coro);
    restore_context(&(coro->_ctx), 1);
}

static bool initial_coro(struct coroutine* coro)
{
    if (init_stack(coro) == false)
    {
        printf ("init_stack failed\n");
        return false;
    }
    init_context(coro);
    return true;
}

void destroy_coro(struct coroutine* coro)
{
    if (coro == NULL)
    {
        return;
    }
    /* assert(coro->_status == COROUTINE_INIT || ); */
    assert(list_empty(&(coro->_link)) == true);
    if (coro->_stack_addr != NULL)
    {
        free(coro->_stack_addr);
    }
    free(coro);
}
static void make_coro_runnable(struct coroutine* coro)
{
    assert(coro->_status == COROUTINE_INIT ||
           coro->_status == COROUTINE_SUSPEND);
    coro->_status = COROUTINE_READY;
#ifdef DEBUG_CORO
    printf ("make_coro_runnable %p, %p\n", &coro->_link, coro);
#endif
    list_add_tail( &coro->_link, &(schedule_obj._pending_list->head));
}
struct coroutine* create_coro(coroutine_func func, void* argv)
{
    struct coroutine* coro = new_coro();
    coro->_entry = func;
    coro->_argv = argv;
    if (initial_coro(coro) == false)
    {
        destroy_coro(coro);
        return NULL;
    }
    make_coro_runnable(coro);
    return coro;
}

void resume_coro(struct coroutine* coro)
{


    assert(current_running_coro() == schedule_obj._daemon);
    assert(coro->_status == COROUTINE_SUSPEND);
    if (save_context(&(current_running_coro()->_ctx)) == 0)
    {
        (( struct coroutine*)(current_running_coro()))->_status = COROUTINE_READY;
        assert(current_running_coro() != coro);
        set_running_coro(coro);

        coro->_status = COROUTINE_RUNNING;
        _restore_ctx(coro);
    }
    /* printf ("%p %p\n",current_running_coro(),  coro); */
    /* zero_context(&(current_running_coro()->_ctx)); */
    assert(current_running_coro()->_status == COROUTINE_RUNNING);
    /* j */
    /* current_running_coro()->_status = COROUTINE_READY; */
    /* assert(current_running_coro() != coro); */
    /* set_running_coro(coro); */
    /* coro->_status = COROUTINE_RUNNING; */
    /* _restore_ctx(coro); */
}


void schedule_loop()
{

#ifdef DEBUG_CORO
    printf ("in schedule_loop %p\n", current_running_coro());
#endif
    assert(current_running_coro()->_status == COROUTINE_RUNNING);
    assert(current_running_coro()== schedule_obj._daemon);
    schedule_obj._daemon->_status = COROUTINE_READY;
    if (save_context(&(schedule_obj._daemon->_ctx)) == 0)
        schedule_coro();
    return;
}

void schedule_coro()
{
    struct coroutine* coro = NULL;
    if (is_list_empty(schedule_obj._pending_list ))
    {
        coro = schedule_obj._daemon; // daemon is always ready for schedule.
    }
    else
    {
        struct list_head* tmp = list_front(schedule_obj._pending_list);
        assert(tmp != NULL);
        coro = list_entry(tmp, struct coroutine, _link);

#ifdef DEBUG_CORO
        printf ("schedule_coro front %p, %p\n", tmp, coro);
#endif
        list_del(tmp); // remove it
    }
    set_running_coro(coro);
    coro->_status = COROUTINE_RUNNING;
    _restore_ctx(coro);
    return;
}

void yield_coro(void)
{
    struct coroutine* _current = current_running_coro();
    if (save_context(&(_current->_ctx)) == 0)
    {
        _current->_status = COROUTINE_SUSPEND;
        schedule_coro();
    }
    // continue running in _current;
    assert(current_running_coro() == _current);
    assert(current_running_coro()->_status == COROUTINE_RUNNING );
}


void shutdown_coro_env(void)
{
    if (current_running_coro() != schedule_obj._daemon)
    {
        return;
    }
    assert(current_running_coro()->_status == COROUTINE_RUNNING);

    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(schedule_obj._pending_list->head))
    {
        struct coroutine* tmp = list_entry(current, struct coroutine, _link);
        list_del(&(tmp->_link));
        destroy_coro(tmp);
    }
    list_for_each_safe(current, tmp_head, &(schedule_obj._block_list->head))
    {
        struct coroutine* tmp = list_entry(current, struct coroutine, _link);
        list_del(&(tmp->_link));
        destroy_coro(tmp);
    }
    destroy_coro(schedule_obj._daemon);
}

/* static void coro_run(struct coroutine* coro) */
/* { */
/*     assert(coro->_status == COROUTINE_READY); */
/*     if (coro->_entry != NULL) */
/*     { */
/*         coro */
/*     } */
/* } */

