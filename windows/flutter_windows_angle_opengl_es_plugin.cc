#include "flutter_windows_angle_opengl_es_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/texture_registrar.h>

namespace flutter_windows_angle_opengl_es {

GLuint CompileShader(GLenum type, const std::string& source) {
  auto shader = glCreateShader(type);
  const char* s[1] = {source.c_str()};
  glShaderSource(shader, 1, s, NULL);
  glCompileShader(shader);
  return shader;
}

GLuint CompileProgram(const std::string& vertex_shader_source,
                      const std::string& fragment_shader_source) {
  auto program = glCreateProgram();

  auto vs = CompileShader(GL_VERTEX_SHADER, vertex_shader_source);
  auto fs = CompileShader(GL_FRAGMENT_SHADER, fragment_shader_source);
  if (vs == 0 || fs == 0) {
    glDeleteShader(fs);
    glDeleteShader(vs);
    glDeleteProgram(program);
    return 0;
  }
  glAttachShader(program, vs);
  glDeleteShader(vs);
  glAttachShader(program, fs);
  glDeleteShader(fs);
  glLinkProgram(program);
  return program;
}

void FlutterWindowsANGLEOpenGLESPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto plugin = std::make_unique<FlutterWindowsANGLEOpenGLESPlugin>(
      registrar,
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "flutter-windows-ANGLE-OpenGL-ES",
          &flutter::StandardMethodCodec::GetInstance()),
      registrar->texture_registrar());
  plugin->channel()->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });
  registrar->AddPlugin(std::move(plugin));
}

FlutterWindowsANGLEOpenGLESPlugin::FlutterWindowsANGLEOpenGLESPlugin(
    flutter::PluginRegistrarWindows* registrar,
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel,
    flutter::TextureRegistrar* texture_registrar)
    : registrar_(registrar),
      channel_(std::move(channel)),
      texture_registrar_(texture_registrar) {}

FlutterWindowsANGLEOpenGLESPlugin::~FlutterWindowsANGLEOpenGLESPlugin() {}

constexpr char kVertexShader[] = R"(
    attribute vec4 vPosition;
    attribute vec2 vTexCoord;
    varying vec2 texCoord;
    void main()
    {
        texCoord = vTexCoord;
        gl_Position = vPosition;
    })";
constexpr char kFragmentShader[] = R"(
    precision mediump float;
    uniform sampler2D sTexture;
    varying vec2 texCoord;
    void main()
    {
        gl_FragColor = texture2D(sTexture, texCoord);
    })";

void FlutterWindowsANGLEOpenGLESPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

  const flutter::EncodableMap *argsList = std::get_if<flutter::EncodableMap>(method_call.arguments());

  GLuint program;

  auto w = 1280;
  auto h = 720;

  if (method_call.method_name().compare("create") == 0) {

    auto w_it = (argsList->find(flutter::EncodableValue("width")))->second;
    auto h_it = (argsList->find(flutter::EncodableValue("height")))->second;

    w = static_cast<int32_t>(std::get<int32_t>((w_it)));
    h = static_cast<int32_t>(std::get<int32_t>((h_it)));

//    constexpr auto w = 1920;
//    constexpr auto h = 1080;
    // ---------------------------------------------
    surface_manager_ = std::make_unique<ANGLESurfaceManager>(w, h);
    // ---------------------------------------------
    texture_ = std::make_unique<FlutterDesktopGpuSurfaceDescriptor>();
    texture_->struct_size = sizeof(FlutterDesktopGpuSurfaceDescriptor);
    texture_->handle = surface_manager_->handle();
    texture_->width = texture_->visible_width = w;
    texture_->height = texture_->visible_height = h;
    texture_->release_context = nullptr;
    texture_->release_callback = [](void* release_context) {};
    texture_->format = kFlutterDesktopPixelFormatBGRA8888;
    // ---------------------------------------------
    texture_variant_ =
        std::make_unique<flutter::TextureVariant>(flutter::GpuSurfaceTexture(
            kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle,
            [&](auto, auto) { return texture_.get(); }));
    // ---------------------------------------------
//    program = CompileProgram(kVertexShader, kFragmentShader);
    // ---------------------------------------------
    auto id = texture_registrar_->RegisterTexture(texture_variant_.get());
//    texture_registrar_->MarkTextureFrameAvailable(id);
    // ---------------------------------------------
    result->Success(flutter::EncodableValue(id));

  } else if (method_call.method_name().compare("render") == 0) {

    auto id_it = (argsList->find(flutter::EncodableValue("id")))->second;
    auto byte_it = (argsList->find(flutter::EncodableValue("data")))->second;

    int64_t textureId = static_cast<int64_t>(std::get<int64_t>((id_it)));
    std::vector<uint8_t> buffer =
        static_cast<std::vector<uint8_t>>(std::get<std::vector<uint8_t>>((byte_it)));

    // ---------------------------------------------
    surface_manager_->Draw([&]() {

        program = CompileProgram(kVertexShader, kFragmentShader);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(program);

        GLfloat vertices[] = {
//            1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f
            -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f
        };
        GLfloat textureVertices[] = {
//            0.0f, 0.0f,     // left top
//            0.0f, 1.0f,     // left bottom
//            1.0f, 0.0f,     // right top
//            1.0f, 1.0f      // right bottom
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f
        };

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
        glUniform1i(glGetUniformLocation(program, "sTexture"), 0);

        auto buf = static_cast<uint8_t*>(buffer.data());
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA,
            (GLuint)w, (GLuint)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf
        );

        // vertices
        auto ph = glGetAttribLocation(program, "vPosition");
        auto tch = glGetAttribLocation(program, "vTexCoord");

        glEnableVertexAttribArray(ph);
        glEnableVertexAttribArray(tch);

        glVertexAttribPointer(ph, 2, GL_FLOAT, GL_FALSE, 4 * 2, vertices);
        glVertexAttribPointer(tch, 2, GL_FLOAT, GL_FALSE, 4 * 2, textureVertices);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(ph);
        glDisableVertexAttribArray(tch);

    });
    surface_manager_->Read();
    // ---------------------------------------------
    texture_registrar_->MarkTextureFrameAvailable(textureId);
    result->Success(flutter::EncodableValue());

  } else {
    result->NotImplemented();
  }
}

}  // namespace flutter_windows_angle_opengl_es
