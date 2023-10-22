export module script;
import coro;

namespace script {
template <typename Tp> struct promise_type;

export template <typename Tp> class task {
  using corohpt = std::coroutine_handle<promise_type<Tp>>;

  corohpt h;
  bool empty;

  bool advance(corohpt who) {
    auto next = who.promise().next();
    if (!next || next.done() || advance(next)) {
      who.resume();
      return who.done();
    }
    return false;
  }

public:
  using promise_type = script::promise_type<Tp>;

  task(corohpt h) : h{h} {}
  task(task &&t) = delete;
  task(const task &t) = delete;
  task &operator=(task &&t) = delete;
  task &operator=(const task &t) = delete;
  ~task() { h.destroy(); }

  auto operator co_await() {
    struct awaiter {
      corohpt self;

      bool await_ready() { return false; }
      auto await_suspend(corohpt h) {
        h.promise().next = self;
        return self;
      }
      void await_resume() {}
    };
    return awaiter{h};
  }

  bool done() {
    fill();
    return h.done();
  }
  Tp next() {
    fill();
    empty = true;

    auto who = h;
    while (auto next = who.promise().next()) {
      if (next.done()) {
        break;
      }
      who = next;
    }

    return who.promise().value;
  }

  void fill() {
    if (!empty)
      return;
    advance(h);
    empty = false;
  }
};
template <typename Tp> struct promise_type {
  std::coroutine_handle<promise_type<Tp>> next{};
  Tp value{};

  task<Tp> get_return_object() {
    return {std::coroutine_handle<promise_type>::from_promise(*this)};
  }
  std::suspend_always initial_suspend() { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  std::suspend_always yield_value(int i) {
    value = i;
    return {};
  }
  void return_void() {}
  void unhandled_exception() {}
};
} // namespace script
