#pragma leco app
import casein;
import coro;
import movie;
import script;
import thread;

using script_task = script::task<step>;

script_task wait_until(movie *mov, double ts) {
  while (mov->timestamp() < ts) {
    co_yield {};
  }
}
script_task rewind(movie *mov, double ts) {
  mov->seek(ts);
  co_yield {};
}

script_task scr(movie *mov) {
  for (auto i = 0; i < 3; i++) {
    co_await rewind(mov, 20.0);
    co_await wait_until(mov, 30.0);
  }
}

extern "C" void casein_handle(const casein::event &e) {
  static thread t{&scr};
  t.handle(e);
}
