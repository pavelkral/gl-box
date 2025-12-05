#ifndef STATS_H
#define STATS_H


struct Stats {
    float deltaTime = 0.0f;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    float fps = 0.0f;

    void update(float dt);

    void drawUI();
};

#endif // STATS_H
