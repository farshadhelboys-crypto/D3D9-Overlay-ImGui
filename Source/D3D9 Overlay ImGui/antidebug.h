#pragma once

namespace AntiDebug {
    // Returns true if a debugger / common RE tool is detected
    bool Detected();
    // If detected: report to license server, show message, exit
    void Guard();
}
