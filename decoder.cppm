export module decoder;
import ffmod;
import hai;
import silog;
import sith;
import vee;
import voo;

export class decoder : public voo::update_thread {
  ffmod::fmt_ctx m_ctx;
  hai::array<ffmod::codec_ctx> m_decs;
  voo::h2l_yuv_image m_img;
  ffmod::packet pkt = ffmod::av_packet_alloc();
  ffmod::frame frm = ffmod::av_frame_alloc();

  void build_cmd_buf(vee::command_buffer cb) override {
    auto pkt_ref = ffmod::av_read_frame(m_ctx, pkt);
    if (!*pkt_ref)
      return;

    auto idx = (*pkt)->stream_index;
    auto &dec = m_decs[idx];

    ffmod::avcodec_send_packet(dec, pkt);
    while (!interrupted()) {
      auto frm_ref = ffmod::avcodec_receive_frame(dec, frm);
      if (!*frm_ref)
        break;

      if ((*dec)->codec_type == AVMEDIA_TYPE_VIDEO) {
        voo::mapmem y{m_img.host_memory_y()};
        voo::mapmem u{m_img.host_memory_u()};
        voo::mapmem v{m_img.host_memory_v()};

        ffmod::copy_frame_yuv(frm, static_cast<unsigned char *>(*y),
                              static_cast<unsigned char *>(*u),
                              static_cast<unsigned char *>(*v));
      } else if ((*dec)->codec_type == AVMEDIA_TYPE_AUDIO) {
        // auto num_samples = (*frm)->nb_samples;
        // auto sample_rate = (*frm)->sample_rate;
        // auto *data = reinterpret_cast<float *>((*frm)->extended_data[0]);
      }
    }

    // This works without validation issues because each video packet
    // contains only a single frame
    voo::cmd_buf_one_time_submit pcb{cb};
    m_img.setup_copy(cb);
  }

public:
  explicit decoder(voo::device_and_queue *dq, const char *filename)
      : update_thread{dq}
      , m_ctx{ffmod::avformat_open_input(filename)}
      , m_decs{ffmod::avcodec_open_all(m_ctx)}
      , m_img{*dq, static_cast<unsigned>((*m_decs[0])->width),
              static_cast<unsigned>((*m_decs[0])->height)} {
    silog::assert((*m_decs[0])->codec_type == AVMEDIA_TYPE_VIDEO,
                  "invalid movie format");
    silog::assert((*m_decs[1])->codec_type == AVMEDIA_TYPE_AUDIO,
                  "invalid movie format");

    // decode a frame earlier to avoid using an unprepared image
    run_once();
  }

  [[nodiscard]] constexpr auto conv() const noexcept { return m_img.conv(); }
  [[nodiscard]] constexpr auto iv() const noexcept { return m_img.iv(); }
};
