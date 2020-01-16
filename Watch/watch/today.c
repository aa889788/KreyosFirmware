#include "contiki.h"

#include "window.h"
#include "pedometer/pedometer.h"
#include <stdio.h>
#include "icons.h"
#include "grlib/grlib.h"
#include "memlcd.h"
#include "memory.h"

enum {
  WALK = 0,
  SPORT = 1
};

#define state d.today.state


#define LINEMARGIN 30
static void drawItem(tContext *pContext, uint8_t n, char icon, const char* text, const char* value)
{
  if (icon)
    {
      GrContextFontSet(pContext, (tFont*)&g_sFontExIcon16);
      GrStringDraw(pContext, &icon, 1, 3, 12 + n * LINEMARGIN, 0);
    }

  // draw text
  if(window_readconfig()->language == 0) GrContextFontSet(pContext, &g_sFontGothic24b);
  else GrContextFontSet(pContext, (const tFont*)&g_sFontUnicode);
  GrStringDraw(pContext, text, -1, 20, 10 + n * LINEMARGIN, 0);

  uint8_t width = GrStringWidthGet(pContext, value, -1);
  GrStringDraw(pContext, value, -1, LCD_WIDTH - width - 4, 10 + n * LINEMARGIN, 0);
}

static int get_int_length(int num){
  int i = 0;
  while(num > 0){
    num /= 10;
    i++;
  }
  return i;
}

static int get_integer_part(int num){
  if(get_int_length(num) <= 2) return 0;
  return (num / 100);
}

static int get_mod_part(int num){
  if(get_int_length(num) <= 2) return num;
  return (num % 10 + num % 100);
  
}

static void onDraw(tContext *pContext)
{
  char buf[30];
  GrContextForegroundSet(pContext, ClrBlack);
  GrRectFill(pContext, &fullscreen_clip);

  GrContextForegroundSet(pContext, ClrWhite);

  if (state == WALK)
  {
    sprintf(buf, "%d", ped_get_steps());
    if(window_readconfig()->language == 0) drawItem(pContext, 0, ICON_STEPS, "Steps", buf);
    else drawItem(pContext, 0, ICON_STEPS, "步", buf);

    uint16_t cals = ped_get_calorie() / 100 / 1000;
    sprintf(buf, "%d", cals);
    if(window_readconfig()->language == 0) drawItem(pContext, 1, ICON_CALORIES, "Calories", buf);
    else drawItem(pContext, 1, ICON_CALORIES, "大卡", buf);

    float dist = (float)ped_get_distance() / 100;

    printf("%d%.%d", get_integer_part(ped_get_distance()), get_mod_part(ped_get_distance()));

    /*if(get_int_length(ped_get_distance) <= 2){
      sprintf(buf, "0.%dm", dist);
    }else{
      
    }*/
    sprintf(buf, "%.2fm", dist);
    if(window_readconfig()->language == 0) drawItem(pContext, 2, ICON_DISTANCE, "Distance", buf);
    else drawItem(pContext, 2, ICON_DISTANCE, "距离", buf);

    sprintf(buf, "%02d:%02d", ped_get_time() / 60, ped_get_time() % 60);
    if(window_readconfig()->language == 0) drawItem(pContext, 3, ICON_TIME, "Active", buf);
    else drawItem(pContext, 3, ICON_TIME, "活动时间", buf);

    // draw progress

    uint16_t goal = window_readconfig()->goal_steps;
    uint32_t steps = ped_get_steps();

    if (steps < goal)
      window_progress(pContext, 5 + 4 * LINEMARGIN, steps * 100 / goal);
    else
      window_progress(pContext, 5 + 4 * LINEMARGIN, 100);
    if(window_readconfig()->language == 0)sprintf(buf, "%d% of %d", (uint16_t)(steps * 100 / goal), goal);
    else sprintf(buf, "%d%%达成%d步", (uint16_t)(steps * 100 / goal), goal);
    GrContextForegroundSet(pContext, ClrWhite);
    GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH/2, 148, 0);
  }
}

uint8_t today_process(uint8_t ev, uint16_t lparam, void* rparam)
{
  switch(ev)
  {
  case EVENT_WINDOW_CREATED:
    state = WALK;
    // fallthrough
  case PROCESS_EVENT_TIMER:
    window_timer(CLOCK_SECOND * 5);
    window_invalid(NULL);
    return 0x80;
  case EVENT_WINDOW_PAINT:
    onDraw((tContext*)rparam);
    break;
#if 0
  case EVENT_KEY_PRESSED:
    if (lparam == KEY_DOWN || lparam == KEY_UP)
    {
      state = 1 - state;
    }
    window_invalid(NULL);
    break;
#endif
  default:
    return 0;
  }

  return 1;
}
