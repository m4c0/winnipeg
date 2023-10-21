export module audio;
import hai;
import siaudio;

export class audio : public siaudio::os_streamer {
  static constexpr const auto buf_len = 441000;
  hai::array<float> m_cbuf{buf_len};
  volatile unsigned m_rp = 0;
  volatile unsigned m_wp = 0;

public:
  void fill_buffer(float *f, unsigned num_samples) override {
    while (num_samples > 0 && m_rp != m_wp) {
      *f++ = m_cbuf[m_rp];
      m_rp = (m_rp + 1) % buf_len;
      num_samples--;
    }
  }

  void push_frame(float *f, unsigned num_samples) {
    while (num_samples > 0 && m_rp != m_wp + 1) {
      m_cbuf[m_wp] = *f++;
      m_wp = (m_wp + 1) % buf_len;
      num_samples--;
    }
  }

  void flush() {
    m_rp = 0;
    m_wp = 0;
    for (auto &f : m_cbuf)
      f = 0;
  }
};
