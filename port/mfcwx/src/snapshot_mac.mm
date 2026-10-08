#import <Cocoa/Cocoa.h>

#include <wx/window.h>

#include <string>

namespace mfcwx {

namespace {

NSView* FindToolbarView(NSView* root) {
    if ([NSStringFromClass([root class]) isEqualToString:@"NSToolbarView"])
        return root;
    for (NSView* child in root.subviews)
        if (NSView* found = FindToolbarView(child))
            return found;
    return nil;
}

bool WritePng(NSView* view, const std::string& path) {
    NSBitmapImageRep* rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
    if (!rep)
        return false;
    [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [png writeToFile:[NSString stringWithUTF8String:path.c_str()] atomically:YES];
}

} // namespace

bool SnapshotWxWindow(wxWindow* w, const std::string& pngPath) {
    if (!w)
        return false;
    NSView* view = (NSView*)w->GetHandle();
    NSView* content = [[view window] contentView];
    if (!content)
        content = view;
    if (!content)
        return false;
    [content displayIfNeeded];
    if (NSView* toolbar = FindToolbarView(content.superview ? content.superview : content)) {
        if (toolbar.bounds.size.height > 0 && pngPath.size() > 4)
            WritePng(toolbar, pngPath.substr(0, pngPath.size() - 4) + "_toolbar.png");
    }
    return WritePng(content, pngPath);
}

} // namespace mfcwx
