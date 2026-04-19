/* clang-format off */
#include "glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "portable-file-dialogs.h"

#include "platform.hpp"
#include "core.hpp"
#include "disassemble.hpp"

#include <functional>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>
#include <fstream>
#include <unordered_map>

/* clang-format on */

struct AppState {
  float window_width{1280.0f};
  float window_height{720.0f};
} global_app_state;

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  global_app_state.window_width = width;
  global_app_state.window_height = height;
  glViewport(0, 0, width, height);
}

int main() {
  if (!glfwInit()) {
    std::println(stderr, "Failed to initialize glfw");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  std::unique_ptr<GLFWwindow, std::function<void(GLFWwindow *)>> window{
      glfwCreateWindow(global_app_state.window_width,
                       global_app_state.window_height, "RISC-V Debugger", NULL,
                       NULL),
      glfwDestroyWindow};

  glfwMakeContextCurrent(window.get());
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::println(stderr, "Failed to initialize GLAD.");
    return 1;
  }

  glViewport(0, 0, global_app_state.window_width,
             global_app_state.window_height);

  glfwSetFramebufferSizeCallback(window.get(), framebuffer_size_callback);

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
  ImGui_ImplOpenGL3_Init();

  ImGuiIO &io = ImGui::GetIO();
  ImFont *main_font =
      io.Fonts->AddFontFromFileTTF("CommitMono-400-Regular.otf", 20.0f);

  std::vector<uint8_t> bindata{};
  riscv::Core core0{};

  std::vector<std::pair<riscv::platform::instruction_t, std::string>>
      disassembly{};

  // TODO: choose better method than using std::unordered_map
  std::unordered_map<riscv::platform::instruction_t, bool> breakpoints{};
  bool breakp{true};
  bool autoscroll{};

  while (!glfwWindowShouldClose(window.get())) {
    std::string fstring{};
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    for (const auto &[b, v] : breakpoints) {
      if (b == core0.pc && v) {
        breakp = true;
      }
    }
    // All the actual stuff is here
    // menu
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open .bin file")) {
          const std::vector<std::string> selection =
              pfd::open_file("Open .bin file", "").result();
          if (!selection.empty()) {
            bindata = riscv::bindata(
                selection[0]); // no multi-selection, so always take first file
            core0.load_bindata(bindata);
            disassembly = riscv::disassemble_from_memory(core0.memory, 0,
                                                         core0.memory_used);
            core0.pc = riscv::platform::memory_base;
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Core")) {
        if (ImGui::MenuItem("Run")) {
          breakp = false;
        }
        if (ImGui::MenuItem("Pause")) {
          breakp = true;
        }
        if (ImGui::MenuItem("Step")) {
          core0.execute();
        }
        if (ImGui::MenuItem("Restart")) {
          autoscroll = true;
          core0.pc = riscv::platform::memory_base;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    if ((core0.current_ins == 0x00000000 || core0.current_ins == 0xffffffff) &&
        core0.pc != riscv::platform::memory_base) {
      breakp = true;
    }

    autoscroll = !breakp;

    if (!breakp) {
      core0.execute();
    }

    // riscv core stuff
    ImGui::SetNextWindowSize(
        ImVec2{global_app_state.window_width * 0.33f,
               global_app_state.window_height - ImGui::GetFrameHeight()},
        ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2{0, ImGui::GetFrameHeight()}, ImGuiCond_Once);
    ImGui::Begin("Registers");
    if (ImGui::BeginTable("", 1, ImGuiTableFlags_Borders)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      fstring = std::format("pc: 0x{:08x}", core0.pc);
      ImGui::Text(fstring.c_str());
      for (int32_t i{}; i < 32; ++i) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        fstring = std::format("x{}: 0x{:08x}", i, core0.x.at(i));
        ImGui::Text(fstring.c_str());
      }
      ImGui::EndTable();
    }
    ImGui::End();

    ImGui::SetNextWindowSize(
        ImVec2{global_app_state.window_width -
                   (global_app_state.window_width * 0.33f),
               (global_app_state.window_height - ImGui::GetFrameHeight()) *
                   0.75f},
        ImGuiCond_Once);
    ImGui::SetNextWindowPos(
        ImVec2{global_app_state.window_width * 0.33f, ImGui::GetFrameHeight()},
        ImGuiCond_Once);
    ImGui::Begin("Disassembly");
    uint32_t memory_addr{};
    if (!bindata.empty()) {
      for (const auto &[ins, text] : disassembly) {
        fstring =
            std::format("{:08x}h: {:08x}h, {}",
                        riscv::platform::memory_base + memory_addr, ins, text);
        if (core0.pc == memory_addr + riscv::platform::memory_base) {
          ImVec2 pos = ImGui::GetCursorScreenPos();
          ImVec2 size = ImGui::CalcTextSize(fstring.c_str());
          ImGui::GetWindowDrawList()->AddRectFilled(
              pos, ImVec2(pos.x + ImGui::GetWindowWidth(), pos.y + size.y),
              0xffff0000);
          ImGui::Text(fstring.c_str());
          if (autoscroll)
            ImGui::SetScrollHereY(0.5f);
        } else {
          if (breakpoints.contains(memory_addr +
                                   riscv::platform::memory_base) &&
              breakpoints[memory_addr + riscv::platform::memory_base]) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::CalcTextSize(fstring.c_str());
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + ImGui::GetWindowWidth(), pos.y + size.y),
                0xff0000ff);
          }

          if (ImGui::Selectable(fstring.c_str())) {
            if (breakpoints.contains(memory_addr +
                                     riscv::platform::memory_base)) {
              breakpoints[memory_addr + riscv::platform::memory_base] =
                  !breakpoints[memory_addr + riscv::platform::memory_base];
            } else {
              breakpoints[memory_addr + riscv::platform::memory_base] = 1;
            }
          }
        }
        memory_addr += 4;
      }
    } else {
      ImGui::Text("Load binary file using File > Open .bin file");
    }
    ImGui::End();

    ImGui::SetNextWindowSize(
        ImVec2{global_app_state.window_width -
                   (global_app_state.window_width * 0.33f),
               (global_app_state.window_height - ImGui::GetFrameHeight()) *
                   0.25f},
        ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2{global_app_state.window_width * 0.33f,
                                   (global_app_state.window_height) * 0.76f},
                            ImGuiCond_Once);

    ImGui::Begin("Breakpoints");
    for (const auto &[b, v] : breakpoints) {
      if (b && v) {
        fstring = std::format("- breakpoint @ {:08x}", b);
        ImGui::Text(fstring.c_str());
      }
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window.get());
    glfwPollEvents();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();

  return 0;
}
