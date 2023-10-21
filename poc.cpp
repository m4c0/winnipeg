#pragma leco app
import casein;
import coro;
import movie;
import thread;

coro<step_promise> script(movie *mov) {
  for (auto i = 0; i < 3; i++) {
    mov->seek(20.0);
    co_yield {};
    while (mov->timestamp() < 30.0) {
      co_yield {};
    }
  }
}

extern "C" void casein_handle(const casein::event &e) {
  static thread t{script};
  t.handle(e);
}
