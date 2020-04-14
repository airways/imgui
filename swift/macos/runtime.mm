#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
#include <pthread.h>

#include "imgui.h"
#include "imgui_impl_osx.h"
#include "imgui_impl_metal.h"
#include "runtime.h"

static pthread_mutex_t inMutex;
static pthread_mutex_t outMutex;

void macosResume(void)
{
    pthread_mutex_unlock(&outMutex);
    pthread_mutex_lock(&inMutex);
}

extern "C" void Gui_Do(void);

static void* macosSwiftMainWrapper(void* userdata)
{
    macosSwiftMain();
    for (;;) { // Save all. This will make sure the main function doesn't fall through.
        Gui_Do();
    }
    return nullptr;
}

static void invokeMacosSwiftMain(void)
{
    static bool pthreadCreated;
    if (!pthreadCreated)
    {
        static pthread_t swiftMainThread;
        pthread_mutex_init(&inMutex, 0);
        pthread_mutex_init(&outMutex, 0);
        pthread_mutex_lock(&inMutex);
        pthread_mutex_lock(&outMutex);
        // We don't really run "two threads". We use additional thread to keep
        // the stack. makecontext / swapcontext doesn't really work with Objective-C / Swift
        // runtime, and I got some strange errors.
        pthread_create(&swiftMainThread, 0, macosSwiftMainWrapper, 0);
        pthreadCreated = true;
    } else {
        pthread_mutex_unlock(&inMutex);
    }
    pthread_mutex_lock(&outMutex);
}

// Renderer

@interface Renderer : NSObject <MTKViewDelegate>

@property (nonatomic, strong) id <MTLDevice> device;
@property (nonatomic, strong) id <MTLCommandQueue> commandQueue;

-(nonnull instancetype)initWithView:(nonnull MTKView *)view;

@end

@implementation Renderer {
    ImFont* _cjkFont;
}

-(nonnull instancetype)initWithView:(nonnull MTKView *)view;
{
    self = [super init];
    if(self)
    {
        _device = view.device;
        _commandQueue = [_device newCommandQueue];

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // ImGuiIO &io = ImGui::GetIO();
        // Better font support.
        // io.Fonts->AddFontFromFileTTF("PingFang.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
        ImGui_ImplMetal_Init(_device);
    }

    return self;
}

- (void)drawInMTKView:(MTKView *)view
{
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize.x = view.bounds.size.width;
    io.DisplaySize.y = view.bounds.size.height;

    CGFloat framebufferScale = view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);

    io.DeltaTime = 1 / float(view.preferredFramesPerSecond ?: 60);

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];

    static bool show_demo_window = true;
    static bool show_another_window = false;
    static float clear_color[4] = { 0.28f, 0.36f, 0.5f, 1.0f };

    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil)
    {
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);

        // Here, you could do additional rendering work, including other passes as necessary.

        id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder pushDebugGroup:@"ImGui demo"];

        // Start the Dear ImGui frame
        ImGui_ImplMetal_NewFrame(renderPassDescriptor);
        ImGui_ImplOSX_NewFrame(view);
        ImGui::NewFrame();

        invokeMacosSwiftMain();

        // Rendering
        ImGui::Render();
        ImDrawData *drawData = ImGui::GetDrawData();
        ImGui_ImplMetal_RenderDrawData(drawData, commandBuffer, renderEncoder);

        [renderEncoder popDebugGroup];
        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:view.currentDrawable];
    }

    [commandBuffer commit];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
}

@end

// ViewController

@interface ViewController : NSViewController

@property (nonatomic, readonly) MTKView *mtkView;

@end

@implementation ViewController {
    id _eventMonitor;
    Renderer *_renderer;
}

- (MTKView *)mtkView
{
    return (MTKView *)self.view;
}

- (void)loadView
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    if (!device) {
        NSLog(@"Metal is not supported");
        abort();
    }

    NSSize preferredContentSize = [self preferredContentSize];
    self.view = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, preferredContentSize.width, preferredContentSize.height)
                                        device:device];
}

- (void)dealloc
{
    [NSEvent removeMonitor:_eventMonitor];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _renderer = [[Renderer alloc] initWithView:self.mtkView];

    [_renderer mtkView:self.mtkView drawableSizeWillChange:self.mtkView.bounds.size];

    self.mtkView.delegate = _renderer;

    [[NSTextInputContext currentInputContext] activate];

    // Add a tracking area in order to receive mouse events whenever the mouse is within the bounds of our view
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                                options:NSTrackingMouseMoved | NSTrackingInVisibleRect | NSTrackingActiveAlways
                                                                  owner:self
                                                               userInfo:nil];
    [self.view addTrackingArea:trackingArea];

    // If we want to receive key events, we either need to be in the responder chain of the key view,
    // or else we can install a local monitor. The consequence of this heavy-handed approach is that
    // we receive events for all controls, not just Dear ImGui widgets. If we had native controls in our
    // window, we'd want to be much more careful than just ingesting the complete event stream, though we
    // do make an effort to be good citizens by passing along events when Dear ImGui doesn't want to capture.
    NSEventMask eventMask = NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged | NSEventTypeScrollWheel;
    __weak __typeof(self) wSelf = self;
    _eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:eventMask handler:^NSEvent * _Nullable(NSEvent *event) {
        __strong __typeof(wSelf) sSelf = wSelf;
        if (!sSelf) {
            return nil;
        }
        BOOL wantsCapture = ImGui_ImplOSX_HandleEvent(event, sSelf.view);
        if (event.type == NSEventTypeKeyDown && wantsCapture) {
            return nil;
        } else {
            return event;
        }

    }];

    ImGui_ImplOSX_Init();
}

- (void)mouseMoved:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)mouseDown:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)mouseUp:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)mouseDragged:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)scrollWheel:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (NSSize)preferredContentSize {
    return NSMakeSize(1280, 1280);
}

@end

// AppDelegate

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate {
    NSWindow *_window;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    ViewController* const viewController = [[ViewController alloc] initWithNibName:nil bundle:nil];
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1280, 1280)
                                          styleMask:NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskClosable | NSWindowStyleMaskTitled
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.contentViewController = viewController;
    [_window makeKeyAndOrderFront:nil];
}

@end

extern "C" int main(int argc, const char * argv[])
{
    AppDelegate* const delegate = [AppDelegate new];
    [NSApplication sharedApplication].delegate = delegate;
    const int retval = NSApplicationMain(argc, argv);
    ImGui_ImplOSX_Shutdown();
    ImGui_ImplMetal_Shutdown();
    return retval;
}
