#include "contiki.h"
#include "window.h"
#include "grlib/grlib.h"
#include "memlcd.h"
#include "rtc.h"
#include "bluetooth.h"
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "sportsdata.h"
#include "system.h"
#include "battery.h"
#include "test.h"

/*
* This implement the menu
*/

#define DATA_DATE 0xF1
#define DATA_TIME 0xF2
#define DATA_ANT 0xF3
#define DATA_BT 0xF4
#define DATA_VERSION 0xF5
#define DATA_BTADDR 0xF6
#define DATA_LEGAL 0xF7
#define DATA_LIGHT 0xF8
#define DATA_VOL 0xF9
#define DATA_FONTCONFIG 0xFA
#define DATA_ALARM 0xFB
#define DATA_PATCH 0xFC
#define DATA_LANGUAGE 0xFD
#define NO_DATA 0xFF

extern const unsigned char logoPixel[];

struct MenuItem
{
  unsigned char icon;
  const char *name;
  windowproc handler;
};

uint8_t disable_key;

static const struct MenuItem SetupMenu[] =
    {
        {DATA_DATE, "Date", &configdate_process},
        {DATA_TIME, "Time", &configtime_process},
        {DATA_LIGHT, "Back Light", &configlight_process},
        {DATA_VOL, "Volume", &configvol_process},
        {DATA_BT, "Bluetooth", &btconfig_process},
        {DATA_FONTCONFIG, "Font", &configfont_process},
        {DATA_LANGUAGE, "Language", &language_process},
        {DATA_ALARM, "Alarm", &configalarm_process},
        {-1, "About", &about_process},
        {-1, "Factory Reset", &reset_process},
        //{-1, "Shutdown", &shutdown_process},
        {-1, NULL, NULL}};
static const struct MenuItem SetupMenu_CN[] = {
  {DATA_DATE, "日期", &configdate_process},
  {DATA_TIME, "时间", &configtime_process},
  {DATA_LIGHT, "背光", &configlight_process},
  {DATA_VOL, "音量", &configvol_process},
  {DATA_BT, "蓝牙", &btconfig_process},
  {DATA_FONTCONFIG, "字体", &configfont_process},
  {DATA_LANGUAGE, "语言", &language_process},
  {DATA_ALARM, "闹钟", &configalarm_process},
  {-1, "关于", &about_process},
  {-1, "恢复出厂设置", &reset_process},
  {-1, "重启", &reboot_process},
  {-1, "TEST", &test_switch},
  //{-1, "关机", &system_shutdown},
  {-1, NULL, NULL}};
static const struct MenuItem AboutMenu[] =
    {
        {DATA_VERSION, "Version", NULL},
        {DATA_BTADDR, "Serial", NULL},
        {DATA_LEGAL, "Legal", NULL},
        {-1, "PATCH", NULL},
        {-1, NULL, NULL}};
static const struct MenuItem AboutMenu_CN[] =
    {
        {-1, "版本", NULL},
        {DATA_VERSION, "", NULL},
        {DATA_PATCH, "", NULL},
        {DATA_BTADDR, "序列号", NULL},
        {DATA_LEGAL, "", NULL},
        {-1, NULL, NULL}};
static const struct MenuItem MainMenu[] =
    {
        {'a', "Activity", &today_process},
        {'b', "Sports", &sporttype_process},
        {'c', "Music", &control_process},
        {'d', "Clock", &worldclock_process},
        {'e', "Calendar", &calendar_process},
        {'f', "Stopwatch", &stopwatch_process},
        {'g', "Timer", &countdown_process},
        {'h', "Digital", &digitclock_process},
        {'i', "Analog", &analogclock_process},
        {'j', "Settings", &menu_process},
        {0, NULL, NULL}
    };
static const struct MenuItem MainMenu_CN[] =
    {
        {'a', "信息汇总", &today_process},
        {'b', "运动", &sporttype_process},
        {'c', "音乐", &control_process},
        {'d', "时钟", &worldclock_process},
        {'e', "日历", &calendar_process},
        {'f', "秒表", &stopwatch_process},
        {'g', "计时器", &countdown_process},
        {'h', "数字时钟", &digitclock_process},
        {'i', "模拟时钟", &analogclock_process},
        {'j', "设置", &menu_process},
        {0, NULL, NULL}
    };
static const struct MenuItem TestMenu[] =
    {
        {DATA_BTADDR, "", NULL},
        {0, "Button", &test_button},
        {0, "Motor", &test_motor},
        {0, "Light", &test_light},
        {0, "LCD", &test_lcd},
#ifdef PRODUCT_W001
        {0, "ANT+", &test_ant},
        {0, "Codec", &test_codec},
        {0, "MPU6050", &test_mpu6050},
#endif
        {0, "Bluetooth", &test_bluetooth},
        {0, "GoogleNow", &test_googlenow},
        {0, "Sleep", &test_sleep},
        {0, "BT DUT", &test_dut},
        {0, "Self-test", &selftest_process},
        {0, "ClearSportsData", &test_cleardata},
        {0, "BuildSportsData", &test_builddata},
        {0, "SportsData", &test_sportsdata},
        {0, "Reboot", &test_reboot},
        {0, NULL, NULL}};

extern struct process filesys_process;
extern uint8_t recordoperation_process(uint8_t ev, uint16_t lparam, void *rparam);

#define HistoryActivity d.menu.HistoryActivity
#define history_names d.menu.displaynames
#define file_names d.menu.filenames
#define rows d.menu.rows
#define row_count d.menu.row_count

#define NUM_MENU_A_PAGE 3
#define MAINMENU_SPACE ((168 - 30) / NUM_MENU_A_PAGE)

extern void adjustAMPM(uint8_t hour, uint8_t *outhour, uint8_t *ispm);

static void drawMenuItem(tContext *pContext, const tFont *textFont, int MENU_SPACE, const struct MenuItem *item, int index, int selected)
{
  if (selected)
  {
    // draw a rect
    GrContextForegroundSet(pContext, ClrWhite);
    GrContextBackgroundSet(pContext, ClrBlack);
  }
  else
  {
    GrContextForegroundSet(pContext, ClrBlack);
    GrContextBackgroundSet(pContext, ClrWhite);
  }

  tRectangle rect = {2, 23 + index * MENU_SPACE, LCD_WIDTH - 2, 15 + (index + 1) * MENU_SPACE};
  GrRectFillRound(pContext, &rect, 2);

  if (!selected)
  {
    // draw a rect
    GrContextForegroundSet(pContext, ClrWhite);
    GrContextBackgroundSet(pContext, ClrBlack);
  }
  else
  {
    GrContextForegroundSet(pContext, ClrBlack);
    GrContextBackgroundSet(pContext, ClrWhite);
  }

  if (item->icon < 0x80 && item->icon != 0)
  {
    GrContextFontSet(pContext, (tFont *)&g_sFontExIcon32);
    GrStringDraw(pContext, (const char *)&item->icon, 1, 4, 13 + (MENU_SPACE - 16) / 2 + index * MENU_SPACE, 0);
  }

  GrContextFontSet(pContext, textFont);
  if (item->icon < 0x80 && item->icon != 0)
  {
    GrStringDraw(pContext, item->name, -1, 40, 17 + (MENU_SPACE - 16) / 2 + index * MENU_SPACE, 0);
  }
  else
  {
    char buf[20];
    int width;
    // <= 0
    GrStringDraw(pContext, item->name, -1, 5, 17 + (MENU_SPACE - 16) / 2 + index * MENU_SPACE, 0);
    if(window_readconfig()->language == 0) GrContextFontSet(pContext, &g_sFontGothic18);
    else GrContextFontSet(pContext, (const tFont*)&g_sFontUnicode);
    switch (item->icon)
    {
    case 0:
    case 0xFF:
      return;
    case DATA_DATE:
    {
      uint8_t month, day;
      uint16_t year;
      rtc_readdate(&year, &month, &day, NULL);
      if(window_readconfig()->language == 0) sprintf(buf, "%s %d, %04d", toMonthName(month, 0), day, year);
      else sprintf(buf, "%d-%d-%d", year, month, day);
      break;
    }
    case DATA_TIME:
    {
      uint8_t hour, minute;
      char buf0[2];
      uint8_t ispm = 0;
      rtc_readtime(&hour, &minute, NULL);
      // draw time
      adjustAMPM(hour, &hour, &ispm);

      if(window_readconfig()->language == 0) 
      {
        if (ispm)
          buf0[0] = 'P';
        else
          buf0[0] = 'A';
        buf0[1] = 'M';
        sprintf(buf, "%02d:%02d %c%c", hour, minute, buf0[0], buf0[1]);
      }else{
        sprintf(buf, "%02d:%02d %s%s", hour, minute, (ispm ? "下" : "上"), "午");
      }
      break;
    }
    case DATA_BT:
      if(window_readconfig()->language == 0) sprintf(buf, "%s", bluetooth_running() ? "ON" : "OFF");
      else sprintf(buf, "%s", bluetooth_running() ? "开" : "关");
      break;
    case DATA_ANT:
      sprintf(buf, "%s", "OFF");
      break;
    case DATA_LEGAL:
      strcpy(buf, "kreyos.com/legal");
      break;
    case DATA_VERSION:
      strcpy(buf, FWVERSION);
      break;
    case DATA_LIGHT:
      sprintf(buf, "%d", window_readconfig()->light_level);
      break;
    case DATA_VOL:
      sprintf(buf, "%d", window_readconfig()->volume_level);
      break;
    case DATA_BTADDR:
    {
      const char *ptr = (const char *)system_getserial();
      sprintf(buf, "%02X%02X", ptr[4], ptr[5]);
      break;
    }
    case DATA_FONTCONFIG:
    {
      if(window_readconfig()->language == 0) strcpy(buf, fontconfig_name[window_readconfig()->font_config]);
      else strcpy(buf, fontconfig_name_CN[window_readconfig()->font_config]);
      break;
    }
    case DATA_LANGUAGE: {
      if(window_readconfig()->language == 0) strcpy(buf, language_list[window_readconfig()->language]);
      else strcpy(buf, language_list_CN[window_readconfig()->language]);
      break;
    }
    case DATA_ALARM:
    {
      uint8_t hour, minute;
      char buf0[2];
      uint8_t ispm = 0;

      ui_config *uiconf = window_readconfig();
      if (uiconf->alarms[0].flag == 0)
      {
        if(window_readconfig()->language == 0) strcpy(buf, "Disabled");
        else strcpy(buf, "关");
        break;
      }

      hour = uiconf->alarms[0].hour;
      minute = uiconf->alarms[0].minutes;
      // draw time
      adjustAMPM(hour, &hour, &ispm);

      if(window_readconfig()->language == 0){
        if (ispm)
          buf0[0] = 'P';
        else
          buf0[0] = 'A';
        buf0[1] = 'M';
        sprintf(buf, "%02d:%02d %c%c", hour, minute, buf0[0], buf0[1]);
      }else{
        sprintf(buf, "%s%02d:%02d ", ispm?"下午":"上午" , hour, minute);
      }
      break;
    }
    case DATA_PATCH:
      sprintf(buf, "Patched: Spike");
      break;
    default:
      strcpy(buf, "TODO");
      break;
    }
    width = GrStringWidthGet(pContext, buf, -1);
    GrStringDraw(pContext, buf, -1, LCD_WIDTH - 12 - width, 17 + (MENU_SPACE - 8) / 2 + index * MENU_SPACE, 0);
  }
}

static const struct MenuItem *Items;
static uint8_t currentTop, current;
static uint8_t menuLength;

static void OnDraw(tContext *pContext)
{
  const tFont *textfont;
  GrContextForegroundSet(pContext, ClrBlack);
  GrRectFill(pContext, &client_clip);

  struct MenuItem const *item = Items;
/*
  
*/
  if(window_readconfig()->language == 0){
    if (item == MainMenu)
      textfont = &g_sFontGothic28b;
    else
      textfont = &g_sFontGothic24b;
    }
  else textfont = (const tFont *)&g_sFontUnicode;
  item += currentTop;

  for (int i = 0; i < NUM_MENU_A_PAGE; i++)
  {
    if (item->name == NULL)
      break;

    drawMenuItem(pContext, textfont, MAINMENU_SPACE, item, i, current == currentTop + i);
    item++;
  }

  if (item->name != NULL)
  {
    GrContextForegroundSet(pContext, ClrWhite);
    // there is something more
    for (int i = 0; i < 8; i++)
    {
      GrLineDrawH(pContext, LCD_WIDTH / 2 - i, LCD_WIDTH / 2 + i, LCD_Y_SIZE - 5 - i);
    }
    GrContextForegroundSet(pContext, ClrBlack);
  }

  if (currentTop > 0)
  {
    GrContextForegroundSet(pContext, ClrWhite);
    // draw some grey area means something in the up
    for (int i = 0; i < 8; i++)
    {
      GrLineDrawH(pContext, LCD_WIDTH / 2 - i, LCD_WIDTH / 2 + i, 18 + i);
    }
    GrContextForegroundSet(pContext, ClrBlack);
  }
}

static void getMenuLength()
{
  menuLength = 0;
  while (Items[menuLength].name != NULL)
    menuLength++;
}

uint8_t about_process(uint8_t ev, uint16_t lparam, void *rparam)
{
  switch (ev)
  {
  case EVENT_WINDOW_PAINT:
  {
    tContext *pContext = (tContext *)rparam;
    GrContextForegroundSet(pContext, ClrBlack);
    GrRectFill(pContext, &client_clip);
    struct MenuItem const *item = (window_readconfig()->language == 0 ? AboutMenu : AboutMenu_CN);
    int i = 0;
    
    while (item != NULL)
    {
      if (item->name == NULL)
        break;

      if(window_readconfig()->language == 0) drawMenuItem(pContext, &g_sFontGothic18b, (168 - 30) / 5, item, i, 0);
      else drawMenuItem(pContext, (const tFont*)&g_sFontUnicode, (168 - 30) / 5, item, i, 0);
      i++;
      item++;
    }
    return 1;
  }
  }

  return 0;
}

static void menu_timeout()
{
  if (battery_state() == BATTERY_STATE_DISCHARGING)
  {
    disable_key = 1;

    // check analog or digit
    if (!window_readconfig()->default_clock)
      window_open(&analogclock_process, NULL);
    else
      window_open(&digitclock_process, NULL);
  }
  else
  {
    window_open(&charging_process, NULL);
  }
}

uint8_t menu_process(uint8_t ev, uint16_t lparam, void *rparam)
{
  static struct etimer timer;
  switch (ev)
  {
  case EVENT_WINDOW_CREATED:
  {
    printf("Current rparam (Menu): %d", rparam);
    if (rparam == NULL)
    {
      if(window_readconfig()->language == 0) Items = (system_testing() == 1) ? TestMenu : MainMenu;
      else Items = (system_testing() == 1) ? TestMenu : MainMenu_CN;
    }
    else if (strcmp(rparam, "Settings") == 0 || strcmp(rparam, "设置") == 0)
    {
      if(window_readconfig()->language == 0) Items = SetupMenu;
      else Items = SetupMenu_CN;
    }
    else if (strcmp(rparam, "Test") == 0)
    {
      Items = TestMenu;
    }

    getMenuLength();

    current = currentTop = 0;
    break;
  }
  case EVENT_WINDOW_ACTIVE:
  {
    disable_key = 0;
    if (!system_testing())
      etimer_set(&timer, CLOCK_SECOND * 30); // #menu_debug_1 why? 
    break;
  }
  case EVENT_WINDOW_DEACTIVE:
  {
    etimer_stop(&timer);
    break;
  }
  case PROCESS_EVENT_TIMER:
  {
    if (rparam == &timer)
    {
      menu_timeout(); // #menu_debug_1 That's why. 
    }
    break;
  }
  case EVENT_WINDOW_PAINT:
  {
    OnDraw((tContext *)rparam);
    return 1;
  }
  case EVENT_KEY_PRESSED:
  {
    etimer_set(&timer, CLOCK_SECOND * 30);
    if (lparam == KEY_UP)
    {
      if (current > 0)
      {
        current--;
        if (currentTop > current)
        {
          currentTop--;
        }
      }
      else
      {
        current = menuLength - 1;
        currentTop = current - NUM_MENU_A_PAGE + 1;
      }

      ///TODO: optimize this
      window_invalid(NULL);
    }
    else if (lparam == KEY_DOWN)
    {
      if (Items[current + 1].name != NULL)
      {
        current++;
        if (currentTop + NUM_MENU_A_PAGE <= current)
        {
          currentTop++;
        }
      }
      else
      {
        current = currentTop = 0;
      }
      ///TODO: optimize this
      window_invalid(NULL);
    }
    else if (lparam == KEY_ENTER)
    {
      if (Items[current].handler)
      {
        if (Items[current].handler == &menu_process)
        {
          if (current == 9)
          {
            if(window_readconfig()->language == 0) Items = SetupMenu;
            else Items = SetupMenu_CN;
          }
          getMenuLength();
          current = currentTop = 0;
          window_invalid(NULL);
        }
        else
        {
          window_open(Items[current].handler, NULL);
        }
      }
    }
    break;
  }
  case EVENT_EXIT_PRESSED:
  {
    if (Items == SetupMenu || Items == SetupMenu_CN)
    {
      if(window_readconfig()->language == 0) Items = MainMenu;
      else Items = MainMenu_CN;
      currentTop = 7;
      current = 9;
      getMenuLength();
      window_invalid(NULL);
    }
    else
    {
      menu_timeout();
    }
    break;
  }

  default:
    return 0;
  }

  return 1;
}

uint8_t test_switch(uint8_t ev, uint16_t lparam, void *rparam)
{
  return menu_process(
      ev,
      lparam,
      &"Test");
}

extern uint8_t shutdown_mode;
uint8_t shutdown_process(uint8_t ev, uint16_t lparam, void *rparam)
{
  shutdown_mode = 1;

  return 1;
}

uint8_t reboot_process(uint8_t ev, uint16_t lparam, void *rparam)
{
  switch (ev)
  {
    case EVENT_WINDOW_PAINT:
    {
      GrContextForegroundSet((tContext *)rparam, ClrBlack);
      tRectangle rect = {0, 0, LCD_WIDTH, LCD_Y_SIZE};
      GrRectFill((tContext *)rparam, &rect);
      printf("Preparing to reboot...\n");
      printf("Now clear screen.\n");
      GrImageDraw((tContext *)rparam, logoPixel, 8, 60);
      GrContextFontSet((tContext *)rparam, &g_sFontGothic18);
      GrStringDrawWrap((tContext *)rparam, "Rebooting...", 20, 100, LCD_WIDTH - 20, ALIGN_CENTER);
      printf("Drop reboot screen.\n");
      clock_delay(1000000);
      printf("Delay for a while.\n");
      system_reset();
      return 1;
    }
  }
  return 0;
}