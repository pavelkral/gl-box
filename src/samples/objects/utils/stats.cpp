#include "stats.h"
#include "imgui.h"

void Stats::update(float dt) {
    deltaTime = dt;
    frameCount++;
    fpsTimer += dt;

    if(fpsTimer >= 1.0f){
        fps = (float)frameCount / fpsTimer;
        frameCount = 0;
        fpsTimer = 0.0f;
        // std::cout << "FPS: " << fps << " | " << std::endl;
    }
}

void Stats::drawUI(){
    ImGui::SetNextWindowPos(ImVec2(1280 - 10, 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::Begin("Stats", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
    ImGui::End();
}
