#include <iostream>
#include "coro.hpp"

cppcoro::generator<bool> test_bools() {
    co_yield true;
    co_yield false;
}

auto test_ints() {
    return list([]() -> nondeterministic<int> {
        bool times2 = co_await choice(test_bools());
        int number = co_await choice{1, 2, 3};
        if (times2)
            co_return number * 2;
        else
            co_return number;
    });
}

int main() {
    auto g = test_ints();
    for (int t : g()) {
        std::cout << t << '\n';
    }
    return 0;
}
