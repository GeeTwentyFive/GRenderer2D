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


const double TARGET_FRAMETIME_MS = 1.0;  // Should be a power-of-2 (like 1.0, 2.0, 4.0, 8.0) (to align as fraction/multiple of USB input report rate)


#define ERROR(msg) throw std::runtime_error(std::string("[ERROR] ") + __FILE__ + "@" + std::to_string(__LINE__) + " (" + __func__ + "): " + (msg))


int main() { try {
        GWindower_OpenGL gwgl; // (GWindower_OpenGL already calls `glViewport()` and `glClearColor()` internally)

        GRenderer2D gr2d{(uint32_t)gwgl.screen_width, (uint32_t)gwgl.screen_height};

        int texture_width, texture_height, _texture_comp;
        unsigned char* texture = stbi_load("Square.png", &texture_width, &texture_height, &_texture_comp, 4);
        uint64_t sprite_id = gr2d.CreateSprite((uint32_t*)texture, texture_width, texture_height);
        stbi_image_free(texture);

        GRenderer2D::SpriteInstance* sprite_instance = gr2d.AddSprite(sprite_id);
        sprite_instance->size[0] = gwgl.screen_height/8; sprite_instance->size[1] = gwgl.screen_height/8;
        sprite_instance->position[0] = gwgl.screen_width/2 - sprite_instance->size[0]/2; sprite_instance->position[1] = gwgl.screen_height/2 - sprite_instance->size[1]/2;

        static const uint32_t BG_TEXTURE[] = {0xFFFFFFFF};
        GRenderer2D::SpriteInstance* bg = gr2d.AddSprite(gr2d.CreateSprite(BG_TEXTURE, 1, 1));
        bg->size[0] = gwgl.screen_height/2; bg->size[1] = gwgl.screen_height/2;
        bg->position[0] = gwgl.screen_width/2 - sprite_instance->size[0]/2; bg->position[1] = gwgl.screen_height/2 - sprite_instance->size[1]/2;
        bg->z_depth = -0.99f;

        GFramePacer gfp; gfp.target_frametime_ms = TARGET_FRAMETIME_MS;
        while (gwgl.Update()) { if(gwgl.key_states[GW_KEY_ESCAPE]) break;
                double delta_time = gfp.Wait();

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                gr2d.camera_pos[0] += ((float)gwgl.mouse_x_delta)/gwgl.screen_width;
                gr2d.camera_pos[1] -= ((float)gwgl.mouse_y_delta)/gwgl.screen_height;

                if (gwgl.key_states[GW_KEY_D]) sprite_instance->position[0] += delta_time * (float)gwgl.screen_height;
                if (gwgl.key_states[GW_KEY_A]) sprite_instance->position[0] -= delta_time * (float)gwgl.screen_height;
                if (gwgl.key_states[GW_KEY_W]) sprite_instance->position[1] += delta_time * (float)gwgl.screen_height;
                if (gwgl.key_states[GW_KEY_S]) sprite_instance->position[1] -= delta_time * (float)gwgl.screen_height;

                gr2d.DrawFrame();
        }

        return 0;
} catch (const std::exception& e) { std::cout << e.what() << std::endl; return 1; } }