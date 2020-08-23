
#include <wtypes.h>
#include <functional>
#include <iostream>
#include <cassert>

#include "uv.h"

#include "queue.h"


struct Scheduler;

void fiber_entry(void* param);

struct Fiber
{
    QueueNode q;
    LPVOID id = nullptr;
    std::function<void()> entry;
    Scheduler* sched = nullptr;

    bool init()
    {
        this->id = ::ConvertThreadToFiber(nullptr);
        return ok();
    }

    bool create(std::function<void()> f)
    {
        this->id = ::CreateFiber(0, fiber_entry, this);
        if (!ok())
        {
            return false;
        }

        this->entry = f;

        return true;
    }

    bool ok() const { return !!this->id; }

    void resume()
    {
        ::SwitchToFiber(id);
    }
};

struct UVParams
{
    Scheduler* sched;
    Fiber* cur;
    int status;
};

struct Scheduler
{
    Fiber main;
    Queue<Fiber, &Fiber::q> rqueue;
    Fiber* cur = nullptr;
    uv_loop_t uv_loop;
    uv_idle_t uv_idle;

    Scheduler()
    {
        main.init();
        uv_loop_init(&uv_loop);
        uv_idle_init(&uv_loop, &uv_idle);

        uv_idle.data = this;
        uv_idle_start(&uv_idle, [](uv_idle_t* h) {
            auto sched = static_cast<Scheduler*>(h->data);
            sched->sched(true);
        });
    }

    ~Scheduler()
    {
        uv_idle_stop(&uv_idle);
        close((uv_handle_t*)&uv_idle);
        uv_loop_close(&uv_loop);
        assert(rqueue.empty());
    }

    template<typename F>
    bool create_fiber(F&& f)
    {
        auto r = std::make_unique<Fiber>();
        if (!r->create(f))
        {
            return false;
        }

        r->sched = this;

        rqueue.push_back(r.release());

        return true;
    }

    int timer_start(uv_timer_t *h, uint64_t timeout, uint64_t repeat)
    {
        UVParams params = { this, cur };
        h->data = &params;

        int ret = uv_timer_start(
            h,
            [](uv_timer_t* h) {
                auto tp = static_cast<UVParams*>(h->data);

                if (!tp->cur->q.next)
                {
                    tp->sched->rqueue.push_back(tp->cur);
                }
            },
            timeout,
            repeat
        );

        sched(true);

        return ret;
    }

    void close(uv_handle_t* h)
    {
        UVParams params = { this, cur };
        h->data = &params;

        uv_close(
            h,
            [](uv_handle_t* h) {
                auto tp = static_cast<UVParams*>(h->data);

                if (!tp->cur->q.next)
                {
                    tp->sched->rqueue.push_back(tp->cur);
                }
            }
        );

        sched(true);
    }

    void sleep(uint64_t ms)
    {
        uv_timer_t h;
        uv_timer_init(&uv_loop, &h);
        timer_start(&h, ms, 0);
        close((uv_handle_t*)&h);
    }

    void sched(bool last = false)
    {
        auto p = rqueue.pop_front();
        if (!p)
        {
            p = &main;
        }

        if (p == cur)
        {
            return;
        }

        if (cur && !last)
        {
            rqueue.push_back(cur);
        }
   
        std::clog << "switch fiber " << cur << " to " << p << std::endl;
        cur = p;
        p->resume();
    }
};

void fiber_entry(void* param)
{
    auto p = static_cast<Fiber*>(param);
    p->entry();

    auto sched = p->sched;
    delete p;
    std::clog << "release fiber " << p << std::endl;
    sched->sched(true);
}


int main()
{
    Scheduler sched;

    sched.create_fiber([&] {
        uv_tcp_t server;

        uv_tcp_init(&sched.uv_loop, &server);

        sockaddr_in local;
        uv_ip4_addr("127.0.0.1", 6789, &local);

        uv_tcp_bind(&server, (const sockaddr*)&local, 0);

        UVParams params = { &sched, sched.cur, 0 };
        server.data = &params;

        uv_listen((uv_stream_t*)&server, 1024, [] (uv_stream_t* server, int status) {
            auto tp = static_cast<UVParams*>(server->data);

            if (!tp->cur->q.next)
            {
                tp->sched->rqueue.push_back(tp->cur);
            }

            tp->status = status;
        });

        while (0 == params.status)
        {
            sched.sched(true);

            uv_tcp_t client;
            uv_tcp_init(&sched.uv_loop, &client);

            uv_accept((uv_stream_t*)&server, (uv_stream_t*)&client);

            sched.create_fiber([&sched, client] {



                sched.close((uv_handle_t *)&client);
            });
        }
    });

    sched.create_fiber([&] {
        sched.sleep(1000);

        uv_tcp_t client;

        uv_tcp_init(&sched.uv_loop, &client);

        sockaddr_in remote;
        uv_ip4_addr("127.0.0.1", 6789, &remote);

        uv_connect_t req;

        UVParams params = { &sched, sched.cur, 0 };
        req.data = &params;

        uv_tcp_connect(
            &req,
            &client,
            (const sockaddr*)&remote,
            [](uv_connect_t* req, int status) {
                auto tp = static_cast<UVParams*>(req->data);

                if (!tp->cur->q.next)
                {
                    tp->sched->rqueue.push_back(tp->cur);
                }

                tp->status = status;
            }
        );

        sched.sched(true);

        //uv_write()
    });

    sched.create_fiber([&] {
        sched.sleep(1000);

        uv_tcp_t client;

        uv_tcp_init(&sched.uv_loop, &client);

        sockaddr_in remote;
        uv_ip4_addr("127.0.0.1", 6789, &remote);

        uv_connect_t req;

        UVParams params = { &sched, sched.cur, 0 };
        req.data = &params;

        uv_tcp_connect(
            &req,
            &client,
            (const sockaddr*)&remote,
            [](uv_connect_t* req, int status) {
            auto tp = static_cast<UVParams*>(req->data);

            if (!tp->cur->q.next)
            {
                tp->sched->rqueue.push_back(tp->cur);
            }

            tp->status = status;
        }
        );

        sched.sched(true);

        //uv_write()
    });

    uv_run(&sched.uv_loop, UV_RUN_DEFAULT);
}
