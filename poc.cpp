#pragma leco app
import casein;
import coro;
import movie;
import script;
import thread;

static constexpr const auto image_1 = "image1.png";

using script_task = script::task<step>;

class sthread : public thread {
  movie *m_mov;

  constexpr auto mov() noexcept { return m_mov; }

public:
  script_task inclined_zoom(double ts) {
    auto sts = mov()->timestamp();
    while (mov()->timestamp() < ts) {
      auto time = static_cast<float>(mov()->timestamp() - sts);

      co_yield {
          .data =
              {
                  .movie_angle = 0.6f - time * 0.02f,
                  .movie_scale = 0.5,
              },
      };
    }
  }

  script_task overlay(auto *o, double ts) {
    while (mov()->timestamp() < ts) {
      co_yield {
          .overlay = o,
      };
    }
  }

  script_task wait_until(double ts) {
    while (mov()->timestamp() < ts) {
      co_yield {};
    }
  }
  script_task rewind(double ts) {
    mov()->seek(ts);
    co_yield {};
  }

  script_task scriptum(movie *mov) override {
    m_mov = mov;
    auto img1 = load_image(image_1);

    co_await wait_until(4.0);
    co_await overlay(&img1, 6.0);

    mov->seek(25.5);
    co_await wait_until(33.2);
    co_await inclined_zoom(34.3);
    co_await wait_until(40.0);
  }
};

extern "C" void casein_handle(const casein::event &e) {
  static sthread t{};
  t.handle(e);
}
