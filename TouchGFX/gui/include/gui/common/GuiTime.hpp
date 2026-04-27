#ifndef GUITIME_HPP
#define GUITIME_HPP

#include <stdint.h>

struct GuiDateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

uint32_t GUI_Time_GetTickMs();
bool GUI_Time_GetDateTime(GuiDateTime& dateTime);

#endif // GUITIME_HPP
