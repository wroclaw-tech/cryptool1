#include <string>

#import <Cocoa/Cocoa.h>

#include "afxwin.h"

bool SnapshotWindow(HWND hWnd, const std::string& pngPath) {
    CWnd* wnd = CWnd::FromHandle(hWnd);
    if (!wnd)
        return false;
    NSView* view = (NSView*)wnd->GetWx()->GetHandle();
    NSView* content = [[view window] contentView];
    if (!content)
        content = view;
    [content displayIfNeeded];
    NSBitmapImageRep* rep = [content bitmapImageRepForCachingDisplayInRect:content.bounds];
    if (!rep)
        return false;
    [content cacheDisplayInRect:content.bounds toBitmapImageRep:rep];
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [png writeToFile:[NSString stringWithUTF8String:pngPath.c_str()] atomically:YES];
}
