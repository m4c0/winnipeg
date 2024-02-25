#pragma leco app
#pragma leco add_shader "poc.vert"
#pragma leco add_shader "poc.frag"

import casein;
import decoder;
import sith;
import vee;
import voo;

static constexpr const auto filename = "movie.mov";

class thread : public voo::casein_thread {
public:
  void run() override {
    voo::device_and_queue dq{"winnipeg", native_ptr()};

    decoder dec{&dq, filename};
    vee::sampler yuv_smp =
        vee::create_yuv_sampler(vee::linear_sampler, dec.conv());

    voo::one_quad quad{dq};

    vee::descriptor_set_layout dsl = vee::create_descriptor_set_layout(
        {vee::dsl_fragment_samplers({*yuv_smp})});
    vee::descriptor_pool dp =
        vee::create_descriptor_pool(1, {vee::combined_image_sampler(2)});
    vee::descriptor_set dset = vee::allocate_descriptor_set(*dp, *dsl);
    vee::update_descriptor_set(dset, 0, dec.iv());

    vee::pipeline_layout pl = vee::create_pipeline_layout({*dsl});

    while (!interrupted()) {
      voo::swapchain_and_stuff sw{dq};

      auto gp = vee::create_graphics_pipeline({
          .pipeline_layout = *pl,
          .render_pass = dq.render_pass(),
          .shaders{
              voo::shader("poc.vert.spv").pipeline_vert_stage(),
              voo::shader("poc.frag.spv").pipeline_frag_stage(),
          },
          .bindings{
              quad.vertex_input_bind(),
          },
          .attributes{
              quad.vertex_attribute(0),
          },
      });

      sith::run_guard tt{&dec};

      extent_loop(dq, sw, [&] {
        sw.queue_one_time_submit(dq, [&](auto pcb) {
          auto scb = sw.cmd_render_pass(pcb);
          vee::cmd_bind_gr_pipeline(*scb, *gp);
          vee::cmd_bind_descriptor_set(*scb, *pl, 0, dset);
          quad.run(scb, 0);
        });
      });
    }
  }
};

extern "C" void casein_handle(const casein::event &e) {
  static thread t{};
  t.handle(e);
}
