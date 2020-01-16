
#include "contiki.h"
#include "status.h"
#include "window.h"
#include "rtc.h"
#include "math.h"
#include "grlib/grlib.h"
#include "memlcd.h"
#include "memory.h"

#include <stdio.h> // for sprintf
#include <string.h>

#define _hour0 d.digit.hour0
#define _minute d.digit.minute
#define _second d.digit.sec
static uint8_t _selection;

extern uint8_t disable_key;

typedef void (*draw_function)(tContext *pContext);

int count_integer_length(int num){
  int i = 0;
  if(num > 0){
    while(num > 0){
      i++;
      num /= 10;
    }
  }else{
    return 1;
  }
  return i;
}

void adjustAMPM(uint8_t hour, uint8_t *outhour, uint8_t *ispm)
{
  *ispm = 0;
  if (hour > 12) // 12 - 23
  {
    *ispm = 1;
    *outhour -= 12;
  }
  else if (hour == 12)
  {
    *ispm = 1;
  }
  else if (hour == 0)
  {
    *outhour = 12;
  }
}

static void drawClock0(tContext *pContext)
{
  uint16_t year;
  uint8_t hour = _hour0;
  uint8_t month, day;
  uint8_t ispm = 0;
  char buf[20];

  rtc_readdate(&year, &month, &day, NULL);

  // draw time

  // adjust hour see if it's am or pm
  adjustAMPM(hour, &hour, &ispm);
  // set font
  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus38);
  // now copy hour and minute to buffer
  sprintf(buf, "%02d:%02d", hour, _minute);
  // actual drawing
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH/2, 60, 0);

  // draw AM/PM

  // get time string size on screen
  int width = GrStringWidthGet(pContext, buf, -1);
  GrContextFontSet(pContext, &g_sFontGothic18);
  if (ispm) buf[0] = 'P';
    else buf[0] = 'A';
  buf[1] = 'M';
  // draw below time, and adjust it on the right side of the screen
  GrStringDraw(pContext, buf, 2, LCD_WIDTH/2+width/2 - GrStringWidthGet(pContext, buf, 2) - 2, 84, 0);
  // now I'll draw seconds on the left side
  sprintf(buf, (count_integer_length(_second) == 1 ? "0%d": "%d"), _second);
  GrStringDraw(pContext, buf, 2, LCD_WIDTH/2-width/2, 84, 0);

  GrContextFontSet(pContext, &g_sFontGothic18);
  sprintf(buf, "%s %d, %d", toMonthName(month, 1), day, year);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH/2, 137, 0);
}

static void drawClock1(tContext *pContext)
{
  uint8_t ispm = 0;
  uint8_t hour = _hour0;
  char buf[20];

  // draw time
  adjustAMPM(hour, &hour, &ispm);

  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus52);

  sprintf(buf, "%02d", hour);
  GrStringDrawCentered(pContext, buf, 2, LCD_WIDTH/ 2, 60, 0);

  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus52);
  sprintf(buf, "%02d", _minute);
  GrStringDrawCentered(pContext, buf, 2, LCD_WIDTH / 2, 100, 0);

  GrContextFontSet(pContext, &g_sFontGothic18b);
  if (ispm) buf[0] = 'P';
    else buf[0] = 'A';
  buf[1] = 'M';
  GrStringDrawCentered(pContext, buf, 2, LCD_WIDTH / 2, 132, 0);
}

static void drawClock2(tContext *pContext)
{
  uint8_t ispm = 0;
  uint8_t hour = _hour0;
  char buf[20];
  const char* buffer;

  // draw time
  adjustAMPM(hour, &hour, &ispm);

  GrContextFontSet(pContext, &g_sFontNimbus34);
  buffer = toEnglish(hour, buf);
  GrStringDraw(pContext, buffer, -1, 5, 58, 0);

  GrContextFontSet(pContext, &g_sFontGothic18);
  buffer = toEnglish(_minute, buf);
  GrStringDraw(pContext, buffer, -1, 5, 88, 0);

  GrContextFontSet(pContext, &g_sFontGothic18);
  if (ispm)
  {
    GrStringDraw(pContext, "In the PM", -1, 5, 120, 0);
  }
  else
  {
    GrStringDraw(pContext, "In the AM", -1, 5, 120, 0);
  }
}


static void drawClock3(tContext *pContext)
{
  uint8_t ispm = 0;
  uint8_t hour = _hour0;
  char buf[20];
  const char* buffer;

  // draw time
  adjustAMPM(hour, &hour, &ispm);

  GrContextFontSet(pContext, &g_sFontNimbus34);

  buffer = toEnglish(hour, buf);
  tRectangle rect = {5, 32, LCD_WIDTH - 5, 80};
  GrRectFill(pContext, &rect);
  GrContextForegroundSet(pContext, ClrBlack);
  GrStringDrawCentered(pContext, buffer, -1, LCD_WIDTH/ 2, 56, 0);
  GrContextForegroundSet(pContext, ClrWhite);
  
  GrContextFontSet(pContext, &g_sFontGothic24b);
  buffer = toEnglish(_minute, buf);
  GrStringDrawCentered(pContext, buffer, -1, LCD_WIDTH / 2, 95, 0);

  GrContextFontSet(pContext, &g_sFontGothic18);
  if (ispm)
  {
    GrStringDrawCentered(pContext, "In the PM", -1, LCD_WIDTH / 2, 120, 0);
  }
  else
  {
    GrStringDrawCentered(pContext, "In the AM", -1, LCD_WIDTH / 2, 120, 0);
  }
}

static void drawClock8(tContext *pContext)
{
    GrContextForegroundSet(pContext, ClrWhite);
    GrRectFill(pContext, &fullscreen_clip);
    GrContextForegroundSet(pContext, ClrBlack);

    drawClock0(pContext);
}

static void drawClock4(tContext *pContext)
{
  uint16_t year;
  uint8_t hour = _hour0;
  uint8_t month, day;
  char buf[20];

  rtc_readdate(&year, &month, &day, NULL);

  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus50);

  sprintf(buf, "%02d:%02d", hour, _minute);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 68, 0);

  GrContextFontSet(pContext, &g_sFontGothic18b);
  sprintf(buf, "%s %d, %d", toMonthName(month, 1), day, year);
  tRectangle rect = {6, 90, LCD_WIDTH-6, 120};
  GrRectFillRound(pContext, &rect, 8);
  GrContextForegroundSet(pContext, ClrBlack);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 105, 0);
}

static void drawClock5(tContext *pContext)
{
  uint16_t year;
  uint8_t hour = _hour0;
  uint8_t month, day;
  uint8_t ispm = 0;
  char buf[20];

  rtc_readdate(&year, &month, &day, NULL);

  // draw time
  adjustAMPM(hour, &hour, &ispm);

  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus46);

  sprintf(buf, "%02d:%02d", hour, _minute);  
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 70, 0);

  int width = GrStringWidthGet(pContext, buf, -1);
  GrContextFontSet(pContext, &g_sFontGothic18b);
  if (ispm) buf[0] = 'P';
    else buf[0] = 'A';
  buf[1] = 'M';
  GrStringDraw(pContext, buf, 2, LCD_WIDTH/2+width/2 - 
    GrStringWidthGet(pContext, buf, 2) - 2
    , 92, 0);

  GrContextFontSet(pContext, &g_sFontGothic18);
  sprintf(buf, "%02d-%02d-%02d", month, day, year - 2000);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 143, 0);
}

static void drawClock6(tContext *pContext)
{
  uint16_t year;
  uint8_t hour = _hour0;
  uint8_t month, day;
  char buf[20];

  rtc_readdate(&year, &month, &day, NULL);

  tRectangle rect = {30, 26, LCD_WIDTH - 30, LCD_Y_SIZE - 37};
  GrRectFillRound(pContext, &rect, 8);
  GrContextForegroundSet(pContext, ClrBlack);
  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus52);
  sprintf(buf, "%02d", hour);
  GrStringDrawCentered(pContext, buf, 1, LCD_WIDTH / 4 + 20, 54, 0);
  GrStringDrawCentered(pContext, buf + 1, 1, LCD_WIDTH / 2 + LCD_WIDTH/4 - 20, 54, 0);

  sprintf(buf, "%02d", _minute);
  GrStringDrawCentered(pContext, buf, 1, LCD_WIDTH / 4 + 20, 100, 0);
  GrStringDrawCentered(pContext, buf + 1, 1, LCD_WIDTH / 2 + LCD_WIDTH/4 - 20, 100, 0);

  GrContextFontSet(pContext, &g_sFontGothic18);
  GrContextForegroundSet(pContext, ClrWhite);
  sprintf(buf, "%s %d, %d", toMonthName(month, 1), day, year);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 150, 0);
}


static void drawClock7(tContext *pContext)
{
  uint16_t year;
  uint8_t hour = _hour0;
  uint8_t month, day;
  uint8_t ispm = 0;
  char buf[20];

  rtc_readdate(&year, &month, &day, NULL);

  // draw time
  adjustAMPM(hour, &hour, &ispm);

  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus52);

  sprintf(buf, "%02d:%02d", hour, _minute);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 70, 0);

  int width = GrStringWidthGet(pContext, buf, -1);
  GrContextFontSet(pContext, &g_sFontGothic18b);
  if (ispm) buf[0] = 'P';
    else buf[0] = 'A';
  buf[1] = 'M';
  GrStringDraw(pContext, buf, 2, LCD_WIDTH/2+width/2 - 
    GrStringWidthGet(pContext, buf, 2) - 2
    , 95, 0);


  GrContextFontSet(pContext, &g_sFontGothic18);
  sprintf(buf, "%s %d, %d", toMonthName(month, 1), day, year);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 35, 0);
}

static void drawClock9(tContext *pContext)
{
  uint16_t year;
  uint8_t hour = _hour0;
  uint8_t month, day;
  char buf[20];

  rtc_readdate(&year, &month, &day, NULL);

  // draw time
  GrContextFontSet(pContext, (tFont*)&g_sFontExNimbus91);

  sprintf(buf, "%02d", hour);
  GrStringDrawCentered(pContext, buf, 1, LCD_WIDTH / 4 + 10, 43, 0);
  GrStringDrawCentered(pContext, buf + 1, 1, LCD_WIDTH / 2 + LCD_WIDTH / 4 - 10, 43, 0);

  sprintf(buf, "%02d", _minute);
  GrStringDrawCentered(pContext, buf, 1, LCD_WIDTH / 4 + 10, 110, 0);
  GrStringDrawCentered(pContext, buf + 1, 1, LCD_WIDTH / 2 + LCD_WIDTH/4 - 10, 110, 0);

  GrContextFontSet(pContext, &g_sFontGothic18b);
  sprintf(buf, "%s %d, %d", toMonthName(month, 1), day, year);
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 157, 0);
}

static const draw_function Clock_selections[] =
{
  drawClock0,
  drawClock1,
  drawClock2,
  drawClock3,
  drawClock4,
  drawClock5,
  drawClock6,
  drawClock7,
  drawClock8,
  drawClock9,
};

uint8_t digitclock_process(uint8_t ev, uint16_t lparam, void* rparam)
{
  if (ev == EVENT_WINDOW_CREATED)
  {
    rtc_readtime(&_hour0, &_minute, NULL);
    if (rparam == NULL)
      _selection = window_readconfig()->digit_clock;
    else
      _selection = (uint8_t)rparam - 1;
    rtc_enablechange(SECOND_CHANGE);
    status_process(EVENT_WINDOW_CREATED, lparam, rparam);
    return 0x80;
  }
  else if (ev == EVENT_WINDOW_ACTIVE)
  {
    rtc_readtime(&_hour0, &_minute, NULL);
  }
  else if (ev == EVENT_WINDOW_PAINT)
  {
    // clear the region
    tContext *pContext = (tContext*)rparam;
    GrContextForegroundSet(pContext, ClrBlack);
    GrRectFill(pContext, &fullscreen_clip);
    GrContextForegroundSet(pContext, ClrWhite);
    Clock_selections[_selection](pContext);
    status_process(STATUS_PAINT_BATTERY, lparam, rparam);
  }
  else if (ev == EVENT_TIME_CHANGED)
  {
    struct datetime* dt = (struct datetime*)rparam;
    _hour0 = dt->hour;
    _minute = dt->minute;
    _second = dt->second;
    window_invalid(NULL);
  }
  else if ((ev == EVENT_KEY_PRESSED) && (disable_key == 0))
  {
    if (lparam == KEY_DOWN)
    {
      _selection += 0x1;
      if (_selection > sizeof(Clock_selections)/sizeof(draw_function) - 1)
      {
        _selection = 0x00;
      }
      window_invalid(NULL);
    }
    else if (lparam == KEY_UP)
    {
      _selection -= 0x1;
      if (_selection == 0xff)
      {
        _selection = sizeof(Clock_selections)/sizeof(draw_function) - 1;
      }
      window_invalid(NULL);
    }
    printf("Now drawing digital clock %d", _selection);
  }
  else if (ev == EVENT_WINDOW_CLOSING)
  {
    rtc_enablechange(0);

    window_readconfig()->default_clock = 1;
    if (_selection != window_readconfig()->digit_clock)
    {
      window_readconfig()->digit_clock = _selection;
      window_writeconfig();
    }
  }
  else
  {
    return 0;
  }

  return 1;
}
