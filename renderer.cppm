export module renderer;
#pragma leco add_shader "quad.vert"
#pragma leco add_shader "yul2rgb.frag"
import casein;
import hai;
import movie;
import silog;
import sith;
import vee;

struct quad {
  static constexpr const auto v_count = 6;
  float p[v_count][2]{
      {0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0},

      {1.0, 1.0}, {0.0, 0.0}, {0.0, 1.0},
  };
};
struct upc {
  float aspect;
  float time;
};

class thread : public sith::thread {
  casein::native_handle_t m_nptr;
  volatile bool m_resized;

public:
  void start(casein::native_handle_t n) {
    m_nptr = n;
    sith::thread::start();
  }
  void resize() { m_resized = true; }

  void run() override;
};

void thread::run() {
  // Instance
  vee::instance i = vee::create_instance("winnipeg");
  vee::debug_utils_messenger dbg = vee::create_debug_utils_messenger();
  vee::surface s = vee::create_surface(m_nptr);
  auto [pd, qf] = vee::find_physical_device_with_universal_queue(*s);

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

  movie mov{pd};

  // Descriptor set layout + pool
  vee::sampler smp = vee::create_yuv_sampler(vee::linear_sampler, mov.conv());
  vee::descriptor_set_layout dsl =
      vee::create_descriptor_set_layout({vee::dsl_fragment_samplers({*smp})});

  vee::descriptor_pool dp =
      vee::create_descriptor_pool(1, {vee::combined_image_sampler(1)});
  vee::descriptor_set dset = vee::allocate_descriptor_set(*dp, *dsl);

  vee::update_descriptor_set(dset, 0, mov.iv());

  while (!interrupted()) {
    // Generic pipeline stuff
    vee::shader_module vert =
        vee::create_shader_module_from_resource("quad.vert.spv");
    vee::shader_module frag =
        vee::create_shader_module_from_resource("yul2rgb.frag.spv");
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
    while (!interrupted() && !m_resized) {
      vee::wait_and_reset_fence(*f);
      auto idx = vee::acquire_next_image(*swc, *img_available_sema);

      // TODO: update pc.time

      pc = {
          .aspect =
              static_cast<float>(ext.width) / static_cast<float>(ext.height),
      };

      // Build command buffer
      vee::begin_cmd_buf_one_time_submit(cb);
      mov.run(cb);
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
      vee::end_cmd_buf(cb);

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

extern "C" void casein_handle(const casein::event &e) {
  static thread t{};

  static constexpr auto map = [] {
    casein::event_map res{};
    res[casein::CREATE_WINDOW] = [](const casein::event &e) {
      t.start(*e.as<casein::events::create_window>());
    };
    res[casein::RESIZE_WINDOW] = [](auto) { t.resize(); };
    res[casein::QUIT] = [](auto) { t.stop(); };
    return res;
  }();

  map.handle(e);
}
