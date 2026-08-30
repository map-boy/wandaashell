//
// ios_bridge.mm - the Objective-C surface an iOS app talks to.
//
// SCOPE. This is the Tier 2 companion app bridge, not the desktop shell. The
// app gets the same tokenizer, pipeline parser, .waa interpreter and
// filesystem built-ins that the desktop shell runs - and nothing that needs to
// launch a program or escalate privilege, because an App Store app cannot do
// either. See docs/TIER2_MOBILE.md before adding anything to this file.
//
#include "core/shell_loop.h"
#include "core/script_parser.h"
#include "core/interpreter.h"
#include "core/output_capture.h"
#include "core/version.h"
#include "builtins/builtins.h"
#include "builtins/builtins_internal.h"
#include "platform/platform.h"
#include "platform/mobile/platform_mobile.h"

#import <Foundation/Foundation.h>

#include <exception>
#include <filesystem>
#include <string>

NS_ASSUME_NONNULL_BEGIN

/// A wandaashell session. Create one per output view.
///
/// Every method returns the text the shell produced rather than writing to a
/// terminal, because there is no terminal: the app renders the result itself.
/// Not thread-safe - drive one instance from one queue.
@interface WandaaShell : NSObject

/// Shell version, matching the desktop build's `waa version`.
@property (class, nonatomic, readonly) NSString *version;

/// Commands this build actually has. Excludes everything the sandbox rules
/// out, so the app can render a keyboard accessory bar from it without
/// offering commands that cannot work.
@property (nonatomic, readonly) NSArray<NSString *> *builtinCommandNames;

/// Runs one command line - built-ins, pipes, redirection and `;` chaining all
/// behave as they do on the desktop. Returns everything it printed.
- (NSString *)runLine:(NSString *)line;

/// Runs .waa source. Returns everything it printed, including any parse or
/// runtime error.
- (NSString *)runScript:(NSString *)source;

/// Plays the bundled voice clip. Safe to call when the device is silenced.
- (void)playStartupVoice;

@end

@implementation WandaaShell

+ (NSString *)version {
    return [NSString stringWithUTF8String:WANDAASHELL_VERSION];
}

- (instancetype)init {
    self = [super init];
    if (!self) return nil;

    // Only the app knows where its bundled assets ended up, so tell the
    // platform layer once, here, rather than having it guess.
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    if (resourcePath) {
        platform::setBundleAssetDir(std::string([resourcePath UTF8String]));
    }

    // The sandbox's Documents directory is the only writable place an app
    // has, so that is what `pwd` should report on launch.
    NSArray<NSString *> *documents =
        NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    if (documents.count > 0) {
        std::error_code ec;
        std::filesystem::current_path(std::string([documents[0] UTF8String]), ec);
    }

    markShellStart();
    return self;
}

- (NSArray<NSString *> *)builtinCommandNames {
    NSMutableArray<NSString *> *names = [NSMutableArray array];
    for (const auto &name : builtinNames()) {
        [names addObject:[NSString stringWithUTF8String:name.c_str()]];
    }
    return names;
}

- (NSString *)runLine:(NSString *)line {
    if (line.length == 0) return @"";
    OutputCapture capture;
    runShellLine(std::string([line UTF8String]));
    return [NSString stringWithUTF8String:capture.text().c_str()];
}

- (NSString *)runScript:(NSString *)source {
    OutputCapture capture;
    try {
        NodePtr program = parseScript(std::string([source UTF8String]));
        Interpreter interp;
        interp.run(program);
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
    return [NSString stringWithUTF8String:capture.text().c_str()];
}

- (void)playStartupVoice {
    platform::playVoiceAsync("wandaa-voice.mp3");
}

- (void)dealloc {
    platform::shutdownAudio();
}

@end

NS_ASSUME_NONNULL_END
