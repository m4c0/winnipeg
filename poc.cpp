#pragma leco app
import coro;
import movie;
import thread;

struct s_step {
  // fx parameters, etc
};

struct s_promise {
  using coro = ::coro<s_promise>;

  s_step value;

  coro get_return_object() { return coro::from_promise(*this); }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  std::suspend_always yield_value(s_step &&s) {
    value = s;
    return {};
  }
  void unhandled_exception() {}
};

struct script {
  movie *mov;

  coro<s_promise> run() {
    mov->seek(70.0);
    while (mov->timestamp() < 72.0) {
      co_yield {};
    }
    mov->seek(120.0);
    while (mov->timestamp() < 200.0) {
      co_yield {};
    }
  }
};
