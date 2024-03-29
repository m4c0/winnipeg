export module movie;
import ffmod;
import player;
import silog;
import sitime;
import vee;
import voo;

static constexpr const auto input_filename = "input.mkv";

class plane_buf {
  vee::buffer m_buf;
  vee::device_memory m_mem;

public:
  plane_buf() = default;
  plane_buf(vee::physical_device pd, int sz) {
    m_buf = vee::create_transfer_src_buffer(sz);
    m_mem = vee::create_host_buffer_memory(pd, *m_buf);
    vee::bind_buffer_memory(*m_buf, *m_mem);
  }

  [[nodiscard]] auto operator*() { return *m_buf; }
  [[nodiscard]] auto map() { return voo::mapmem(*m_mem); }
};

export class movie {
  player m_player;
  player_promise::coro m_coro;
  sitime::stopwatch m_watch{};
  unsigned m_seek{};
  bool m_prev_paused{false};

  vee::extent m_ext;

  vee::sampler_ycbcr_conversion m_smp_conv;

  plane_buf m_buf_y;
  plane_buf m_buf_u;
  plane_buf m_buf_v;

  vee::image m_img;
  vee::device_memory m_mem;
  vee::image_view m_iv;

public:
  movie(vee::physical_device pd)
      : m_player{input_filename}, m_coro{m_player.play()} {
    auto w = m_player.width();
    auto h = m_player.height();
    m_ext = {static_cast<unsigned>(w), static_cast<unsigned>(h)};

    m_smp_conv = vee::create_sampler_yuv420p_conversion(pd);

    m_buf_y = plane_buf{pd, w * h};
    m_buf_u = plane_buf{pd, w * h / 4};
    m_buf_v = plane_buf{pd, w * h / 4};

    m_img = vee::create_yuv420p_image(m_ext);
    m_mem = vee::create_local_image_memory(pd, *m_img);
    vee::bind_image_memory(*m_img, *m_mem);
    m_iv = vee::create_yuv420p_image_view(*m_img, *m_smp_conv);

    if (m_coro.promise().failed)
      return;
    silog::log(silog::info, "Video size: %dx%d", m_ext.width, m_ext.height);
  }

  void seek(double ts) {
    m_player.seek(ts);
    m_seek = static_cast<int>(ts * 1000.0);
    m_watch = {};
  }

  [[nodiscard]] auto conv() const noexcept { return *m_smp_conv; }
  [[nodiscard]] auto iv() const noexcept { return *m_iv; }

  [[nodiscard]] auto timestamp() const noexcept { return m_player.timestamp(); }

  void pause(bool paused) {
    if (paused && !m_prev_paused) {
      m_seek += m_watch.millis();
      m_prev_paused = true;
    } else if (!paused && m_prev_paused) {
      m_watch = {};
      m_prev_paused = false;
    }
  }
  void run(vee::command_buffer cb) {
    if (m_prev_paused)
      return;

    if (m_coro.done() || m_coro.promise().failed)
      return;

    m_coro.resume();
    if (m_coro.done() || m_coro.promise().failed)
      return;

    auto frm = m_coro.promise().value;

    silog::log(silog::debug, "Movie at: %f", m_player.timestamp());
    auto pts = static_cast<int>(m_player.timestamp() * 1000.0);
    auto mts = m_watch.millis() + m_seek;
    if (pts > mts) {
      // TODO: fix after seeking
      sitime::sleep_ms(pts - mts);
    }

    // TODO: assert 4:2:0
    // TODO: assert linesize > ext.w

    auto y = m_buf_y.map();
    auto *yy = static_cast<unsigned char *>(*y);
    auto u = m_buf_u.map();
    auto *uu = static_cast<unsigned char *>(*u);
    auto v = m_buf_v.map();
    auto *vv = static_cast<unsigned char *>(*v);
    ffmod::copy_frame_yuv(*frm, yy, uu, vv);

    vee::cmd_pipeline_barrier(cb, *m_img, vee::from_host_to_transfer);
    vee::cmd_copy_yuv420p_buffers_to_image(cb, m_ext, *m_buf_y, *m_buf_u,
                                           *m_buf_v, *m_img);
    vee::cmd_pipeline_barrier(cb, *m_img, vee::from_transfer_to_fragment);
  }
};
