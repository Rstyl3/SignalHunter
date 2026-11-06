#pragma once
// Scanner scheduling: BLE scans in short back-to-back sessions; every
// kWifiSweepPeriodMs it is paused for one promiscuous Wi-Fi sweep (the two
// cannot share the 2.4 GHz radio while we hop channels). New devices are
// announced on Serial from this task, never from radio callbacks.
namespace sh {

void scannerStart();

}  // namespace sh
