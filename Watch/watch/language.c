#include "contiki.h"
#include "window.h"
#include "grlib/grlib.h"
#include "memlcd.h"
#include "memory.h"
#include "system.h"

#include <stdio.h>

int language_selected = 0;
int process_key(uint8_t key)
{
    if(key == KEY_ENTER)
    {
        window_readconfig()->language = language_selected;
        window_writeconfig();
        printf("Language selection confirm: %s", language_list[window_readconfig()->language]);
        window_close();
        return 1;
    }else if(key == KEY_DOWN || key == KEY_UP)
    {
        language_selected = (language_selected == 1 ? 0 : 1);
        printf("Now selected language is: %s", language_list[language_selected]);
        return 1;
    }
    return 0;
}

static void OnDrawFontConfig(tContext *pContext, int config)
{
    // clear the region
  GrContextForegroundSet(pContext, ClrBlack);
  GrRectFill(pContext, &client_clip);

  GrContextForegroundSet(pContext, ClrWhite);
  GrContextBackgroundSet(pContext, ClrBlack);

  if(language_selected == 0) GrContextFontSet(pContext, &g_sFontGothic24b);
  else GrContextFontSet(pContext, (const tFont*)&g_sFontUnicode);
  int width = GrStringWidthGet(pContext, language_list[config], -1);
  window_selecttext(pContext, language_list[config], -1, 72 - width/2, 70);

  window_button(pContext, KEY_ENTER, "OK");
}

uint8_t language_process(uint8_t ev, uint16_t lparam, void* rparam)
{
    switch(ev)
    {
        case EVENT_WINDOW_CREATED:{
            language_selected = window_readconfig()->language;
            break;
        }
        
        case EVENT_WINDOW_PAINT:{
            OnDrawFontConfig((tContext*)rparam, language_selected);
            break;
        }
        
        case EVENT_KEY_PRESSED:{
            process_key((uint8_t)lparam);
            window_invalid(NULL);
            break;
        }

        default:
            return 0;
    }
    return 1;
}

