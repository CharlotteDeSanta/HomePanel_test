#include <gui/containers/ssidScrollWheelContainer.hpp>
#include <string.h>

#if defined(STM32H743xx)
#include "app_wifi.h"
#endif

namespace
{
void copySsidText(char* dst, uint16_t dstSize, const char* src)
{
    const char* safeSrc = (src != 0) ? src : "";

    if ((dst == 0) || (dstSize == 0U))
    {
        return;
    }

    strncpy(dst, safeSrc, dstSize - 1U);
    dst[dstSize - 1U] = '\0';
}
}

ssidScrollWheelContainer::ssidScrollWheelContainer() :
    scanEntryCount(0U)
{
    memset(scanEntries, 0, sizeof(scanEntries));
}

void ssidScrollWheelContainer::initialize()
{
    ssidScrollWheelContainerBase::initialize();
    ssidscrollWheel.setSelectedItemOffset(65);
#if defined(STM32H743xx)
    (void)APP_WiFi_RequestScan();
#endif
    refreshScanResults();
}

void ssidScrollWheelContainer::ssidscrollWheelUpdateItem(ssidElement& item, int16_t itemIndex)
{
    item.setSSID(getSSIDForIndex(itemIndex));
}

void ssidScrollWheelContainer::ssidscrollWheelUpdateCenterItem(ssidElementSelected& item, int16_t itemIndex)
{
    item.setSSID(getSSIDForIndex(itemIndex));
}

void ssidScrollWheelContainer::refreshScanResults()
{
    ScanEntry nextEntries[MAX_SCAN_RESULTS];
    uint16_t nextCount = 0U;
    bool changed = false;

    memset(nextEntries, 0, sizeof(nextEntries));

#if defined(STM32H743xx)
    if (APP_WiFi_IsScanComplete() != 0U)
    {
        APP_WiFiScanResult_t results[APP_WIFI_SCAN_RESULT_CACHE_SIZE];
        uint32_t copiedCount = APP_WiFi_CopyCachedScanResults(results, APP_WIFI_SCAN_RESULT_CACHE_SIZE);

        if (copiedCount > MAX_SCAN_RESULTS)
        {
            copiedCount = MAX_SCAN_RESULTS;
        }

        for (uint32_t i = 0U; i < copiedCount; i++)
        {
            const char* ssid = (results[i].ssid[0] != '\0') ? results[i].ssid : "<hidden>";
            copySsidText(nextEntries[nextCount].ssid, MAX_SSID_LENGTH, ssid);
            nextCount++;
        }

        if (nextCount == 0U)
        {
            copySsidText(nextEntries[0].ssid, MAX_SSID_LENGTH, "No networks");
            nextCount = 1U;
        }
    }
    else if (APP_WiFi_IsScanAborted() != 0U)
    {
        copySsidText(nextEntries[0].ssid, MAX_SSID_LENGTH, "Scan aborted");
        nextCount = 1U;
    }
    else
    {
        copySsidText(nextEntries[0].ssid, MAX_SSID_LENGTH, "Scanning...");
        nextCount = 1U;
    }
#else
    static const char* const mockSsids[] =
    {
        "HomePanel Demo",
        "LivingRoom_AP",
        "Kitchen_AP",
        "Bedroom_AP",
        "Guest_WiFi"
    };

    nextCount = static_cast<uint16_t>(sizeof(mockSsids) / sizeof(mockSsids[0]));
    for (uint16_t i = 0U; i < nextCount; i++)
    {
        copySsidText(nextEntries[i].ssid, MAX_SSID_LENGTH, mockSsids[i]);
    }
#endif

    if (nextCount != scanEntryCount)
    {
        changed = true;
    }
    else
    {
        for (uint16_t i = 0U; i < nextCount; i++)
        {
            if (strcmp(nextEntries[i].ssid, scanEntries[i].ssid) != 0)
            {
                changed = true;
                break;
            }
        }
    }

    if (changed)
    {
        memcpy(scanEntries, nextEntries, sizeof(scanEntries));
        scanEntryCount = nextCount;
        ssidscrollWheel.setNumberOfItems(static_cast<int16_t>(scanEntryCount));
        ssidscrollWheel.setTouchable(scanEntryCount > 1U);
        ssidscrollWheel.animateToItem(0, 0);
        ssidscrollWheel.invalidate();
    }
}

const char* ssidScrollWheelContainer::getSelectedSSID() const
{
    if (scanEntryCount == 0U)
    {
        return "";
    }

    return getSSIDForIndex(static_cast<int16_t>(ssidscrollWheel.getSelectedItem()));
}

const char* ssidScrollWheelContainer::getSSIDForIndex(int16_t itemIndex) const
{
    if ((itemIndex < 0) || (static_cast<uint16_t>(itemIndex) >= scanEntryCount))
    {
        return "";
    }

    return scanEntries[itemIndex].ssid;
}
