# Nondeterminism with coroutines

This is a POC implementation of an idea I've had in my head for a while: with C++ coroutines' ability to yield to an outer context and get resumed once some concrete data is available, can we use them to model nondeterminism?

If <https://github.com/toby-allsopp/coroutine_monad> is Maybe and Either, this tries to look like List.

This is very experimental code, and still very much a work in progress, but it seems worth sharing.

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
// prints the numbers 2,4,6,1,2,3
```

## Usage

This repository should contain everything needed, included generator types taken from <https://github.com/lewissbaker/cppcoro>. I have been testing this with clang 5, c++17, libc++ and -fcoroutines-ts.

I'll add build scripts and more examples at some point.

## Work in progress

- remove some obvious ineficiencies (the underlying coroutine is restarted more times than truly necessary)
- roll more functionality into the list interface, and make nesting them cleaner
- move closer to proper monad laws and concepts for (hopefully!) a cleaner abstraction