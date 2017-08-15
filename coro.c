#include "coro.h"
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


int bootstrap_coro_env()
{
    schedule_obj._pending_list = (struct list*)(malloc(sizeof(struct list)));
    assert(schedule_obj._pending_list != NULL);
    list_init(schedule_obj._pending_list);
    return 0;
}

static struct coroutine* current_running_coro()
{
    return schedule_obj._running;
}

static struct coroutine* new_coro()
{
    struct coroutine* co = (struct coroutine*)(malloc(sizeof(struct coroutine)));
    if (co == NULL)
    {
        return NULL;
    }
    co->_status = COROUTINE_DEAD;
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

static void init_context(struct coroutine* coro)
{
    if (save_context(&(coro->_ctx)) != 0)
    {
        // TODO schedule myself.
        assert(current_running_coro() == coro);
        start_running_coro();
    }
    assert(coro->_stack_top != NULL);
    // then we put our created stack into ctx
    if (coro->_stack_top )
        replace_esp(&(coro->_ctx), coro->_stack_top);
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
    assert(coro->_status == COROUTINE_DEAD);
    assert(list_empty(&(coro->_link)) == true);
    free(coro->_stack_addr);
    free(coro);
}
static void make_coro_runnable(struct coroutine* coro)
{
    assert(coro->_status == COROUTINE_DEAD ||
           coro->_status == COROUTINE_SUSPEND);
    coro->_status = COROUTINE_READY;
    list_add_tail(&(schedule_obj._pending_list->head), &coro->_link);
}
struct coroutine* create_coro(coroutine_func func, void* argv)
{
    struct coroutine* coro = new_coro();
    coro->_entry = func;
    coro->_argv = argv;
    if (initial_coro(coro) == false)
    {
        destroy_coro(coro);
        return  NULL;
    }
    make_coro_runnable(coro);
    return coro;
}
static void set_running_coro(struct coroutine* coro)
{
    schedule_obj._running = coro;
}

void resume_coro(struct coroutine* coro)
{
    struct coroutine* _current = current_running_coro();
    assert(_current != coro);
    set_running_coro(coro);
    _restore_ctx(coro);
}

static void schedule_coro()
{
    if (is_list_empty(schedule_obj._pending_list ))
    {

    }
    else
    {


    }
    //TODO
    return;
}

void yield_coro(void)
{
    struct coroutine* _current = current_running_coro();
    if (save_context(&(_current->_ctx)) == 0)
    {
        schedule_coro();
    }
    // continue running in _current;
    assert(current_running_coro() == _current);
}

/* static void coro_run(struct coroutine* coro) */
/* { */
/*     assert(coro->_status == COROUTINE_READY); */
/*     if (coro->_entry != NULL) */
/*     { */
/*         coro */
/*     } */
/* } */

