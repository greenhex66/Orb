#pragma once
#include <juce_core/juce_core.h>

// Diagnostic trace: appends one line per step to  %TEMP%\HEX-Q-ORBIT-log.txt
// (opened and closed on every call so the last line survives a crash).
// Used only outside the audio thread. Will be removed once the plugin is confirmed stable.
namespace orbitdiag
{
inline void trace (const juce::String& message)
{
    static juce::CriticalSection lock;
    const juce::ScopedLock sl (lock);

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("HEX-Q-ORBIT-log.txt");

    file.appendText (juce::Time::getCurrentTime().toString (false, true, true, true)
                         + "  [thread " + juce::String ((juce::int64) (juce::pointer_sized_int) juce::Thread::getCurrentThreadId()) + "]  "
                         + message + "\n");
}
}
