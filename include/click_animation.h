#pragma once

enum ClickAnimationType {
  CLICK_ANIM_LBUTTON = 0,
  CLICK_ANIM_RBUTTON,
  CLICK_ANIM_MBUTTON,
  CLICK_ANIM_XBUTTON1,
  CLICK_ANIM_XBUTTON2,
  CLICK_ANIM_SCROLL_UP,
  CLICK_ANIM_SCROLL_DOWN,
};

constexpr int CLICK_ANIM_MAX = 5;
constexpr int CLICK_ANIM_INTERVAL = 16;
constexpr int CLICK_ANIM_FRAMES = 18;
constexpr unsigned int CLICK_ANIM_DEFAULT_RADIUS = 25;

void triggerClickAnimation(int x, int y, ClickAnimationType type);