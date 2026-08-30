//
// platform_ios.mm - the two things iOS does differently from Android within
// the Tier 2 backend: finding the executable, and playing a sound.
//
// Everything else in the mobile contract is in platform_mobile_common.cpp.
//
#include "platform/platform.h"
#include "platform/mobile/platform_mobile.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>

#include <cstdint>
#include <vector>

// Held for the life of the sound: AVAudioPlayer stops the moment it is
// released, so a local would cut the clip off immediately.
static AVAudioPlayer* g_voicePlayer = nil;

namespace platform {

std::string getExecutablePath() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) return std::string();
    std::vector<char> buf(size + 1, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return std::string();
    char resolved[PATH_MAX];
    if (realpath(buf.data(), resolved)) return std::string(resolved);
    return std::string(buf.data());
}

// AVAudioPlayer, as the spec requires - not a subprocess, and not an
// mciSendString equivalent, because iOS has neither. miniaudio is deliberately
// not used on mobile: the desktop builds want one shared backend, but an app
// has to cooperate with the system audio session, which is an AVFoundation
// concept with no miniaudio equivalent.
void playVoiceAsync(const std::string& assetRelativePath) {
    const std::string path = assetPath(assetRelativePath);
    if (path.empty()) return;

    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSURL* url = [NSURL fileURLWithPath:nsPath];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSError* sessionError = nil;
        // Ambient: the voice clip is a flourish, so it must not stop the
        // user's music or take over the audio session.
        [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient
                                               error:&sessionError];
        [[AVAudioSession sharedInstance] setActive:YES error:&sessionError];

        NSError* playerError = nil;
        AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithContentsOfURL:url
                                                                      error:&playerError];
        if (!player || playerError) return;   // no device, bad file: stay silent
        g_voicePlayer = player;
        [player prepareToPlay];
        [player play];
    });
}

void shutdownAudio() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [g_voicePlayer stop];
        g_voicePlayer = nil;
    });
}

} // namespace platform
