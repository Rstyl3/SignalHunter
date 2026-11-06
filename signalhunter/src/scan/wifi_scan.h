#pragma once
// Wi-Fi side of the scanner: one promiscuous-mode sweep across the 2.4 GHz
// channels, harvesting beacons (access points), probe requests and data
// frames (stations) with per-frame RSSI. No active scanning: the device
// never transmits.
namespace sh {

void wifiScanInit();    // bring the radio up in station mode, no connection
void wifiSweepOnce();   // blocking: channels 1..13, kPromiscDwellMs each
                        // (longer on the locked target's channel)

}  // namespace sh
