export module script;
import coro;

namespace script {
struct promise_type;
using corohpt = std::coroutine_handle<promise_type>;
void set_next(corohpt, corohpt);
corohpt get_next(corohpt);
int get_value(corohpt);
export class task {
  corohpt h;
  bool empty;

  bool advance(corohpt who) {
    auto next = get_next(who);
    if (!next || next.done() || advance(next)) {
      who.resume();
      return who.done();
    }
    return false;
  }

public:
  task(corohpt h) : h{h} {
    // printf("%p ctor %p\n", this, &h.promise());
  }
  task(task &&t) = delete;
  task(const task &t) = delete;
  task &operator=(task &&t) = delete;
  task &operator=(const task &t) = delete;
  ~task() {
    // printf("%p dtor %p\n", this, &h.promise());
    h.destroy();
  }

  auto operator co_await() {
    struct awaiter {
      corohpt self;

      bool await_ready() { return false; }
      auto await_suspend(corohpt h) {
        // printf("sus %p %p\n", &h.promise(), &self.promise());
        set_next(h, self);
        return self;
      }
      void await_resume() {
        // printf("res %p\n", &self.promise());
      }
    };
    // printf("%p await %p\n", this, &h.promise());
    return awaiter{h};
  }

  bool done() {
    fill();
    return h.done();
  }
  int next() {
    fill();
    empty = true;

    auto who = h;
    while (auto next = get_next(who)) {
      if (next.done()) {
        break;
      }
      who = next;
    }

    return get_value(who);
  }

  void fill() {
    if (!empty)
      return;
    advance(h);
    empty = false;
  }

  using promise_type = script::promise_type;
};
struct promise_type {
  std::coroutine_handle<promise_type> next{};
  int value = -1;

  task get_return_object() {
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
corohpt get_next(corohpt h) { return h.promise().next; }
void set_next(corohpt a, corohpt b) { a.promise().next = b; }
int get_value(corohpt h) { return h.promise().value; }
} // namespace script
