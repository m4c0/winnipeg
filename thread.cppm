export module thread;
#pragma leco add_shader "winnipeg.vert"
#pragma leco add_shader "winnipeg.frag"
import casein;
import coro;
import hai;
import movie;
import script;
import silog;
import sith;
import stubby;
import vee;
import voo;

struct quad {
  static constexpr const auto v_count = 6;
  float p[v_count][2]{
      {0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0},

      {1.0, 1.0}, {0.0, 0.0}, {0.0, 1.0},
  };
};

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

export class thread : sith::thread, public casein::handler {
  casein::native_handle_t m_nptr{};
  vee::physical_device m_pd{};

  volatile bool m_resized{};

  volatile bool m_paused{};
  volatile bool m_step{};

protected:
  virtual script::task<step> scriptum(movie *) = 0;

  auto load_image(const char *file) { return voo::sires_image(file, m_pd); }

public:
  thread() = default;
  virtual ~thread() = default;

  void run() override;

  void create_window(const casein::events::create_window &e) override {
    m_nptr = *e;
    start();
  }
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
  void resize_window(const casein::events::resize_window &e) override {
    if ((*e).live)
      return;

    m_resized = true;
  }
  void quit(const casein::events::quit &e) override {}
};

void thread::run() {
  // Instance
  vee::instance i = vee::create_instance("winnipeg");
  vee::debug_utils_messenger dbg = vee::create_debug_utils_messenger();
  vee::surface s = vee::create_surface(m_nptr);
  auto [pd, qf] = vee::find_physical_device_with_universal_queue(*s);
  m_pd = pd;

  // Device
  vee::device d = vee::create_single_queue_device(pd, qf);
  vee::queue q = vee::get_queue_for_family(qf);

  // Inputs (vertices + instance)
  vee::buffer q_buf = vee::create_vertex_buffer(sizeof(quad));
  vee::device_memory q_mem = vee::create_host_buffer_memory(pd, sizeof(quad));
  vee::bind_buffer_memory(*q_buf, *q_mem, 0);
  {
    vee::mapmem mem{*q_mem};
    *static_cast<quad *>(*mem) = {};
  }

  // Command pool + buffer
  vee::command_pool cp = vee::create_command_pool(qf);
  vee::command_buffer cb = vee::allocate_primary_command_buffer(*cp);

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

    auto create_grp = [&](const vee::render_pass &rp) {
      return vee::create_graphics_pipeline(
          *pl, *rp,
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

    // Sync stuff
    vee::semaphore img_available_sema = vee::create_semaphore();
    vee::semaphore rnd_finished_sema = vee::create_semaphore();
    vee::fence f = vee::create_fence_signaled();

    // Depth buffer
    vee::image d_img = vee::create_depth_image(pd, *s);
    vee::device_memory d_mem = vee::create_local_image_memory(pd, *d_img);
    [[maybe_unused]] decltype(nullptr) d_bind =
        vee::bind_image_memory(*d_img, *d_mem);
    vee::image_view d_iv = vee::create_depth_image_view(*d_img);

    vee::swapchain swc = vee::create_swapchain(pd, *s);
    vee::extent ext = vee::get_surface_capabilities(pd, *s).currentExtent;
    vee::render_pass rp = vee::create_render_pass(pd, *s);

    auto swc_imgs = vee::get_swapchain_images(*swc);
    hai::array<vee::image_view> c_ivs{swc_imgs.size()};
    hai::array<vee::framebuffer> fbs{swc_imgs.size()};

    for (auto i = 0; i < swc_imgs.size(); i++) {
      c_ivs[i] = vee::create_rgba_image_view(swc_imgs[i], pd, *s);
      fbs[i] = vee::create_framebuffer({
          .physical_device = pd,
          .surface = *s,
          .render_pass = *rp,
          .image_buffer = *c_ivs[i],
          .depth_buffer = *d_iv,
      });
    }

    vee::gr_pipeline gp = create_grp(rp);

    upc pc{};

    const auto render = [&](auto &fb) {
      vee::cmd_bind_descriptor_set(cb, *pl, 0, dset);
      vee::cmd_push_vert_frag_constants(cb, *pl, &pc);

      vee::cmd_bind_vertex_buffers(cb, 0, *q_buf);
      vee::cmd_draw(cb, 6);
      vee::cmd_end_render_pass(cb);
    };

    m_resized = false;
    while (!interrupted() && !m_resized && !scr.done()) {
      vee::wait_and_reset_fence(*f);
      auto idx = vee::acquire_next_image(*swc, *img_available_sema);

      auto stp = scr.next();
      auto ov_img = stp.overlay ? stp.overlay : &blank_img;
      vee::update_descriptor_set(dset, 1, ov_img->iv(), *smp);

      mov.pause(m_paused);

      pc = {
          .aspect =
              static_cast<float>(ext.width) / static_cast<float>(ext.height),
          .s = stp.data,
      };

      // Build command buffer
      {
        voo::cmd_buf_one_time_submit pcm{cb};
        if (m_step && m_paused) {
          mov.pause(false);
          mov.run(cb);
          mov.pause(true);
          m_step = false;
        } else {
          mov.run(cb);
        }
        ov_img->run(pcm);

        vee::cmd_begin_render_pass({
            .command_buffer = cb,
            .render_pass = *rp,
            .framebuffer = *fbs[idx],
            .extent = ext,
            .clear_color = {{0.1, 0.2, 0.3, 1.0}},
            .use_secondary_cmd_buf = false,
        });
        vee::cmd_set_scissor(cb, ext);
        vee::cmd_set_viewport(cb, ext);
        vee::cmd_bind_gr_pipeline(cb, *gp);
        render(fbs[idx]);
      }

      vee::queue_submit({
          .queue = q,
          .fence = *f,
          .command_buffer = cb,
          .wait_semaphore = *img_available_sema,
          .signal_semaphore = *rnd_finished_sema,
      });
      vee::queue_present({
          .queue = q,
          .swapchain = *swc,
          .wait_semaphore = *rnd_finished_sema,
          .image_index = idx,
      });
    }

    vee::device_wait_idle();
  }
}
