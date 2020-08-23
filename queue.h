#pragma once

struct QueueNode
{
    QueueNode* next;
    QueueNode* prev;

    void insert_back(QueueNode *node) noexcept
    {
        node->prev = this;
        node->next = this->next;
        this->next->prev = node;
        this->next = node;
    }

    void insert_front(QueueNode *node) noexcept
    {
        node->prev = this->prev;
        node->next = this;
        this->prev->next = node;
        this->prev = node;
    }

    void del() noexcept
    {
        this->next->prev = this->prev;
        this->prev->next = this->next;
        this->next = nullptr;
        this->prev = nullptr;
    }

    template<typename T, QueueNode(T::* POS)>
    T* get() noexcept
    {
        union
        {
            QueueNode(T::* pos);
            size_t offset = 0;
        };

        pos = POS;

        return reinterpret_cast<T *>(reinterpret_cast<uint8_t*>(this) - offset);
    }

    template<typename T, QueueNode(T::* POS)>
    const T* get() const noexcept { return const_cast<QueueNode*>(this)->get(); }
};

struct QueueSentry : QueueNode
{
    QueueSentry() noexcept
    {
        clear();
    }

    void clear() noexcept { this->next = this; this->prev = this; }
    bool empty() const noexcept { return this == this->next; }

    QueueNode* begin() noexcept { return this->next; }
    const QueueNode* begin() const noexcept { return this->next; }

    QueueNode* end() noexcept { return this; }
    const QueueNode* end() const noexcept { return this; }

    QueueNode* front() noexcept { return this->next; }
    const QueueNode* front() const noexcept { return this->next; }

    QueueNode* back() noexcept { return this->prev; }
    const QueueNode* back() const noexcept { return this->prev; }

    void push_back(QueueNode* node) noexcept { insert_back(node); }
    void push_front(QueueNode* node) noexcept { insert_front(node); }
};

template<typename T, QueueNode (T::*POS)>
class Queue
{
public:
    bool empty() const noexcept { return _sentry.empty(); }

    T* front() noexcept { return _sentry.front()->get<T, POS>(); }
    const T* front() const noexcept { return _sentry.front()->get<T, POS>(); }

    T* back() noexcept { return  _sentry.back()->get<T, POS>(); }
    const T* back() const noexcept { _sentry.back()->get<T, POS>(); }

    void push_front(T* ptr) noexcept { _sentry.push_front(&(ptr->*POS)); }
    void push_back(T* ptr) noexcept { _sentry.push_back(&(ptr->*POS)); }

    T* pop_front() noexcept
    {
        auto ptr = empty() ? nullptr : front();
        if (ptr)
        {
            (ptr->*POS).del();
        }

        return ptr;
    }

    T* pop_back() noexcept
    {
        auto ptr = empty() ? nullptr : back();
        if (ptr)
        {
            (ptr->*POS).del();
        }

        return ptr;
    }

    template<typename Fn>
    void foreach(Fn f)
    {
        auto node = _sentry.begin();
        auto end = _sentry.end();
        while (end != node)
        {
            f(node->get<T, POS>());

            node = node->next;
        }
    }
private:
    QueueSentry _sentry;
};

