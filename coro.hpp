#include "cppcoro/recursive_generator.hpp"

#include <experimental/coroutine>
#include <vector>

struct choice_base {
    std::size_t pick;
};

template <class Choices>
struct choice : choice_base {
    Choices* choices;

    choice(Choices&& v) noexcept : choice_base{0}, choices(&v) {}

    bool await_ready() noexcept { return false; }

    template <class U>
    void await_suspend(
        std::experimental::coroutine_handle<U> h) noexcept {
        h.promise().options.clear();
        for (std::size_t i = 0; i < choices->size(); ++i) {
            h.promise().options.emplace_back(i);
        }
        h.promise().awaiting = this;
    }

    auto await_resume() noexcept {
        auto begin = std::begin(*choices);
        while (pick) {
            ++begin;
            --pick;
        }
        return *begin;
    }
};

template <class Choice>
choice(std::initializer_list<Choice>)
    ->choice<std::initializer_list<Choice>>;

template <class T>
struct nondeterministic {
    struct promise_type {
        promise_type() noexcept = default;
        promise_type(promise_type const&) = delete;
        promise_type& operator=(promise_type const&) = delete;

        auto get_return_object() noexcept {
            return nondeterministic{*this};
        }

        std::experimental::suspend_always initial_suspend() noexcept {
            return {};
        }

        std::experimental::suspend_always final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() noexcept { std::terminate(); }

        void return_value(T&& v) noexcept { return_value(v); }
        void return_value(T& v) noexcept { value = &v; }

        T* value = nullptr;
        std::vector<std::size_t> options = {};
        choice_base* awaiting = nullptr;
    };

    // constructors/destructor
    nondeterministic(promise_type& p) noexcept
        : coro(handle_type::from_promise(p)) {}
    nondeterministic(nondeterministic const& other) = delete;
    nondeterministic& operator=(nondeterministic const& other) =
        delete;
    nondeterministic(nondeterministic&& other) noexcept
        : coro(std::exchange(other.coro, nullptr)) {}
    nondeterministic& operator=(nondeterministic&& other) noexcept {
        destroy();
        coro = std::exchange(other.coro, nullptr);
    }
    void destroy() noexcept {
        if (coro) {
            coro.destroy();
            coro = nullptr;
        }
    }
    ~nondeterministic() { destroy(); }

    // data
    using handle_type =
        std::experimental::coroutine_handle<promise_type>;
    handle_type coro;

    // coroutine methods
    auto const& operator()() {
        coro.resume();
        return coro.promise().options;
    }

    auto const& send(std::size_t pick) {
        // send pick to awaiter
        // resume
        coro.promise().awaiting->pick = pick;
        return (*this)();
    }
};

template <class Gen>
struct list {
    Gen genfunc;

    template <class T>
    struct get_return_type {};
    template <class T>
    struct get_return_type<nondeterministic<T>> {
        using type = T;
    };
    using coro_return_type =
        typename get_return_type<decltype(genfunc())>::type;
    using value_type = coro_return_type;
    list(Gen g) noexcept : genfunc(std::move(g)) {}

    cppcoro::recursive_generator<coro_return_type> operator()(
        std::vector<std::size_t> chosen_indices = {}) {
        // initiate the wrapped coroutine (get a nondeterministic<T>)
        auto g = genfunc();

        // call it until it stops on a choice
        std::vector<std::size_t> possible_choices = g();
        // repeat all previous choices
        for (auto id : chosen_indices) {
            possible_choices = g.send(id);
        }
        // we've reached the end, yield the final result
        if (g.coro.done()) {
            co_yield std::move(*g.coro.promise().value);
        }
        // the coroutine is stuck on a choice, restart with each
        // possibility
        else {
            for (auto newid : possible_choices) {
                auto new_indices = chosen_indices;
                new_indices.emplace_back(newid);
                co_yield (*this)(new_indices);
            }
        }
    }
};
