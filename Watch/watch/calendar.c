#include "contiki.h"
#include "window.h"
#include "grlib/grlib.h"
#include "memlcd.h"
#include "rtc.h"
#include <stdio.h>

uint8_t month, now_month, day, now_day;
uint16_t year, now_year;
uint8_t currentpage;

static void OnDraw(tContext *pContext)
{
  char buf[20];
  // clear screen
  GrContextForegroundSet(pContext, ClrBlack);
  GrContextBackgroundSet(pContext, ClrWhite);
  GrRectFill(pContext, &fullscreen_clip);

  // draw white table title background
  GrContextForegroundSet(pContext, ClrWhite);
  GrContextBackgroundSet(pContext, ClrBlack);
  const tRectangle rect = {0, 17, 255, 33};
  GrRectFill(pContext, &rect);
  
  printf("\n[#][DEBUG][Calendar] month = %d, toMonthName() = %s", month, toChineseMonthName(month, 1));
  
  // draw the title bar, like "Jan 2020" or "2020 一月"
  if(window_readconfig()->language == 0) GrContextFontSet(pContext, &g_sFontGothic18b);
  else GrContextFontSet(pContext, (const tFont*)&g_sFontUnicode);
  if(window_readconfig()->language == 0) sprintf(buf, "%s %d", toMonthName(month, 1), year);
  else sprintf(buf, "%d %s月", year, toChineseMonthName(month, 0));
  // we don't neew actually that high of title bar, so lower to 5, original is 15
  GrStringDrawCentered(pContext, buf, -1, LCD_WIDTH / 2, 5, 0);

  // now draw week marks
  GrContextForegroundSet(pContext, ClrBlack);
  GrContextBackgroundSet(pContext, ClrWhite);
  if(window_readconfig()->language == 0) GrContextFontSet(pContext, &g_sFontGothic18);
  else GrContextFontSet(pContext, (const tFont*)&g_sFontUnicode);
  for(int i = 0; i < 7; i++)
  {
    if(window_readconfig()->language == 0) GrStringDrawCentered(pContext, week_shortname[i], -1, i * 20 + 11, 26, 0);
    else GrStringDrawCentered(pContext, week_shortname_CN[i], -1, i * 20 + 11, 23, 0);
    // draw line in title bar
    GrLineDrawV(pContext, i * 20, 18, 32);
  }

  GrContextFontSet(pContext, &g_sFontGothic18);
  GrContextForegroundSet(pContext, ClrWhite);
  GrContextBackgroundSet(pContext, ClrBlack);

  // get the start point of this month
  uint8_t weekday = rtc_getweekday(year, month, 1) - 1; // use 0 as index
  uint8_t maxday = rtc_getmaxday(year, month);
  uint8_t y = 42;

  for(int day = 1; day <= maxday; day++)
  {
    sprintf(buf, "%d", day);

    uint8_t today = now_year == year && now_month == month && now_day == day;
    if (today)
    {
      const tRectangle rect = {weekday * 20 + 1, y - 7, 20 + weekday * 20 - 1, y + 7};
      GrRectFill(pContext, &rect);
      GrContextForegroundSet(pContext, ClrBlack);
      GrContextBackgroundSet(pContext, ClrWhite);
    }
    GrStringDrawCentered( pContext, buf, -1, weekday * 20 + 11, y, 0);
    if (today)
    {
      const tRectangle rect2 = {weekday * 20 + 16, y - 5, weekday * 20 + 17, y - 4};
      GrRectFill(pContext, &rect2);

      GrContextForegroundSet(pContext, ClrWhite);
      GrContextBackgroundSet(pContext, ClrBlack);
    }

    if (weekday != 6)
      GrLineDrawV(pContext, (weekday + 1 ) * 20, 32, y + 7);

    weekday++;
    if (weekday == 7)
    {
      GrLineDrawH(pContext, 0, LCD_WIDTH, y + 8);

      weekday = 0;
      y += 16;
    }
  }

  GrLineDrawH(pContext, 0, weekday * 20, y + 8);

  // draw the buttons
  if(window_readconfig()->language == 0){
    if (month == 1)
      sprintf(buf, "%s %d", toMonthName(12, 0), year - 1);
    else
      sprintf(buf, "%s %d", toMonthName(month - 1, 0), year);
    window_button(pContext, KEY_ENTER, buf);

    if (month == 12)
      sprintf(buf, "%s %d", toMonthName(1, 0), year + 1);
    else
      sprintf(buf, "%s %d", toMonthName(month + 1, 0), year);
    window_button(pContext, KEY_DOWN, buf);
  }else{
    if (month == 1)
      sprintf(buf, "%d-%d", year - 1, 12, 0);
    else
      sprintf(buf, "%d-%d", year, month - 1);
    window_button(pContext, KEY_ENTER, buf);

    if (month == 12)
      sprintf(buf, "%d-%d", year + 1, 1);
    else
      sprintf(buf, "%d-%d", year, month + 1);
    window_button(pContext, KEY_DOWN, buf);
  }
}


uint8_t calendar_process(uint8_t ev, uint16_t lparam, void* rparam)
{
  if (ev == EVENT_WINDOW_CREATED)
  {
    rtc_readdate(&year, &month, &day, NULL);
    now_month = month;
    now_year = year;
    now_day = day;
    return 0x80;
  }
  else if (ev == EVENT_KEY_PRESSED)
  {
    if (lparam == KEY_ENTER)
    {
      currentpage--; // previous page
      if (month <= 1)
      {
        month = 12;
        year--;
      }
      else
      {
        month--;
      }
      window_invalid(NULL);
    }
    else if (lparam == KEY_DOWN)
    {
      currentpage++; // next page
      if (month >= 12)
      {
        month = 1;
        year++;
      }
      else
      {
        month++;
      }
      window_invalid(NULL);
    }else if (lparam == KEY_UP)
    {
      year = now_year;
      month = now_month;
      currentpage = 0;
      window_invalid(NULL);
    }
  }
  else if (ev == EVENT_WINDOW_PAINT)
  {
    OnDraw((tContext*)rparam);
  }
  else
  {
    return 0;
  }

  return 1;
}
