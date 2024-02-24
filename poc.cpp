#pragma leco app
#pragma leco add_shader "poc.vert"
#pragma leco add_shader "poc.frag"

import casein;
import decoder;
import mtx;
import rng;
import sith;
import vee;
import voo;

struct inst {
  float x, y;
};

class updater : public voo::update_thread {
  voo::h2l_buffer m_insts;

  void load_instances() {
    voo::mapmem m{m_insts.host_memory()};
    static_cast<inst *>(*m)[0] = {rng::randf(), rng::randf()};
    static_cast<inst *>(*m)[1] = {-1, -1};
  }
  void setup_copy(vee::command_buffer cb) {
    voo::cmd_buf_one_time_submit pcb{cb};
    m_insts.setup_copy(*pcb);
  }

  void build_cmd_buf(vee::command_buffer cb) override {
    load_instances();
    setup_copy(cb);
  }

public:
  explicit updater(voo::device_and_queue *dq)
      : update_thread{dq}
      , m_insts{*dq, 2 * sizeof(inst)} {}

  [[nodiscard]] constexpr auto local_buffer() const noexcept {
    return m_insts.local_buffer();
  }

  using update_thread::run;
};

class thread : public voo::casein_thread {
public:
  void run() override {
    voo::device_and_queue dq{"winnipeg", native_ptr()};

    voo::one_quad quad{dq};

    vee::pipeline_layout pl = vee::create_pipeline_layout();

    updater u{&dq};

    // TODO: fix validation issues while resizing
    while (!interrupted()) {
      voo::swapchain_and_stuff sw{dq};

      // This ensures the thread dies before we leave this loop. This allows
      // release of "updater" resources without any racing with other threads
      sith::memfn_thread<updater> ut{&u, &updater::run};
      ut.start();

      auto gp = vee::create_graphics_pipeline({
          .pipeline_layout = *pl,
          .render_pass = dq.render_pass(),
          .shaders{
              voo::shader("poc.vert.spv").pipeline_vert_stage(),
              voo::shader("poc.frag.spv").pipeline_frag_stage(),
          },
          .bindings{
              quad.vertex_input_bind(),
              vee::vertex_input_bind_per_instance(sizeof(inst)),
          },
          .attributes{
              quad.vertex_attribute(0),
              vee::vertex_attribute_vec2(1, 0),
          },
      });

      extent_loop(dq, sw, [&] {
        sw.queue_one_time_submit(dq, [&](auto pcb) {
          auto scb = sw.cmd_render_pass(pcb);
          vee::cmd_bind_gr_pipeline(*scb, *gp);
          vee::cmd_bind_vertex_buffers(*scb, 1, u.local_buffer());
          quad.run(scb, 0, 2);
        });
      });
    }
  }
};

extern "C" void casein_handle(const casein::event &e) {
  static thread t{};
  t.handle(e);
}
