#include "file_dialog.h"

#import <AppKit/AppKit.h>

#include <string>

namespace
{
std::string PathFromURL(NSURL *url)
{
    if (url == nil || url.path == nil)
        return {};

    const char *path = url.path.fileSystemRepresentation;
    return path ? std::string(path) : std::string();
}

std::string ExtensionFromPattern(const char *filterPattern)
{
    if (filterPattern == nullptr)
        return {};

    std::string pattern(filterPattern);
    const size_t wildcard = pattern.rfind("*.");
    if (wildcard == std::string::npos)
        return {};

    std::string extension = pattern.substr(wildcard + 2);
    const size_t separator = extension.find_first_of(";, ");
    if (separator != std::string::npos)
        extension.resize(separator);
    return extension;
}

std::string RunOpenPanel(bool chooseDirectories, const char *filterPattern)
{
    @autoreleasepool
    {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.canChooseDirectories = chooseDirectories;
        panel.canChooseFiles = !chooseDirectories;
        panel.allowsMultipleSelection = NO;
        panel.canCreateDirectories = NO;
        panel.resolvesAliases = YES;
        panel.title = chooseDirectories ? @"Select PDF Folder" : @"Select a File";

        if (!chooseDirectories)
        {
            const std::string extension = ExtensionFromPattern(filterPattern);
            if (!extension.empty())
            {
                NSString *type = [NSString stringWithUTF8String:extension.c_str()];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                panel.allowedFileTypes = @[ type ];
#pragma clang diagnostic pop
            }
        }

        if ([panel runModal] != NSModalResponseOK)
            return {};

        return PathFromURL(panel.URL);
    }
}
} // namespace

namespace FileDialog
{
std::string OpenPDF()
{
    return RunOpenPanel(false, "*.pdf");
}

std::string Open(const char * /*filterName*/, const char *filterPattern)
{
    return RunOpenPanel(false, filterPattern);
}

std::string OpenFolder()
{
    return RunOpenPanel(true, nullptr);
}
} // namespace FileDialog
