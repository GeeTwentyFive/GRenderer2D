#include "GRenderer2D.hpp"

#include <glad/glad.h>
#include <linalg.h>
using namespace linalg::aliases;

#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <string.h>


static const size_t SSBO_VERTEX_INSTANCE_INPUT_CAPACITY = 256 * 1024 * 1024;
static const size_t SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY = 64 * 1024 * 1024;


#define ERROR(msg) throw std::runtime_error(std::string("[ERROR] ") + __FILE__ + "@" + std::to_string(__LINE__) + " (" + __func__ + "): " + (msg))


const static float QUAD_VERTICES[] = {
        // pos          // uv
        0.0f, 1.0f,     0.0f, 1.0f,
        1.0f, 0.0f,     1.0f, 0.0f,
        0.0f, 0.0f,     0.0f, 0.0f,

        0.0f, 1.0f,     0.0f, 1.0f,
        1.0f, 1.0f,     1.0f, 1.0f,
        1.0f, 0.0f,     1.0f, 0.0f
};
const GLchar* VERTEX_SHADER = {"#version 460 core\n"
        "layout (location = 0) in vec4 vertex;"  // .xy = position, .zw = uv
        "uniform mat4 viewProjection;"
        "layout(binding = 0, std430) readonly buffer ssboVertexInstanceInput {"
                "mat4 modelMatrices[];"
        "};"

        "out vec2 frag_uv;"
        "flat out int frag_InstanceID;"

        "void main() {" "frag_uv = vertex.zw;" "frag_InstanceID = gl_InstanceID;"
                "gl_Position = viewProjection * modelMatrices[gl_InstanceID] * vec4(vertex.xy, 0.0f, 1.0f);"
        "}"
""};
const GLchar* FRAGMENT_SHADER = {"#version 460 core\n"
        "in vec2 frag_uv;"
        "flat in int frag_InstanceID;"
        "layout (binding = 0) uniform sampler2D texture0;"
        "layout(binding = 1, std430) readonly buffer ssboFragmentInstanceInput {"
                "vec4 instanceColors[];"
        "};"

        "out vec4 FragColor;"

        "void main() {"
                "vec4 color = texture(texture0, frag_uv) * instanceColors[frag_InstanceID];"
                "if (color.a < 0.5) discard;"
                "FragColor = color;"
        "}"
""};

struct GRenderer2D::_impl { uint64_t _last_uid = 0; uint64_t NewUID() { return _last_uid++; }
        uint32_t window_width, window_height;

        GLuint shader_id; GLuint sprite_vao;
        float4x4 ortho_projection;
        GLint shader_viewProjection_location;
        std::unordered_map<uint64_t, GLuint> sprites;

        GLuint ssbo_vertex_instance_input_id; std::vector<float4x4> instance_model_matrices;
        GLuint ssbo_fragment_instance_input_id; std::vector<float4> instance_colors;
};

GRenderer2D::GRenderer2D(uint32_t window_width, uint32_t window_height) { this->_ = std::make_unique<GRenderer2D::_impl>(); GLint status;
        this->_->window_width = window_width; this->_->window_height = window_height;

        if (!gladLoadGL()) ERROR("Failed to load OpenGL functions");

        this->_->shader_id = glCreateProgram(); if (!this->_->shader_id) ERROR("Failed to create shader program");
        GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER); if (!vertex_shader_id) ERROR("Failed to create vertex shader object");
        glShaderSource(vertex_shader_id, 1, &VERTEX_SHADER, NULL); glCompileShader(vertex_shader_id); glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile 2D vertex shader");
        glAttachShader(this->_->shader_id, vertex_shader_id);
        GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER); if (!fragment_shader_id) ERROR("Failed to create fragment shader object");
        glShaderSource(fragment_shader_id, 1, &FRAGMENT_SHADER, NULL); glCompileShader(fragment_shader_id); glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile 2D fragment shader");
        glAttachShader(this->_->shader_id, fragment_shader_id);
        glLinkProgram(this->_->shader_id); glGetProgramiv(this->_->shader_id, GL_LINK_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to link shader program"); glDeleteShader(vertex_shader_id); glDeleteShader(fragment_shader_id);

        glGenVertexArrays(1, &this->_->sprite_vao);
        glBindVertexArray(this->_->sprite_vao);
        unsigned int _vbo; glGenBuffers(1, &_vbo); glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)0);

        this->_->ortho_projection = {  // (column-major)
                {(float)(2.0 / this->_->window_width),  0,      0,      0},
                {0,     (float)(2.0 / this->_->window_height),  0,      0},
                {0,     0,      -1.0f,  0},
                {-1.0f, -1.0f,  0,      1.0f}
        };

        this->_->shader_viewProjection_location = glGetUniformLocation(this->_->shader_id, "viewProjection");

        glCreateBuffers(1, &this->_->ssbo_vertex_instance_input_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->_->ssbo_vertex_instance_input_id);
        glNamedBufferStorage(this->_->ssbo_vertex_instance_input_id, SSBO_VERTEX_INSTANCE_INPUT_CAPACITY, NULL, GL_DYNAMIC_STORAGE_BIT);

        glCreateBuffers(1, &this->_->ssbo_fragment_instance_input_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->_->ssbo_fragment_instance_input_id);
        glNamedBufferStorage(this->_->ssbo_fragment_instance_input_id, SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY, NULL, GL_DYNAMIC_STORAGE_BIT);

        //glEnable(GL_DEBUG_OUTPUT); glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam){ERROR(message);}, nullptr);
}

uint64_t GRenderer2D::CreateSprite(
        const uint32_t* texture_RGBA, const uint16_t texture_width, const uint16_t texture_height
) {
        uint64_t sprite_id = this->_->NewUID();
        this->_->sprites[sprite_id] = 0;

        glCreateTextures(GL_TEXTURE_2D, 1, &this->_->sprites[sprite_id]); glTextureParameteri(this->_->sprites[sprite_id], GL_TEXTURE_MAG_FILTER, GL_NEAREST); glTextureParameteri(this->_->sprites[sprite_id], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureStorage2D(this->_->sprites[sprite_id], 1, GL_RGBA8, texture_width, texture_height);
        glTextureSubImage2D(this->_->sprites[sprite_id], 0, 0, 0, texture_width, texture_height, GL_RGBA, GL_UNSIGNED_BYTE, texture_RGBA);

        return sprite_id;
}

int GRenderer2D::DrawFrame() noexcept { GLboolean _prev_depth_enabled = glIsEnabled(GL_DEPTH_TEST); glDisable(GL_DEPTH_TEST);
        glUseProgram(this->_->shader_id);

        float4x4 viewProjection = linalg::mul(
                linalg::mul(
                        linalg::translation_matrix(float3{-this->camera_pos[0], -this->camera_pos[1], 0.0}),
                        linalg::scaling_matrix(float3{this->camera_zoom, this->camera_zoom, 1.0})
                ),
                this->_->ortho_projection
        );
        glUniformMatrix4fv(
                this->_->shader_viewProjection_location, 1, GL_FALSE,
                (const GLfloat*)&viewProjection
        );

        // Draw sprites
        glBindVertexArray(this->_->sprite_vao);
        for (const auto& [sprite_id, _sprite_instances] : sprite_instances) {
                glBindTextureUnit(0, this->_->sprites[sprite_id]);

                this->_->instance_model_matrices.clear(); this->_->instance_model_matrices.reserve(_sprite_instances.size()); if (this->_->instance_model_matrices.capacity() > SSBO_VERTEX_INSTANCE_INPUT_CAPACITY) return __LINE__;
                this->_->instance_colors.clear(); this->_->instance_colors.reserve(_sprite_instances.size()); if (this->_->instance_colors.capacity() > SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY) return __LINE__;
                for (const auto& sprite_instance : _sprite_instances) {
                        this->_->instance_model_matrices.push_back(linalg::mul(
                                linalg::translation_matrix(float3{
                                        sprite_instance->position[0],
                                        sprite_instance->position[1],
                                        sprite_instance->z_depth
                                }),
                                linalg::scaling_matrix(float3{
                                        sprite_instance->size[0],
                                        sprite_instance->size[1],
                                        1.0f
                                })
                        ));

                        this->_->instance_colors.push_back({
                                ((sprite_instance->color_RGBA >> 24) & 0xFF) / 255.0f,
                                ((sprite_instance->color_RGBA >> 16) & 0xFF) / 255.0f,
                                ((sprite_instance->color_RGBA >> 8) & 0xFF) / 255.0f,
                                ((sprite_instance->color_RGBA >> 0) & 0xFF) / 255.0f
                        });
                }
                glNamedBufferSubData(this->_->ssbo_vertex_instance_input_id, 0, this->_->instance_model_matrices.size()*sizeof(float4x4), this->_->instance_model_matrices.data());
                glNamedBufferSubData(this->_->ssbo_fragment_instance_input_id, 0, this->_->instance_colors.size()*sizeof(float4), this->_->instance_colors.data());

                glDrawArraysInstanced(GL_TRIANGLES, 0, 6, _sprite_instances.size());
        }


        if (_prev_depth_enabled) glEnable(GL_DEPTH_TEST);

        return 0;
}

GRenderer2D::~GRenderer2D() { glDeleteProgram(this->_->shader_id); }