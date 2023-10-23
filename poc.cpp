#pragma leco app
import casein;
import coro;
import movie;
import script;
import thread;

using script_task = script::task<step>;

script_task inclined_zoom(movie *mov, double ts) {
  auto sts = mov->timestamp();
  while (mov->timestamp() < ts) {
    co_yield {
        .movie_angle =
            static_cast<float>(0.6 - (mov->timestamp() - sts) * 0.02),
    };
  }
}

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
  mov->seek(25.5);
  co_await wait_until(mov, 33.2);
  co_await inclined_zoom(mov, 34.3);
  co_await wait_until(mov, 40.0);
}

extern "C" void casein_handle(const casein::event &e) {
  static thread t{&scr};
  t.handle(e);
}
