export module coro;

export namespace std {
template <typename Tp, typename...> struct coroutine_traits {
  using promise_type = Tp::promise_type;
};
template <typename Tp = void> class coroutine_handle;
template <> class coroutine_handle<> {
  void *addr;

  template <typename> friend class coroutine_handle;

  constexpr coroutine_handle(void *addr) : addr{addr} {}

public:
  static constexpr coroutine_handle from_address(void *addr) { return {addr}; };

  bool done() const { return __builtin_coro_done(addr); }
  void destroy() const { return __builtin_coro_destroy(addr); }
  void resume() const { return __builtin_coro_resume(addr); }
};
template <typename Tp> class coroutine_handle : public coroutine_handle<> {
  using coroutine_handle<>::coroutine_handle;

public:
  static constexpr coroutine_handle from_promise(Tp &p) {
    return {__builtin_coro_promise(&p, alignof(Tp), true)};
  };
  // constexpr operator coroutine_handle<>() const noexcept { return {addr}; }

  Tp &promise() const {
    return *static_cast<Tp *>(__builtin_coro_promise(addr, alignof(Tp), false));
  }
};

struct suspend_always {
  constexpr bool await_ready() const noexcept { return false; }
  constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
  constexpr void await_resume() const noexcept {}
};
struct suspend_never {
  constexpr bool await_ready() const noexcept { return true; }
  constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
  constexpr void await_resume() const noexcept {}
};
} // namespace std