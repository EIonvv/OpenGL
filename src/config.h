#ifndef CONFIG_H
#define CONFIG_H

#define debug 1
#define release 0
int mode = release;

static bool isDragging = false;

// Initial target FPS (can be overridden by command-line argument)
static int targetFPS = 144;

// Initial target frame time based on target FPS
static double targetFrameTime = 1.0 / targetFPS;
#endif /* CONFIG_H */
