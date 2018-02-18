# Nondeterminism with coroutines

This is a POC implementation of an idea I've had in my head for a while: with C++ coroutines' ability to yield to an outer context and get resumed once some concrete data is available, can we use them to model nondeterminism?

If <https://github.com/toby-allsopp/coroutine_monad> is Maybe and Either, this tries to look like List.

## Example

``` cpp
auto test_ints() {
    return list([]() -> nondeterministic<int> {
        bool times2 = co_await choice{true, false};
        int number = co_await choice{1, 2, 3};
        if (times2)
            co_return number * 2;
        else
            co_return number;
    });
}

// later
auto g = test_ints();
for (int t : g()) {
    std::cout << t << '\n';
}
    return 0;
// prints the numbers 2,4,6,1,2,3
```