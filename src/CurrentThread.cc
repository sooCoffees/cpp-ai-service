#include <CurrentThread.h>
#ifdef __APPLE__
#include <pthread.h>
#endif

namespace CurrentThread
{
      thread_local int t_cachedTid = 0; // 在源文件中定义线程局部变量
    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
#ifdef __APPLE__
            uint64_t tid = 0;
            pthread_threadid_np(nullptr, &tid);
            t_cachedTid = static_cast<pid_t>(tid);
#else
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid)); // Ensure syscall and SYS_gettid are defined
#endif
        }
    }
}
