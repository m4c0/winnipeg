export module thread;
#pragma leco add_shader "winnipeg.vert"
#pragma leco add_shader "winnipeg.frag"
import casein;
import coro;
import movie;
import script;
import silog;
import vee;
import voo;

struct step_data {
  float angle{};
  float scale{1.0};
  float pos_x{};
  float pos_y{};
};
export struct step {
  step_data data{};
  voo::sires_image *overlay;
};
struct upc {
  float aspect;
  step_data s;
};

export class thread : public voo::casein_thread {
  vee::physical_device m_pd{};

  volatile bool m_paused{};
  volatile bool m_step{};

protected:
  virtual script::task<step> scriptum(movie *) = 0;

  auto load_image(const char *file) { return voo::sires_image(file, m_pd); }

public:
  void run() override;

  void key_down(const casein::events::key_down &e) override {
    switch (*e) {
    case casein::K_SPACE:
      m_paused = !m_paused;
      break;
    case casein::K_Q:
      m_step = true;
      break;
    default:
      break;
    }
  }
};

void thread::run() {
  voo::device_and_queue dq{"winnipeg", native_ptr()};
  auto cp = dq.command_pool();
  auto pd = dq.physical_device();
  auto q = dq.queue();
  auto s = dq.surface();
  m_pd = pd;

  voo::one_quad quad{pd};

  // Command pool + buffer
  vee::command_buffer cb = vee::allocate_primary_command_buffer(cp);

  // Wrapped stuff
  movie mov{pd};
  auto scr = scriptum(&mov);
  voo::sires_image blank_img{pd};

  // Descriptor set layout + pool
  vee::sampler yuv_smp =
      vee::create_yuv_sampler(vee::linear_sampler, mov.conv());
  vee::descriptor_set_layout dsl = vee::create_descriptor_set_layout(
      {vee::dsl_fragment_samplers({*yuv_smp}), vee::dsl_fragment_sampler()});

  vee::descriptor_pool dp =
      vee::create_descriptor_pool(1, {vee::combined_image_sampler(2)});
  vee::descriptor_set dset = vee::allocate_descriptor_set(*dp, *dsl);

  vee::sampler smp = vee::create_sampler(vee::linear_sampler);

  vee::update_descriptor_set(dset, 0, mov.iv());
  vee::update_descriptor_set(dset, 1, blank_img.iv());

  while (!interrupted() && !scr.done()) {
    // Generic pipeline stuff
    vee::shader_module vert =
        vee::create_shader_module_from_resource("winnipeg.vert.spv");
    vee::shader_module frag =
        vee::create_shader_module_from_resource("winnipeg.frag.spv");
    vee::pipeline_layout pl = vee::create_pipeline_layout(
        {*dsl}, {vee::vert_frag_push_constant_range<upc>()});

    auto create_grp = [&](vee::render_pass::type rp) {
      return vee::create_graphics_pipeline(
          *pl, rp,
          {
              vee::pipeline_vert_stage(*vert, "main"),
              vee::pipeline_frag_stage(*frag, "main"),
          },
          {
              vee::vertex_input_bind(sizeof(float) * 2),
          },
          {
              vee::vertex_attribute_vec2(0, 0),
          });
    };

    voo::swapchain_and_stuff sw{pd, s};

    vee::gr_pipeline gp = create_grp(sw.render_pass());

    upc pc{};

    resized() = false;
    while (!interrupted() && !resized() && !scr.done()) {
      sw.acquire_next_image();

      auto stp = scr.next();
      auto ov_img = stp.overlay ? stp.overlay : &blank_img;
      vee::update_descriptor_set(dset, 1, ov_img->iv(), *smp);

      mov.pause(m_paused);

      auto ext = sw.extent();
      pc = {
          .aspect =
              static_cast<float>(ext.width) / static_cast<float>(ext.height),
          .s = stp.data,
      };

      // Build command buffer
      {
        voo::cmd_buf_one_time_submit pcb{cb};
        if (m_step && m_paused) {
          mov.pause(false);
          mov.run(cb);
          mov.pause(true);
          m_step = false;
        } else {
          mov.run(cb);
        }
        ov_img->run(pcb);

        auto scb = sw.cmd_render_pass(cb);
        vee::cmd_bind_gr_pipeline(cb, *gp);
        vee::cmd_bind_descriptor_set(cb, *pl, 0, dset);
        vee::cmd_push_vert_frag_constants(cb, *pl, &pc);

        quad.cmd_bind_vertex_buffer(cb, 0);
        vee::cmd_draw(cb, 6);
      }

      sw.queue_submit(q, cb);
      sw.queue_present(q);
    }

    vee::device_wait_idle();
  }
}
