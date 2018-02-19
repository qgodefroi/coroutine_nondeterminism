#include "cppcoro/recursive_generator.hpp"

#include <any>
#include <experimental/coroutine>
#include <variant>
#include <vector>

struct choice_base {
    std::any pick;
};

template <class Choices>
struct choice : choice_base {
    Choices* choices;

    using value_type = std::decay_t<decltype(*std::begin(*choices))>;
    choice(Choices&& v) noexcept : choice_base{0}, choices(&v) {}

    bool await_ready() noexcept { return false; }

    template <class U>
    void await_suspend(
        std::experimental::coroutine_handle<U> h) noexcept {
        h.promise().options_or_result =
            [this]() -> cppcoro::generator<std::any> {
            for (auto ch : *choices) {
                co_yield{ch};
            }
        }();
        h.promise().awaiting = this;
    }

    value_type await_resume() noexcept {
        return std::move(std::any_cast<value_type>(pick));
    }
};

template <class Choice>
choice(std::initializer_list<Choice>)
    ->choice<std::initializer_list<Choice>>;

template <class T>
struct nondeterministic {
    using value_type = T;

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

        template <class ReturnedT>
        void return_value(ReturnedT&& v) noexcept {
            options_or_result.template emplace<T>(
                std::forward<ReturnedT>(v));
        }

        template <class Iterable>
        auto await_transform(Iterable&& it) noexcept {
            return choice(std::forward<Iterable>(it));
        }

        std::variant<std::monostate, T, cppcoro::generator<std::any>>
            options_or_result;
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
    void operator()() { coro.resume(); }

    void resume_with(std::any const& pick) {
        // send pick to awaiter
        coro.promise().awaiting->pick = pick;
        // resume
        (*this)();
    }

    T& result() & {
        return std::get<T>(coro.promise().options_or_result);
    }

    T&& result() && {
        return std::move(
            std::get<T>(coro.promise().options_or_result));
    }

    cppcoro::generator<std::any>& options() {
        return std::get<cppcoro::generator<std::any>>(
            coro.promise().options_or_result);
    }
};

template <class Gen>
struct list {
    Gen genfunc;

    using value_type = typename decltype(genfunc())::value_type;

    explicit list(Gen g) noexcept : genfunc(std::move(g)) {}

    cppcoro::recursive_generator<value_type> operator()(
        std::vector<std::any*> chosen_picks = {}) {
        // initiate the wrapped coroutine (get a nondeterministic<T>)
        auto nondet = genfunc();

        // call it until it stops on a choice
        nondet();
        // repeat all previous choices
        for (auto id : chosen_picks) {
            nondet.resume_with(*id);
        }

        // we've reached the end, yield the final result
        if (nondet.coro.done()) {
            co_yield std::move(nondet.result());
        }

        // if the coroutine is stuck on a choice, restart with each
        // possibility
        else {
            for (auto new_pick : nondet.options()) {
                auto new_picks = chosen_picks;
                new_picks.emplace_back(&new_pick);
                co_yield (*this)(new_picks);
            }
        }
    }
};
