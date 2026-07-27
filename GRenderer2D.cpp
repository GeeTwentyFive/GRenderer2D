#include "GRenderer.hpp"

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


const GLchar* VERTEX_SHADER_3D = {"#version 460 core\n"
        "layout (location = 0) in vec3 pos;"
        "layout (location = 1) in vec3 normal;"
        "layout (location = 2) in vec2 uv;"
        "uniform mat4 viewProjection;"
        "layout(binding = 0, std430) readonly buffer ssboVertexInstanceInput {"
                "mat4 modelMatrices[];"
        "};"

        "out vec4 frag_worldPos;"
        "out vec3 frag_normal;"
        "out vec2 frag_uv;"
        "flat out int frag_InstanceID;"

        "void main() {" "frag_worldPos = modelMatrices[gl_InstanceID] * vec4(pos, 1.0f);" "frag_normal = normalize(mat3(transpose(inverse(modelMatrices[gl_InstanceID]))) * normal);" "frag_uv = uv;" "frag_InstanceID = gl_InstanceID;"
                "gl_Position = viewProjection * modelMatrices[gl_InstanceID] * vec4(pos, 1.0f);"
        "}"
""};
const GLchar* FRAGMENT_SHADER_3D = {"#version 460 core\n"
        "in vec4 frag_worldPos;"
        "in vec3 frag_normal;"
        "in vec2 frag_uv;"
        "flat in int frag_InstanceID;"
        "layout(binding = 0) uniform sampler2D texture0;"
        "layout(binding = 1, std430) readonly buffer ssboFragmentInstanceInput {"
                "vec4 instanceColors[];"
        "};"
        "uniform vec3 cameraPos;"

        "out vec4 FragColor;"

        "void main() {"
                "FragColor = (texture(texture0, frag_uv) * instanceColors[frag_InstanceID]) * max(dot(frag_normal, normalize(cameraPos - frag_worldPos.xyz)), 0);"
        "}"
""};

const static float QUAD_VERTICES[] = {
        // pos          // uv
        0.0f, 1.0f,     0.0f, 1.0f,
        1.0f, 0.0f,     1.0f, 0.0f,
        0.0f, 0.0f,     0.0f, 0.0f,

        0.0f, 1.0f,     0.0f, 1.0f,
        1.0f, 1.0f,     1.0f, 1.0f,
        1.0f, 0.0f,     1.0f, 0.0f
};
const GLchar* VERTEX_SHADER_2D = {"#version 460 core\n"
        "layout (location = 0) in vec4 vertex;"  // .xy = position, .zw = uv
        "uniform mat4 projection;"
        "layout(binding = 0, std430) readonly buffer ssboVertexInstanceInput {"
                "mat4 modelMatrices[];"
        "};"

        "out vec2 frag_uv;"
        "flat out int frag_InstanceID;"

        "void main() {" "frag_uv = vertex.zw;" "frag_InstanceID = gl_InstanceID;"
                "gl_Position = projection * modelMatrices[gl_InstanceID] * vec4(vertex.xy, 0.0f, 1.0f);"
        "}"
""};
const GLchar* FRAGMENT_SHADER_2D = {"#version 460 core\n"
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


struct Mesh {
        GLuint vao; GLsizei indices_count;
        GLuint texture_id;
};

struct GRenderer::_impl { uint64_t _last_uid = 0; uint64_t NewUID() { return _last_uid++; }
        uint32_t window_width, window_height;

        GLuint shader_3d_id;
        GLint shader_3d_cameraPos_location; GLint shader_3d_viewProjection_location;
        std::unordered_map<uint64_t, Mesh> meshes;

        GLuint shader_2d_id; GLuint sprite_vao;
        float4x4 ortho_projection;
        GLint shader_2d_projection_location;
        std::unordered_map<uint64_t, GLuint> sprites;

        GLuint ssbo_vertex_instance_input_id; std::vector<float4x4> instance_model_matrices;
        GLuint ssbo_fragment_instance_input_id; std::vector<float4> instance_colors;
};

GRenderer::GRenderer(uint32_t window_width, uint32_t window_height) { this->_ = std::make_unique<GRenderer::_impl>(); GLint status;
        this->_->window_width = window_width; this->_->window_height = window_height;

        if (!gladLoadGL()) ERROR("Failed to load OpenGL"); glViewport(0, 0, this->_->window_width, this->_->window_height); glClearColor(0.0f, 0.0f, 0.0f, 1.0f); glEnable(GL_DEPTH_TEST); glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);


        // 3D
        this->_->shader_3d_id = glCreateProgram(); if (!this->_->shader_3d_id) ERROR("Failed to create 3D shader program");
        GLuint vertex_shader_3d_id = glCreateShader(GL_VERTEX_SHADER); if (!vertex_shader_3d_id) ERROR("Failed to create 3D vertex shader object");
        glShaderSource(vertex_shader_3d_id, 1, &VERTEX_SHADER_3D, NULL); glCompileShader(vertex_shader_3d_id); glGetShaderiv(vertex_shader_3d_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile 3D vertex shader");
        glAttachShader(this->_->shader_3d_id, vertex_shader_3d_id);
        GLuint fragment_shader_3d_id = glCreateShader(GL_FRAGMENT_SHADER); if (!fragment_shader_3d_id) ERROR("Failed to create 3D fragment shader object");
        glShaderSource(fragment_shader_3d_id, 1, &FRAGMENT_SHADER_3D, NULL); glCompileShader(fragment_shader_3d_id); glGetShaderiv(fragment_shader_3d_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile 3D fragment shader");
        glAttachShader(this->_->shader_3d_id, fragment_shader_3d_id);
        glLinkProgram(this->_->shader_3d_id); glGetProgramiv(this->_->shader_3d_id, GL_LINK_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to link 3D shader program"); glDeleteShader(vertex_shader_3d_id); glDeleteShader(fragment_shader_3d_id);

        this->_->shader_3d_cameraPos_location = glGetUniformLocation(this->_->shader_3d_id, "cameraPos");
        this->_->shader_3d_viewProjection_location = glGetUniformLocation(this->_->shader_3d_id, "viewProjection");


        // 2D
        this->_->shader_2d_id = glCreateProgram(); if (!this->_->shader_2d_id) ERROR("Failed to create 2D shader program");
        GLuint vertex_shader_2d_id = glCreateShader(GL_VERTEX_SHADER); if (!vertex_shader_2d_id) ERROR("Failed to create 2D vertex shader object");
        glShaderSource(vertex_shader_2d_id, 1, &VERTEX_SHADER_2D, NULL); glCompileShader(vertex_shader_2d_id); glGetShaderiv(vertex_shader_2d_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile 2D vertex shader");
        glAttachShader(this->_->shader_2d_id, vertex_shader_2d_id);
        GLuint fragment_shader_2d_id = glCreateShader(GL_FRAGMENT_SHADER); if (!fragment_shader_2d_id) ERROR("Failed to create 2D fragment shader object");
        glShaderSource(fragment_shader_2d_id, 1, &FRAGMENT_SHADER_2D, NULL); glCompileShader(fragment_shader_2d_id); glGetShaderiv(fragment_shader_2d_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile 2D fragment shader");
        glAttachShader(this->_->shader_2d_id, fragment_shader_2d_id);
        glLinkProgram(this->_->shader_2d_id); glGetProgramiv(this->_->shader_2d_id, GL_LINK_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to link 2D shader program"); glDeleteShader(vertex_shader_2d_id); glDeleteShader(fragment_shader_2d_id);

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

        this->_->shader_2d_projection_location = glGetUniformLocation(this->_->shader_2d_id, "projection");


        // Shared/Common
        glCreateBuffers(1, &this->_->ssbo_vertex_instance_input_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->_->ssbo_vertex_instance_input_id);
        glNamedBufferStorage(this->_->ssbo_vertex_instance_input_id, SSBO_VERTEX_INSTANCE_INPUT_CAPACITY, NULL, GL_DYNAMIC_STORAGE_BIT);

        glCreateBuffers(1, &this->_->ssbo_fragment_instance_input_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->_->ssbo_fragment_instance_input_id);
        glNamedBufferStorage(this->_->ssbo_fragment_instance_input_id, SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY, NULL, GL_DYNAMIC_STORAGE_BIT);


        //glEnable(GL_DEBUG_OUTPUT); glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam){ERROR(message);}, nullptr);
}

uint64_t GRenderer::CreateMesh(
        const Vertex* vertices, const size_t vertices_count,
        const uint32_t* indices, const size_t indices_count,
        const uint32_t* texture_RGBA, const uint16_t texture_width, const uint16_t texture_height
) {
        uint64_t mesh_id = this->_->NewUID();
        this->_->meshes[mesh_id] = Mesh{};


        glGenVertexArrays(1, &this->_->meshes[mesh_id].vao);
        glBindVertexArray(this->_->meshes[mesh_id].vao);

        unsigned int _vbo; glGenBuffers(1, &_vbo); glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices_count*sizeof(Vertex), vertices, GL_STATIC_DRAW);

        unsigned int _ebo; glGenBuffers(1, &_ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_count*sizeof(uint32_t), indices, GL_STATIC_DRAW);
        this->_->meshes[mesh_id].indices_count = indices_count;

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));


        glCreateTextures(GL_TEXTURE_2D, 1, &this->_->meshes[mesh_id].texture_id);
        glTextureParameteri(this->_->meshes[mesh_id].texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST); glTextureParameteri(this->_->meshes[mesh_id].texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureStorage2D(this->_->meshes[mesh_id].texture_id, 1, GL_RGBA8, texture_width, texture_height);
        glTextureSubImage2D(this->_->meshes[mesh_id].texture_id, 0, 0, 0, texture_width, texture_height, GL_RGBA, GL_UNSIGNED_BYTE, texture_RGBA);


        return mesh_id;
}

void GRenderer::MeshInstance::RotateXEuler(float degrees) noexcept { float4 new_rot = linalg::qmul(
                float4{this->rotation[0], this->rotation[1], this->rotation[2], this->rotation[3]},
                linalg::rotation_quat(float3{1, 0, 0}, (float)(degrees * 3.141592653589793238462643383279502884L/180.0))
        ); this->rotation[0] = new_rot.x; this->rotation[1] = new_rot.y; this->rotation[2] = new_rot.z; this->rotation[3] = new_rot.w;
}
void GRenderer::MeshInstance::RotateYEuler(float degrees) noexcept { float4 new_rot = linalg::qmul(
                float4{this->rotation[0], this->rotation[1], this->rotation[2], this->rotation[3]},
                linalg::rotation_quat(float3{0, 1, 0}, (float)(degrees * 3.141592653589793238462643383279502884L/180.0L))
        ); this->rotation[0] = new_rot.x; this->rotation[1] = new_rot.y; this->rotation[2] = new_rot.z; this->rotation[3] = new_rot.w;
}
void GRenderer::MeshInstance::RotateZEuler(float degrees) noexcept { float4 new_rot = linalg::qmul(
                float4{this->rotation[0], this->rotation[1], this->rotation[2], this->rotation[3]},
                linalg::rotation_quat(float3{0, 0, 1}, (float)(degrees * 3.141592653589793238462643383279502884L/180.0L))
        ); this->rotation[0] = new_rot.x; this->rotation[1] = new_rot.y; this->rotation[2] = new_rot.z; this->rotation[3] = new_rot.w;
}

uint64_t GRenderer::CreateSprite(
        const uint32_t* texture_RGBA, const uint16_t texture_width, const uint16_t texture_height
) {
        uint64_t sprite_id = this->_->NewUID();
        this->_->sprites[sprite_id] = 0;

        glCreateTextures(GL_TEXTURE_2D, 1, &this->_->sprites[sprite_id]);
        glTextureParameteri(this->_->sprites[sprite_id], GL_TEXTURE_MAG_FILTER, GL_NEAREST); glTextureParameteri(this->_->sprites[sprite_id], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureStorage2D(this->_->sprites[sprite_id], 1, GL_RGBA8, texture_width, texture_height);
        glTextureSubImage2D(this->_->sprites[sprite_id], 0, 0, 0, texture_width, texture_height, GL_RGBA, GL_UNSIGNED_BYTE, texture_RGBA);

        return sprite_id;
}

int GRenderer::DrawFrame() noexcept { glClear(GL_COLOR_BUFFER_BIT);
        // 3D pass
        glUseProgram(this->_->shader_3d_id); glClear(GL_DEPTH_BUFFER_BIT);

        // Set view and projection matrices (Camera)
        float3 _camera_pos = {camera_pos[0], camera_pos[1], camera_pos[2]};
        glUniform3fv(
                this->_->shader_3d_cameraPos_location, 1,
                (const GLfloat*)&_camera_pos
        );
        if (camera_pitch < 16385) camera_pitch = 16385; if (camera_pitch > 49151) camera_pitch = 49151;  // 16384..49152 = PI/2..PI+PI/2
        float4x4 view = linalg::lookat_matrix(
                _camera_pos,
                _camera_pos + linalg::qrot(
                        linalg::qmul(
                                linalg::rotation_quat(float3{0, 1, 0}, -(float)(camera_yaw*(2*M_PI/65536))),
                                linalg::rotation_quat(float3{1, 0, 0}, (float)(camera_pitch*(2*M_PI/65536)))
                        ),
                        float3{0, 0, -1}
                ),
                float3{0, 1, 0}
        );
        float4x4 proj = linalg::perspective_matrix(camera_fov, ((float)this->_->window_width)/((float)this->_->window_height), camera_near, camera_far, linalg::fwd_axis::neg_z, linalg::z_range::zero_to_one);
        float4x4 viewProjection = linalg::mul(proj, view);  // NOTE: Flip multiplication order in not-OpenGL
        glUniformMatrix4fv(
                this->_->shader_3d_viewProjection_location, 1, GL_FALSE,
                (const GLfloat*)&viewProjection
        );

        // Draw meshes
        for (const auto& [mesh_id, _mesh_instances] : mesh_instances) {
                glBindVertexArray(this->_->meshes[mesh_id].vao);
                glBindTextureUnit(0, this->_->meshes[mesh_id].texture_id);

                this->_->instance_model_matrices.clear(); this->_->instance_model_matrices.reserve(_mesh_instances.size()); if (this->_->instance_model_matrices.capacity() > SSBO_VERTEX_INSTANCE_INPUT_CAPACITY) return __LINE__;
                this->_->instance_colors.clear(); this->_->instance_colors.reserve(_mesh_instances.size()); if (this->_->instance_colors.capacity() > SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY) return __LINE__;
                for (const auto& mesh_instance : _mesh_instances) {
                        this->_->instance_model_matrices.push_back(linalg::mul(
                                linalg::translation_matrix(float3{
                                        mesh_instance->position[0],
                                        mesh_instance->position[1],
                                        mesh_instance->position[2]
                                }),
                                linalg::mul(
                                        linalg::rotation_matrix(float4{
                                                mesh_instance->rotation[0],
                                                mesh_instance->rotation[1],
                                                mesh_instance->rotation[2],
                                                mesh_instance->rotation[3]
                                        }),
                                        linalg::scaling_matrix(float3{
                                                mesh_instance->scale[0],
                                                mesh_instance->scale[1],
                                                mesh_instance->scale[2]
                                        })
                                )
                        ));

                        this->_->instance_colors.push_back({
                                ((mesh_instance->color_RGBA >> 24) & 0xFF) / 255.0f,
                                ((mesh_instance->color_RGBA >> 16) & 0xFF) / 255.0f,
                                ((mesh_instance->color_RGBA >> 8) & 0xFF) / 255.0f,
                                ((mesh_instance->color_RGBA >> 0) & 0xFF) / 255.0f
                        });
                }
                glNamedBufferSubData(this->_->ssbo_vertex_instance_input_id, 0, this->_->instance_model_matrices.size()*sizeof(float4x4), this->_->instance_model_matrices.data());
                glNamedBufferSubData(this->_->ssbo_fragment_instance_input_id, 0, this->_->instance_colors.size()*sizeof(float4), this->_->instance_colors.data());

                glDrawElementsInstanced(GL_TRIANGLES, this->_->meshes[mesh_id].indices_count, GL_UNSIGNED_INT, 0, _mesh_instances.size());
        }


        // 2D pass
        glUseProgram(this->_->shader_2d_id); glClear(GL_DEPTH_BUFFER_BIT);

        // Set orthographic projection matrix
        glUniformMatrix4fv(
                this->_->shader_2d_projection_location, 1, GL_FALSE,
                (const GLfloat*)&this->_->ortho_projection
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


        return 0;
}

GRenderer::~GRenderer() { glDeleteProgram(this->_->shader_2d_id); glDeleteProgram(this->_->shader_3d_id); }