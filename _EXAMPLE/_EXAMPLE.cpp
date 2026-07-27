#include <GWindower_OpenGL/GWindower_OpenGL.hpp>
#include "../GRenderer2D.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
#include <linalg.h>
using namespace linalg::aliases;
#include <GFramePacer.hpp>

#include <stdexcept>
#include <string>
#include <math.h>
#include <chrono>
#include <thread>
#include <iostream>


const double TARGET_FRAMETIME_MS = 2.0;  // Should be a power-of-2 (like 1.0, 2.0, 4.0, 8.0) (to align as fraction/multiple of USB input report rate)

const int CAMERA_SENSITIVITY = 16;
const int CAMERA_MOVE_SPEED = 8;


#define ERROR(msg) throw std::runtime_error(std::string("[ERROR] ") + __FILE__ + "@" + std::to_string(__LINE__) + " (" + __func__ + "): " + (msg))


int main() { try {  // TODO; WIP
        GWindower_OpenGL gwgl;

        GRenderer2D gr{(uint32_t)gwgl.screen_width, (uint32_t)gwgl.screen_height};

        int texture_width, texture_height, _texture_comp;
        unsigned char* texture = stbi_load("Square.png", &texture_width, &texture_height, &_texture_comp, 4);
        uint64_t cube_mesh_id = gr.CreateMesh(
                (GRenderer::Vertex*)cube_mesh_data.vertices.data(), cube_mesh_data.vertices.size(),
                (uint32_t*)cube_mesh_data.indices.data(), cube_mesh_data.indices.size(),
                (uint32_t*)texture, texture_width, texture_height
        );
        stbi_image_free(texture);

        GRenderer::MeshInstance* cube_mesh_instance_1 = gr.AddMesh(cube_mesh_id);
        cube_mesh_instance_1->scale[0] = 32.0f; cube_mesh_instance_1->scale[2] = 32.0f;
        cube_mesh_instance_1->position[1] = -2.0f;
        GRenderer::MeshInstance* cube_mesh_instance_2 = gr.AddMesh(cube_mesh_id);
        cube_mesh_instance_2->RotateZEuler(10.0f);

        static const uint32_t CROSSHAIR_TEXTURE[] = {
                0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000,
                0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFF000000,
                0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000,
                0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFF000000,
                0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000
        };
        uint64_t sprite_crosshair_id = gr.CreateSprite(CROSSHAIR_TEXTURE, 5, 5);
        GRenderer::SpriteInstance* sprite_crosshair = gr.AddSprite(sprite_crosshair_id);
        sprite_crosshair->size[0] = gwgl.screen_height/180; sprite_crosshair->size[1] = gwgl.screen_height/180;
        sprite_crosshair->position[0] = gwgl.screen_width/2 - sprite_crosshair->size[0]/2; sprite_crosshair->position[1] = gwgl.screen_height/2 - sprite_crosshair->size[1]/2;

        GFramePacer gfp; gfp.target_frametime_ms = TARGET_FRAMETIME_MS;
        while (gwgl.Update()) {
                double delta_time = gfp.Wait();

                if(gwgl.key_states[GW_KEY_ESCAPE]) break;

                gr.camera_yaw += gwgl.mouse_x_delta * CAMERA_SENSITIVITY;
                gr.camera_pitch += gwgl.mouse_y_delta * CAMERA_SENSITIVITY;
                gr.camera_pos[1] += (-gwgl.key_states[GW_KEY_LEFT_CONTROL] + gwgl.key_states[GW_KEY_SPACE]) * CAMERA_MOVE_SPEED * delta_time;
                float2 move_dir = float2{
                        (float)(-gwgl.key_states[GW_KEY_A] + gwgl.key_states[GW_KEY_D]),
                        (float)(-gwgl.key_states[GW_KEY_W] + gwgl.key_states[GW_KEY_S])
                };
                if (linalg::length2(move_dir) != 0) move_dir = linalg::normalize(move_dir);
                move_dir = linalg::rot((float)(gr.camera_yaw*(2*M_PI/65536)), move_dir);
                gr.camera_pos[0] += -move_dir.x * CAMERA_MOVE_SPEED * delta_time;
                gr.camera_pos[2] += -move_dir.y * CAMERA_MOVE_SPEED * delta_time;

                gr.DrawFrame();
        }

        return 0;
} catch (const std::exception& e) { std::cout << e.what() << std::endl; return 1; } }