#ifndef SSIDSCROLLWHEELCONTAINER_HPP
#define SSIDSCROLLWHEELCONTAINER_HPP

#include <gui_generated/containers/ssidScrollWheelContainerBase.hpp>

class ssidScrollWheelContainer : public ssidScrollWheelContainerBase
{
public:
    static const uint16_t MAX_SCAN_RESULTS = 32;
    static const uint16_t MAX_SSID_LENGTH = 33;

    ssidScrollWheelContainer();
    virtual ~ssidScrollWheelContainer() {}

    virtual void initialize();
    virtual void ssidscrollWheelUpdateItem(ssidElement& item, int16_t itemIndex);
    virtual void ssidscrollWheelUpdateCenterItem(ssidElementSelected& item, int16_t itemIndex);
    void refreshScanResults();
    const char* getSelectedSSID() const;
protected:
    struct ScanEntry
    {
        char ssid[MAX_SSID_LENGTH];
    };

    uint16_t scanEntryCount;
    ScanEntry scanEntries[MAX_SCAN_RESULTS];

    const char* getSSIDForIndex(int16_t itemIndex) const;
};

#endif // SSIDSCROLLWHEELCONTAINER_HPP
