#import <Cocoa/Cocoa.h>

#include <wx/window.h>

#include <string>

namespace mfcwx {

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
    NSBitmapImageRep* rep = [content bitmapImageRepForCachingDisplayInRect:content.bounds];
    if (!rep)
        return false;
    [content cacheDisplayInRect:content.bounds toBitmapImageRep:rep];
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [png writeToFile:[NSString stringWithUTF8String:pngPath.c_str()] atomically:YES];
}

} // namespace mfcwx
