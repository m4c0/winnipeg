#pragma leco app
import casein;
import coro;
import movie;
import thread;

coro<step_promise> script(movie *mov) {
  mov->seek(70.0);
  while (mov->timestamp() < 72.0) {
    co_yield {};
  }
  mov->seek(120.0);
  while (mov->timestamp() < 123.0) {
    co_yield {};
  }
  mov->seek(20.0);
  co_yield {};
  while (mov->timestamp() < 30.0) {
    co_yield {};
  }
}

extern "C" void casein_handle(const casein::event &e) {
  static thread t{script};
  t.handle(e);
}
