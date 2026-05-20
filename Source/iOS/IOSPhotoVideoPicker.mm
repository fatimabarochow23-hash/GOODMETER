#include "IOSPhotoVideoPicker.h"

#if JUCE_IOS
 #import <PhotosUI/PhotosUI.h>
 #import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
 #import <UIKit/UIKit.h>

static NSMutableArray* gmActivePhotoVideoDelegates()
{
    static NSMutableArray* delegates = [[NSMutableArray alloc] init];
    return delegates;
}

static UIWindow* gmFindKeyWindow()
{
    UIWindow* targetWindow = nil;

    if (@available(iOS 13.0, *))
    {
        for (UIScene* scene in [UIApplication sharedApplication].connectedScenes)
        {
            if (![scene isKindOfClass:[UIWindowScene class]])
                continue;

            UIWindowScene* windowScene = (UIWindowScene*) scene;
            for (UIWindow* window in windowScene.windows)
            {
                if (window.isKeyWindow)
                {
                    targetWindow = window;
                    break;
                }
            }

            if (targetWindow != nil)
                break;
        }
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (targetWindow == nil)
        targetWindow = [UIApplication sharedApplication].keyWindow;
#pragma clang diagnostic pop

    return targetWindow;
}

static UIViewController* gmFindPresenter()
{
    UIWindow* targetWindow = gmFindKeyWindow();
    if (targetWindow == nil)
        return nil;

    UIViewController* presenter = targetWindow.rootViewController;
    while (presenter.presentedViewController != nil)
        presenter = presenter.presentedViewController;

    return presenter;
}

static NSString* gmSanitizedFileBase(NSString* raw)
{
    NSString* base = raw.length > 0 ? raw : @"PhotosVideo";
    NSCharacterSet* invalid = [NSCharacterSet characterSetWithCharactersInString:@"/\\?%*|\"<>:"];
    NSArray* parts = [base componentsSeparatedByCharactersInSet:invalid];
    NSString* joined = [parts componentsJoinedByString:@"_"];
    return joined.length > 0 ? joined : @"PhotosVideo";
}

static NSURL* gmUniqueDocumentsUrlForProvider(NSItemProvider* provider, NSURL* sourceUrl)
{
    NSArray<NSURL*>* docs = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                                    inDomains:NSUserDomainMask];
    NSURL* docsUrl = docs.firstObject;
    if (docsUrl == nil)
        return nil;

    NSString* sourceExtension = sourceUrl.pathExtension.length > 0 ? sourceUrl.pathExtension : @"mov";
    NSString* suggested = provider.suggestedName.length > 0 ? provider.suggestedName : @"PhotosVideo";
    NSString* base = gmSanitizedFileBase([suggested stringByDeletingPathExtension]);
    NSString* fileName = [NSString stringWithFormat:@"%@-%lld.%@",
                          base,
                          (long long) ([[NSDate date] timeIntervalSince1970] * 1000.0),
                          sourceExtension];

    return [docsUrl URLByAppendingPathComponent:fileName];
}

static NSString* gmBestVideoTypeIdentifier(NSItemProvider* provider)
{
    for (NSString* identifier in provider.registeredTypeIdentifiers)
    {
        UTType* type = [UTType typeWithIdentifier:identifier];
        if ([type conformsToType:UTTypeMovie] || [type conformsToType:UTTypeVideo])
            return identifier;
    }

    return UTTypeMovie.identifier;
}

@interface GOODMETERPhotoVideoPickerDelegate : NSObject <PHPickerViewControllerDelegate>
{
@public
    GoodMeterIOSPhotoVideoPicker::Completion completion;
}
@property (nonatomic, strong) PHPickerViewController* picker;
- (void)finishWithURL:(juce::URL)url;
@end

@implementation GOODMETERPhotoVideoPickerDelegate

- (void)picker:(PHPickerViewController*)picker didFinishPicking:(NSArray<PHPickerResult*>*)results
{
    [picker dismissViewControllerAnimated:YES completion:nil];

    PHPickerResult* result = results.firstObject;
    if (result == nil)
    {
        [self finishWithURL:juce::URL()];
        return;
    }

    NSItemProvider* provider = result.itemProvider;
    NSString* identifier = gmBestVideoTypeIdentifier(provider);
    if (![provider hasItemConformingToTypeIdentifier:identifier])
    {
        [self finishWithURL:juce::URL()];
        return;
    }

    __weak GOODMETERPhotoVideoPickerDelegate* weakSelf = self;
    [provider loadFileRepresentationForTypeIdentifier:identifier
                                    completionHandler:^(NSURL* url, NSError*)
    {
        GOODMETERPhotoVideoPickerDelegate* strongSelf = weakSelf;
        if (strongSelf == nil || url == nil)
        {
            dispatch_async(dispatch_get_main_queue(), ^
            {
                [strongSelf finishWithURL:juce::URL()];
            });
            return;
        }

        NSURL* destination = gmUniqueDocumentsUrlForProvider(provider, url);
        if (destination == nil)
        {
            dispatch_async(dispatch_get_main_queue(), ^
            {
                [strongSelf finishWithURL:juce::URL()];
            });
            return;
        }

        [[NSFileManager defaultManager] removeItemAtURL:destination error:nil];
        NSError* copyError = nil;
        BOOL copied = [[NSFileManager defaultManager] copyItemAtURL:url
                                                              toURL:destination
                                                              error:&copyError];

        juce::URL pickedUrl;
        if (copied && copyError == nil)
            pickedUrl = juce::URL(juce::File(juce::String(destination.path.UTF8String)));

        dispatch_async(dispatch_get_main_queue(), ^
        {
            [strongSelf finishWithURL:pickedUrl];
        });
    }];
}

- (void)finishWithURL:(juce::URL)url
{
    auto callback = completion;
    completion = nullptr;

    if (callback != nullptr)
        callback(url);

    [gmActivePhotoVideoDelegates() removeObject:self];
}

@end
#endif

namespace GoodMeterIOSPhotoVideoPicker
{
#if JUCE_IOS
bool isAvailable()
{
    if (@available(iOS 14.0, *))
        return true;

    return false;
}

void open(juce::Component*, Completion completion)
{
    if (completion == nullptr)
        return;

    dispatch_async(dispatch_get_main_queue(), ^
    {
        if (!isAvailable())
        {
            completion(juce::URL());
            return;
        }

        UIViewController* presenter = gmFindPresenter();
        if (presenter == nil)
        {
            completion(juce::URL());
            return;
        }

        PHPickerConfiguration* config = [[PHPickerConfiguration alloc] init];
        config.selectionLimit = 1;
        config.filter = [PHPickerFilter videosFilter];

        PHPickerViewController* picker = [[PHPickerViewController alloc] initWithConfiguration:config];
        GOODMETERPhotoVideoPickerDelegate* delegate = [[GOODMETERPhotoVideoPickerDelegate alloc] init];
        delegate->completion = completion;
        delegate.picker = picker;
        picker.delegate = delegate;
        [gmActivePhotoVideoDelegates() addObject:delegate];

        [presenter presentViewController:picker animated:YES completion:nil];
    });
}
#else
bool isAvailable() { return false; }
void open(juce::Component*, Completion completion)
{
    if (completion != nullptr)
        completion(juce::URL());
}
#endif
}
