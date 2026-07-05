#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

#include "macos_window.h"

void ConfigureMacOSWindow(GLFWwindow *window)
{
    NSWindow *cocoaWindow = glfwGetCocoaWindow(window);
    if (!cocoaWindow)
        return;

    // Native macOS fullscreen moves the NSOpenGL window into a separate Space.
    // On current macOS releases that transition can leave GLFW rendering into
    // a non-interactive surface. Opt out so the green button uses the standard
    // zoom behavior instead: the window fills the usable desktop while keeping
    // its title bar and input routing intact.
    NSWindowCollectionBehavior behavior = [cocoaWindow collectionBehavior];
    behavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
    behavior |= NSWindowCollectionBehaviorFullScreenNone;
    [cocoaWindow setCollectionBehavior:behavior];
}
