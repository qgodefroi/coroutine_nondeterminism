# Nondeterminism with coroutines

This is a POC implementation of an idea I've had in my head for a while: with C++ coroutines' ability to yield to an outer context and get resumed once some concrete data is available, can we use them to model nondeterminism?

If [https://github.com/toby-allsopp/coroutine_monad] is Maybe and Either, this would sort of be List. As things are, we're pretty far away from a full monadic construct, but it'll get there.

## Example

``` cpp
nondet<int> test_ints_impl() {
    bool times2 = co_await choice{true, false};
    int number = co_await choice{1, 2, 3};
    if (times2)
        co_return number * 2;
    else
        co_return number;
}
auto test_ints() { return list{test_ints_impl}; }

// later
auto g = test_ints();
for (int t : g()) {
    std::cout << t << '\n';
}
// prints the list 2,4,6,1,2,3
```