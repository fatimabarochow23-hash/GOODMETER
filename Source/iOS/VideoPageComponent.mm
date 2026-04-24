/*
  ==============================================================================
    VideoPageComponent.mm
    GOODMETER iOS - Page 5: Native AVPlayer-backed video page
  ==============================================================================
*/

#include "VideoPageComponent.h"

#if JUCE_IOS

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

@interface GOODMETERPlayerSurfaceView : UIView
@end

@implementation GOODMETERPlayerSurfaceView
+ (Class)layerClass { return [AVPlayerLayer class]; }
- (AVPlayerLayer*) playerLayer { return (AVPlayerLayer*) self.layer; }
@end

@interface GOODMETERVideoView : UIView
@property (nonatomic, copy) void (^tapHandler)(void);
@property (nonatomic, strong) GOODMETERPlayerSurfaceView* surfaceView;
@property (nonatomic) CGSize sourceDisplaySize;
@property (nonatomic) BOOL forceLandscapePresentation;
@property (nonatomic) CGFloat forcedRotationAngle;
@property (nonatomic) BOOL maximizeFill;
@property (nonatomic) BOOL sourceWasPortrait;
- (AVPlayerLayer*) playerLayer;
- (void)configureForSourceDisplaySize:(CGSize)size
             forceLandscapePresentation:(BOOL)forceLandscape
                          rotationAngle:(CGFloat)angle
                           maximizeFill:(BOOL)maximizeFill
                       sourceWasPortrait:(BOOL)sourceWasPortrait;
@end

@implementation GOODMETERVideoView
- (instancetype) initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    if (self != nil)
    {
        self.userInteractionEnabled = YES;
        self.clipsToBounds = YES;
        self.backgroundColor = UIColor.blackColor;
        _surfaceView = [[GOODMETERPlayerSurfaceView alloc] initWithFrame:CGRectZero];
        _surfaceView.backgroundColor = UIColor.blackColor;
        _surfaceView.playerLayer.videoGravity = AVLayerVideoGravityResizeAspect;
        [self addSubview:_surfaceView];

        UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap)];
        [self addGestureRecognizer:tap];
    }

    return self;
}

- (void) handleTap
{
    if (self.tapHandler != nil)
        self.tapHandler();
}

- (AVPlayerLayer*) playerLayer
{
    return self.surfaceView.playerLayer;
}

- (void)configureForSourceDisplaySize:(CGSize)size
             forceLandscapePresentation:(BOOL)forceLandscape
                          rotationAngle:(CGFloat)angle
                           maximizeFill:(BOOL)shouldMaximizeFill
                       sourceWasPortrait:(BOOL)wasPortrait
{
    self.sourceDisplaySize = size;
    self.forceLandscapePresentation = forceLandscape;
    self.forcedRotationAngle = angle;
    self.maximizeFill = shouldMaximizeFill;
    self.sourceWasPortrait = wasPortrait;
    [self setNeedsLayout];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    auto bounds = self.bounds;
    CGSize sourceSize = self.sourceDisplaySize;
    if (sourceSize.width <= 0.0 || sourceSize.height <= 0.0)
        sourceSize = bounds.size;

    const BOOL shouldRotate = self.forceLandscapePresentation;
    CGSize targetSize = shouldRotate ? CGSizeMake(sourceSize.height, sourceSize.width) : sourceSize;

    CGRect fitted = AVMakeRectWithAspectRatioInsideRect(targetSize, bounds);

    if (shouldRotate)
    {
        self.surfaceView.bounds = CGRectMake(0.0, 0.0, fitted.size.height, fitted.size.width);
        self.surfaceView.center = CGPointMake(CGRectGetMidX(fitted), CGRectGetMidY(fitted));
        self.surfaceView.transform = CGAffineTransformMakeRotation(self.forcedRotationAngle);
    }
    else
    {
        if (self.maximizeFill && self.sourceWasPortrait)
        {
            CGFloat width = bounds.size.width;
            CGFloat height = width * (sourceSize.height / juce::jmax(1.0, (double) sourceSize.width));
            if (height > bounds.size.height)
            {
                height = bounds.size.height;
                width = height * (sourceSize.width / juce::jmax(1.0, (double) sourceSize.height));
            }

            CGRect portraitRect = CGRectMake((bounds.size.width - width) * 0.5f,
                                             (bounds.size.height - height) * 0.5f,
                                             width,
                                             height);
            self.surfaceView.transform = CGAffineTransformIdentity;
            self.surfaceView.frame = portraitRect;
        }
        else
        {
            self.surfaceView.transform = CGAffineTransformIdentity;
            self.surfaceView.frame = fitted;
        }
    }
}
@end

class VideoPageComponent::NativeVideoPlayer
{
public:
    NativeVideoPlayer()
    {
        videoView = [[GOODMETERVideoView alloc] initWithFrame:CGRectZero];
        videoView.backgroundColor = UIColor.whiteColor;
        videoView.playerLayer.videoGravity = AVLayerVideoGravityResizeAspect;
        videoView.tapHandler = ^{
            if (onVideoTapped != nullptr)
                onVideoTapped();
        };
        host.setView(videoView);
        setThemeDark(false);
    }

    ~NativeVideoPlayer()
    {
        clear();
        host.setView(nullptr);
        videoView = nil;
    }

    juce::UIViewComponent& getHost() { return host; }

    bool load(const juce::File& file, juce::String& error)
    {
        clear();

        auto nsPath = [NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()];
        auto url = [NSURL fileURLWithPath:nsPath];
        auto asset = [AVURLAsset URLAssetWithURL:url options:nil];
        auto videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
        if ([videoTracks count] == 0)
        {
            error = "No video track found";
            return false;
        }

        AVAssetTrack* videoTrack = [videoTracks firstObject];

        playerItem = [AVPlayerItem playerItemWithAsset:asset];
        player = [AVPlayer playerWithPlayerItem:playerItem];

        if (player == nil)
        {
            error = "AVPlayer could not open this file";
            playerItem = nil;
            return false;
        }

        player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
        player.automaticallyWaitsToMinimizeStalling = NO;
        playerItem.preferredForwardBufferDuration = 0.0;
        videoView.playerLayer.player = player;

        durationSeconds = CMTimeGetSeconds(asset.duration);
        if (!std::isfinite(durationSeconds) || durationSeconds < 0.0)
            durationSeconds = 0.0;

        CGSize naturalSize = videoTrack.naturalSize;
        CGAffineTransform preferred = videoTrack.preferredTransform;
        CGRect transformedRect = CGRectApplyAffineTransform(CGRectMake(0.0, 0.0, naturalSize.width, naturalSize.height), preferred);
        CGSize displaySize = CGSizeMake(std::fabs(transformedRect.size.width), std::fabs(transformedRect.size.height));
        if (displaySize.width <= 0.0 || displaySize.height <= 0.0)
            displaySize = CGSizeMake(std::fabs(naturalSize.width), std::fabs(naturalSize.height));

        const bool isPortraitSource = displaySize.height > displaySize.width;
        sourceWasPortrait = isPortraitSource;
        presentationSize = isPortraitSource ? CGSizeMake(displaySize.height, displaySize.width)
                                            : displaySize;
        [videoView configureForSourceDisplaySize:displaySize
                        forceLandscapePresentation:isPortraitSource
                                     rotationAngle:(CGFloat) -juce::MathConstants<double>::halfPi
                                      maximizeFill:isPortraitSource
                                  sourceWasPortrait:isPortraitSource];

        currentPath = file.getFullPathName();
        return true;
    }

    void clear()
    {
        lifeToken->store(false, std::memory_order_relaxed);
        lifeToken = std::make_shared<std::atomic<bool>>(true);
        isSeekInProgress = false;
        hasChasePosition = false;
        chaseNeedsExact = false;
        resumeAfterSeek = false;
        chasePositionSeconds = 0.0;

        if (player != nil)
            [player pause];

        if (videoView != nil)
        {
            videoView.playerLayer.player = nil;
            [videoView configureForSourceDisplaySize:CGSizeZero
                            forceLandscapePresentation:NO
                                         rotationAngle:0.0f
                                          maximizeFill:NO
                                      sourceWasPortrait:NO];
        }

        player = nil;
        playerItem = nil;
        durationSeconds = 0.0;
        currentPath.clear();
        presentationSize = CGSizeZero;
        sourceWasPortrait = false;
    }

    void play()
    {
        if (player != nil)
            [player play];
    }

    void pause()
    {
        if (player != nil)
        {
            player.rate = 0.0f;
            [player pause];
        }
    }

    bool isPlaying() const
    {
        return player != nil && player.rate > 0.001f;
    }

    void setPosition(double seconds, bool precise = true)
    {
        if (player == nil)
            return;

        lifeToken->store(false, std::memory_order_relaxed);
        lifeToken = std::make_shared<std::atomic<bool>>(true);
        isSeekInProgress = false;
        hasChasePosition = false;
        chaseNeedsExact = false;
        resumeAfterSeek = false;
        chasePositionSeconds = juce::jlimit(0.0, getDuration(), seconds);

        auto target = CMTimeMakeWithSeconds(seconds, 600);
        const auto tolerance = precise ? kCMTimeZero : CMTimeMakeWithSeconds(0.08, 600);
        [player seekToTime:target toleranceBefore:tolerance toleranceAfter:tolerance];
    }

    void queueSmoothSeek(double seconds, bool preciseFinal)
    {
        if (player == nil)
            return;

        const bool wasPlaying = isPlaying();
        chasePositionSeconds = juce::jlimit(0.0, getDuration(), seconds);
        hasChasePosition = true;
        // While actively playing we prioritize visual response over frame-perfect refine.
        // Exact refine is still used when the user is paused/stopped at a position.
        chaseNeedsExact = chaseNeedsExact || (preciseFinal && !wasPlaying);
        resumeAfterSeek = resumeAfterSeek || wasPlaying;

        if (!isSeekInProgress)
            performQueuedSeek(false);
    }

    void seekPreview(double seconds)
    {
        if (player == nil)
            return;

        lifeToken->store(false, std::memory_order_relaxed);
        lifeToken = std::make_shared<std::atomic<bool>>(true);
        isSeekInProgress = false;
        hasChasePosition = false;
        chaseNeedsExact = false;
        resumeAfterSeek = false;
        chasePositionSeconds = juce::jlimit(0.0, getDuration(), seconds);

        if (playerItem != nil)
            [playerItem cancelPendingSeeks];

        auto target = CMTimeMakeWithSeconds(chasePositionSeconds, 600);
        auto tolerance = CMTimeMakeWithSeconds(0.28, 600);
        [player seekToTime:target toleranceBefore:tolerance toleranceAfter:tolerance];
    }

    bool hasPendingSeek() const
    {
        return isSeekInProgress || hasChasePosition;
    }

    double getPosition() const
    {
        if (player == nil)
            return 0.0;

        auto seconds = CMTimeGetSeconds(player.currentTime);
        if (!std::isfinite(seconds) || seconds < 0.0)
            return 0.0;

        return seconds;
    }

    double getDuration() const
    {
        if (playerItem != nil)
        {
            auto itemDuration = CMTimeGetSeconds(playerItem.duration);
            if (std::isfinite(itemDuration) && itemDuration > 0.0)
                return itemDuration;
        }

        return durationSeconds;
    }

    void setVolume(float volume)
    {
        if (player != nil)
            player.volume = volume;
    }

    bool reloadPreservingState(double targetSeconds, bool shouldResume, juce::String& error)
    {
        if (currentPath.isEmpty())
        {
            error = "No current video to reload";
            return false;
        }

        const auto path = currentPath;
        const auto volume = player != nil ? player.volume : 1.0f;
        const auto target = juce::jlimit(0.0, getDuration(), targetSeconds);

        if (!load(juce::File(path), error))
            return false;

        setThemeDark(isThemeDark);
        setVolume(volume);
        setPosition(target, false);

        if (shouldResume)
            play();
        else
            pause();

        return true;
    }

    void setThemeDark(bool dark)
    {
        isThemeDark = dark;
        if (videoView == nil)
            return;

        UIColor* bg = dark ? UIColor.blackColor : UIColor.whiteColor;
        videoView.backgroundColor = bg;
        videoView.surfaceView.backgroundColor = bg;
    }

    juce::String getCurrentPath() const { return currentPath; }
    juce::Point<float> getPresentationSize() const
    {
        return { static_cast<float>(presentationSize.width), static_cast<float>(presentationSize.height) };
    }
    bool isSourcePortrait() const { return sourceWasPortrait; }

    std::function<void()> onVideoTapped;

private:
    void performQueuedSeek(bool exactPhase)
    {
        if (player == nil || !hasChasePosition)
        {
            isSeekInProgress = false;
            return;
        }

        isSeekInProgress = true;
        const double seekTimeSeconds = chasePositionSeconds;
        const bool shouldRefine = chaseNeedsExact;
        auto target = CMTimeMakeWithSeconds(seekTimeSeconds, 600);
        const auto tolerance = exactPhase ? kCMTimeZero : CMTimeMakeWithSeconds(0.12, 600);
        auto life = lifeToken;

        [player seekToTime:target toleranceBefore:tolerance toleranceAfter:tolerance completionHandler:^(BOOL finished)
        {
            if (!life->load(std::memory_order_relaxed))
                return;

            this->isSeekInProgress = false;

            if (this->player == nil)
                return;

            if (!finished)
            {
                if (this->hasChasePosition)
                    this->performQueuedSeek(false);
                return;
            }

            const bool targetChanged = std::abs(this->chasePositionSeconds - seekTimeSeconds) > 0.01;

            if (targetChanged)
            {
                this->performQueuedSeek(false);
                return;
            }

            if (!exactPhase && shouldRefine && this->hasChasePosition)
            {
                this->chaseNeedsExact = false;
                this->performQueuedSeek(true);
                return;
            }

            this->hasChasePosition = false;
            const bool shouldResume = this->resumeAfterSeek;
            this->resumeAfterSeek = false;
            if (shouldResume && this->player != nil)
                [this->player play];
        }];
    }

    juce::UIViewComponent host;
    GOODMETERVideoView* videoView = nil;
    AVPlayer* player = nil;
    AVPlayerItem* playerItem = nil;
    double durationSeconds = 0.0;
    juce::String currentPath;
    CGSize presentationSize = CGSizeZero;
    bool sourceWasPortrait = false;
    bool isSeekInProgress = false;
    bool hasChasePosition = false;
    bool chaseNeedsExact = false;
    bool resumeAfterSeek = false;
    double chasePositionSeconds = 0.0;
    std::shared_ptr<std::atomic<bool>> lifeToken = std::make_shared<std::atomic<bool>>(true);
    bool isThemeDark = false;
};

#else

class VideoPageComponent::NativeVideoPlayer
{
public:
    juce::Component& getHost() { return host; }
    bool load(const juce::File&, juce::String& error) { error = "Video playback is only enabled on iOS"; return false; }
    void clear() {}
    void play() {}
    void pause() {}
    bool isPlaying() const { return false; }
    void setPosition(double, bool = true) {}
    void seekPreview(double) {}
    double getPosition() const { return 0.0; }
    double getDuration() const { return 0.0; }
    void setVolume(float) {}
    juce::String getCurrentPath() const { return {}; }

private:
    juce::Component host;
};

#endif

VideoPageComponent::VideoPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
    : processor(proc), audioEngine(engine)
{
#if MARATHON_ART_STYLE
    bgCanvas = std::make_unique<DotMatrixCanvas>(21, 24);
    randomizeBackground();
#endif

    nativePlayer = std::make_unique<NativeVideoPlayer>();
    addAndMakeVisible(nativePlayer->getHost());
    tapOverlay.onTap = [this]()
    {
        if (hasVideoLoaded)
            setDrawerOpen(!drawerOpen);
    };
    tapOverlay.onDoubleTap = [this]()
    {
        if (isMarkerModeActive != nullptr
            && isMarkerModeActive()
            && addMarkerAtCurrentPosition != nullptr)
        {
            addMarkerAtCurrentPosition();
        }
    };
    addAndMakeVisible(tapOverlay);
    nativePlayer->onVideoTapped = [this]()
    {
        if (hasVideoLoaded)
            setDrawerOpen(!drawerOpen);
    };

    topMeterSwipe.onSwipe = [this](int direction) { cycleMeterSlot(true, direction); };
    bottomMeterSwipe.onSwipe = [this](int direction) { cycleMeterSlot(false, direction); };
    topMeterSwipe.onDoubleTap = [this]()
    {
        if (isMarkerModeActive != nullptr
            && isMarkerModeActive()
            && addMarkerAtCurrentPosition != nullptr)
        {
            addMarkerAtCurrentPosition();
        }
    };
    bottomMeterSwipe.onDoubleTap = topMeterSwipe.onDoubleTap;
    addAndMakeVisible(topMeterSwipe);
    addAndMakeVisible(bottomMeterSwipe);

    fileNameLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    fileNameLabel.setJustificationType(juce::Justification::centred);
    fileNameLabel.setMinimumHorizontalScale(0.72f);
    fileNameLabel.setText("No video loaded", juce::dontSendNotification);
    addAndMakeVisible(fileNameLabel);

    hintLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    hintLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
    hintLabel.setJustificationType(juce::Justification::centred);
    hintLabel.setText("Import a video on Page 1 or load one from History", juce::dontSendNotification);
    addAndMakeVisible(hintLabel);

    currentTimeLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    currentTimeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
    currentTimeLabel.setJustificationType(juce::Justification::centredRight);
    currentTimeLabel.setText("0:00", juce::dontSendNotification);
    addAndMakeVisible(currentTimeLabel);

    remainingTimeLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    remainingTimeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
    remainingTimeLabel.setJustificationType(juce::Justification::centredLeft);
    remainingTimeLabel.setText("-0:00", juce::dontSendNotification);
    addAndMakeVisible(remainingTimeLabel);

    progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    progressSlider.setRange(0.0, 1.0, 0.001);
    progressSlider.setColour(juce::Slider::thumbColourId, GoodMeterLookAndFeel::textMain);
    progressSlider.setColour(juce::Slider::trackColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.15f));
    progressSlider.setColour(juce::Slider::backgroundColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
    progressSlider.onDragStart = [this]()
    {
        progressScrubDragging = true;
        progressSeekPending = false;
        pendingProgressSeekSeconds = 0.0;
        lastProgressSeekCommitMs = 0;
    };
    progressSlider.onValueChange = [this]()
    {
        if (progressSlider.isMouseButtonDown())
        {
            auto duration = getDurationSeconds();
            if (duration > 0.01)
            {
                auto target = progressSlider.getValue() * duration;
                nativePlayer->seekPreview(target);
                queueProgressSeek(target);
                currentTimeLabel.setText(fmtTime(target), juce::dontSendNotification);
                remainingTimeLabel.setText("-" + fmtTime(juce::jmax(0.0, duration - target)),
                                           juce::dontSendNotification);
            }
        }
    };
    progressSlider.onDragEnd = [this]()
    {
        progressScrubDragging = false;
        if (hasVideoLoaded && nativePlayer != nullptr)
        {
            auto target = juce::jlimit(0.0, getDurationSeconds(), progressSlider.getValue() * getDurationSeconds());
            queueProgressSeek(target);
            nativePlayer->queueSmoothSeek(target, true);
        }
        flushQueuedProgressSeek(false);
    };
    addAndMakeVisible(progressSlider);

    auto makeTransportBtn = [this](juce::TextButton& btn, const juce::String& text)
    {
        btn.setButtonText(text);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId, GoodMeterLookAndFeel::textMain);
        addAndMakeVisible(btn);
    };

    makeTransportBtn(rewindBtn, "|<<");
    makeTransportBtn(skipBackBtn, "<<");
    makeTransportBtn(skipFwdBtn, ">>");
    makeTransportBtn(stopBtn, ">>|");
    addAndMakeVisible(playPauseBtn);

    rewindBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        nativePlayer->queueSmoothSeek(0.0, true);
        queueProgressSeek(0.0);
    };
    skipBackBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        auto pos = juce::jmax(0.0, nativePlayer->getPosition() - 5.0);
        nativePlayer->queueSmoothSeek(pos, true);
        queueProgressSeek(pos);
    };
    playPauseBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        const bool shouldPlay = !nativePlayer->isPlaying();
        setPlayButtonVisualState(shouldPlay);

        juce::Component::SafePointer<VideoPageComponent> safeThis(this);
        juce::Timer::callAfterDelay(16, [safeThis, shouldPlay]()
        {
            if (safeThis == nullptr || !safeThis->hasVideoLoaded)
                return;

            if (shouldPlay)
                safeThis->playTransport();
            else
                safeThis->pauseTransport();
        });
    };
    skipFwdBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        auto duration = getDurationSeconds();
        auto pos = juce::jmin(duration, nativePlayer->getPosition() + 5.0);
        nativePlayer->queueSmoothSeek(pos, true);
        queueProgressSeek(pos);
    };
    stopBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        auto duration = getDurationSeconds();
        auto target = juce::jmax(0.0, duration - 0.05);
        nativePlayer->queueSmoothSeek(target, true);
        queueProgressSeek(target);
    };

    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(audioEngine.getVolume());
    volumeSlider.setColour(juce::Slider::thumbColourId, GoodMeterLookAndFeel::textMain);
    volumeSlider.setColour(juce::Slider::trackColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.15f));
    volumeSlider.setColour(juce::Slider::backgroundColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
    volumeSlider.onValueChange = [this]()
    {
        const auto volume = (float) volumeSlider.getValue();
        audioEngine.setVolume(volume);
        nativePlayer->setVolume(syncedAudioLoaded ? 0.0f : volume);
    };
    addAndMakeVisible(volumeSlider);

    rebuildMeterSlot(true);
   rebuildMeterSlot(false);

    startTimerHz(30);
    setDarkTheme(isDarkTheme);
}

VideoPageComponent::~VideoPageComponent()
{
    stopTimer();
    clearVideo();
}

void VideoPageComponent::setDarkTheme(bool dark)
{
    isDarkTheme = dark;
    GoodMeterLookAndFeel::setEditorialPopupMode(true, dark);
    if (nativePlayer != nullptr)
        nativePlayer->setThemeDark(isDarkTheme);

    fileNameLabel.setColour(juce::Label::textColourId,
                            isDarkTheme ? juce::Colour(0xFFF3EEE4).withAlpha(0.96f)
                                        : GoodMeterLookAndFeel::textMain.withAlpha(0.92f));
    hintLabel.setColour(juce::Label::textColourId,
                        isDarkTheme ? juce::Colour(0xFFF3EEE4).withAlpha(0.60f)
                                    : GoodMeterLookAndFeel::textMuted);

    applyMeterCardTheme(topMeterCard.get());
    applyMeterCardTheme(bottomMeterCard.get());
    updateDrawerThemeColors();
    resized();
    repaint();
}

void VideoPageComponent::applyEmbeddedMeterTheme(juce::Component* content)
{
    if (content == nullptr)
        return;

    if (auto* meter = dynamic_cast<LevelsMeterComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<VUMeterComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<Band3Component*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<SpectrumAnalyzerComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<PhaseCorrelationComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<StereoImageComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<SpectrogramComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
    else if (auto* meter = dynamic_cast<PsrMeterComponent*>(content))
        meter->setMarathonDarkStyle(isDarkTheme);
}

void VideoPageComponent::applyMeterCardTheme(MeterCardComponent* card)
{
    if (card == nullptr)
        return;

    card->isDarkTheme = isDarkTheme;
    card->useEditorialDarkStyle = isDarkTheme;
    card->useEditorialLightStyle = !isDarkTheme;
    card->useMonospacedTitleFont = true;
    applyEmbeddedMeterTheme(card->getContentComponent());
    card->resized();
    card->repaint();
}

void VideoPageComponent::updateDrawerThemeColors()
{
    const auto transportTextCol = isDarkTheme ? juce::Colour(0xFFF3EEE4)
                                              : GoodMeterLookAndFeel::textMain;
    const auto transportBgCol = isDarkTheme ? juce::Colour(0xFF1E2230)
                                            : GoodMeterLookAndFeel::bgMain;
    const auto thumbCol = transportTextCol;
    const auto trackCol = transportTextCol.withAlpha(isDarkTheme ? 0.28f : 0.15f);
    const auto railCol = transportTextCol.withAlpha(isDarkTheme ? 0.10f : 0.08f);

    currentTimeLabel.setColour(juce::Label::textColourId, transportTextCol.withAlpha(isDarkTheme ? 0.96f : 0.78f));
    remainingTimeLabel.setColour(juce::Label::textColourId, transportTextCol.withAlpha(isDarkTheme ? 0.96f : 0.78f));

    rewindBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);
    skipBackBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);
    skipFwdBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);
    stopBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);

    progressSlider.setColour(juce::Slider::thumbColourId, thumbCol);
    progressSlider.setColour(juce::Slider::trackColourId, trackCol);
    progressSlider.setColour(juce::Slider::backgroundColourId, railCol);
    volumeSlider.setColour(juce::Slider::thumbColourId, thumbCol);
    volumeSlider.setColour(juce::Slider::trackColourId, trackCol);
    volumeSlider.setColour(juce::Slider::backgroundColourId, railCol);
    playPauseBtn.setColours(isDarkTheme ? juce::Colour(0xFFF3EEE4) : transportTextCol,
                            isDarkTheme ? juce::Colour(0xFF1E2230) : transportBgCol);
}

bool VideoPageComponent::shouldConsumeHorizontalSwipe(juce::Point<float> point) const
{
    if (getWidth() > getHeight())
        return false;

    const auto rounded = point.roundToInt();
    return topMeterBounds.contains(rounded) || bottomMeterBounds.contains(rounded);
}

VideoPageComponent::EmbeddedMeterKind VideoPageComponent::wrapMeterKind(int rawIndex) const
{
    static constexpr int totalKinds = 8;
    auto wrapped = rawIndex % totalKinds;
    if (wrapped < 0)
        wrapped += totalKinds;
    return static_cast<EmbeddedMeterKind>(wrapped);
}

void VideoPageComponent::cycleMeterSlot(bool topSlot, int direction)
{
    auto& kind = topSlot ? topMeterKind : bottomMeterKind;
    kind = wrapMeterKind(static_cast<int>(kind) + direction);

    if (topSlot)
        topMeterAlpha = 0.18f;
    else
        bottomMeterAlpha = 0.18f;

    rebuildMeterSlot(topSlot);
    resized();
    repaint();
}

void VideoPageComponent::rebuildMeterSlot(bool topSlot)
{
    auto& slotCard = topSlot ? topMeterCard : bottomMeterCard;
    auto& kind = topSlot ? topMeterKind : bottomMeterKind;
    auto& levelsPtr = topSlot ? topLevelsMeter : bottomLevelsMeter;
    auto& vuPtr = topSlot ? topVuMeter : bottomVuMeter;
    auto& phasePtr = topSlot ? topPhaseMeter : bottomPhaseMeter;

    levelsPtr = nullptr;
    vuPtr = nullptr;
    phasePtr = nullptr;

    if (slotCard != nullptr)
        removeChildComponent(slotCard.get());

    auto createCard = [&](const juce::String& title, juce::Colour accent)
    {
        auto card = std::make_unique<MeterCardComponent>(title, accent, true);
        card->isMiniMode = true;
        card->mobileAllowHeaderToggle = false;
        card->setMobileListMode(true);
        card->setExpanded(true, false);
        card->setInterceptsMouseClicks(false, false);
        return card;
    };

    switch (kind)
    {
        case EmbeddedMeterKind::levels:
        {
            auto card = createCard("LEVELS", GoodMeterLookAndFeel::accentPink);
            auto* meter = new LevelsMeterComponent(processor);
            meter->setupTargetMenu();
            card->setContentComponent(std::unique_ptr<juce::Component>(meter));
            card->setHeaderWidget(&meter->getTargetMenu());
            levelsPtr = meter;
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::vu:
        {
            auto card = createCard("VU METER", GoodMeterLookAndFeel::accentYellow);
            auto* meter = new VUMeterComponent();
            card->setContentComponent(std::unique_ptr<juce::Component>(meter));
            vuPtr = meter;
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::band3:
        {
            auto card = createCard("3-BAND", GoodMeterLookAndFeel::accentPurple);
            card->setContentComponent(std::make_unique<Band3Component>(processor));
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::spectrum:
        {
            auto card = createCard("SPECTRUM", GoodMeterLookAndFeel::accentCyan);
            card->setContentComponent(std::make_unique<SpectrumAnalyzerComponent>(processor));
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::phase:
        {
            auto card = createCard("PHASE", GoodMeterLookAndFeel::accentBlue);
            auto* meter = new PhaseCorrelationComponent();
            card->setContentComponent(std::unique_ptr<juce::Component>(meter));
            phasePtr = meter;
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::stereo:
        {
            auto card = createCard("STEREO", GoodMeterLookAndFeel::accentSoftPink);
            card->setContentComponent(std::make_unique<StereoImageComponent>(processor));
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::spectrogram:
        {
            auto card = createCard("SPECTROGRAM", GoodMeterLookAndFeel::accentYellow);
            card->setContentComponent(std::make_unique<SpectrogramComponent>(processor));
            slotCard = std::move(card);
            break;
        }
        case EmbeddedMeterKind::psr:
        {
            auto card = createCard("PSR", juce::Colour(0xFF20C997));
            card->setContentComponent(std::make_unique<PsrMeterComponent>(processor));
            slotCard = std::move(card);
            break;
        }
    }

    if (slotCard != nullptr)
    {
        addAndMakeVisible(slotCard.get());
        slotCard->setAlpha(topSlot ? topMeterAlpha : bottomMeterAlpha);
        applyMeterCardTheme(slotCard.get());
        if (topSlot)
            topMeterSwipe.toFront(false);
        else
            bottomMeterSwipe.toFront(false);
    }
}

juce::Rectangle<int> VideoPageComponent::getPresentedVideoBounds(juce::Rectangle<int> hostBounds) const
{
    const auto presentationSize = nativePlayer->getPresentationSize();
    if (presentationSize.x <= 1.0f || presentationSize.y <= 1.0f || hostBounds.isEmpty())
        return hostBounds;

    const float aspect = presentationSize.x / juce::jmax(1.0f, presentationSize.y);
    float fittedWidth = static_cast<float>(hostBounds.getWidth());
    float fittedHeight = fittedWidth / aspect;

    if (fittedHeight > static_cast<float>(hostBounds.getHeight()))
    {
        fittedHeight = static_cast<float>(hostBounds.getHeight());
        fittedWidth = fittedHeight * aspect;
    }

    return juce::Rectangle<int>(juce::roundToInt(fittedWidth),
                                juce::roundToInt(fittedHeight))
        .withCentre(hostBounds.getCentre());
}

juce::File VideoPageComponent::getExpectedAudioFileForVideo(const juce::File& videoFile) const
{
    auto baseName = juce::URL::removeEscapeChars(videoFile.getFileNameWithoutExtension());
    if (baseName.isEmpty())
        baseName = "ImportedVideo";

    return videoFile.getParentDirectory().getChildFile("Extract_" + baseName + ".wav");
}

bool VideoPageComponent::attachSyncedAudioIfAvailable()
{
    if (!hasVideoLoaded || currentFilePath.isEmpty())
        return false;

    auto videoFile = juce::File(currentFilePath);
    auto extractedAudio = getExpectedAudioFileForVideo(videoFile);
    if (!extractedAudio.existsAsFile())
        return false;

    const auto extractedPath = extractedAudio.getFullPathName();
    if (audioEngine.getCurrentFilePath() != extractedPath
        && !audioEngine.loadFile(extractedAudio))
    {
        syncedAudioLoaded = false;
        syncedAudioPath.clear();
        nativePlayer->setVolume((float) volumeSlider.getValue());
        return false;
    }

    syncedAudioLoaded = true;
    syncedAudioPath = extractedPath;
    audioEngine.setVolume((float) volumeSlider.getValue());
    nativePlayer->setVolume(0.0f);
    return true;
}

void VideoPageComponent::syncAudioTransportToPosition(double positionSeconds, bool preservePlayingState)
{
    if (!syncedAudioLoaded && !attachSyncedAudioIfAvailable())
        return;

    const bool shouldResume = preservePlayingState && nativePlayer->isPlaying();
    audioEngine.pause();
    audioEngine.seek(positionSeconds);
    if (shouldResume)
        audioEngine.play();
}

void VideoPageComponent::paint(juce::Graphics& g)
{
    bool hasVideo = (nativePlayer != nullptr && !currentFilePath.isEmpty());

    if (!hasVideo)
    {
#if MARATHON_ART_STYLE
        g.fillAll(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);

        juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain);
        int gridH = bgCanvas->getHeight();
        int gridW = bgCanvas->getWidth();
        auto bounds = getLocalBounds().toFloat();
        float cellW = bounds.getWidth() / gridW;
        float cellH = bounds.getHeight() / gridH;

        for (int y = 0; y < gridH; ++y)
        {
            for (int x = 0; x < gridW; ++x)
            {
                auto cell = bgCanvas->getCell(x, y);
                float px = x * cellW;
                float py = y * cellH;

                auto symbolColour = isDarkTheme
                    ? cell.color.withMultipliedAlpha(cell.brightness)
                    : GoodMeterLookAndFeel::textMain.withAlpha(0.055f + cell.brightness * 0.125f);
                g.setColour(symbolColour);
                g.setFont(monoFont);
                juce::String str = juce::String::charToString(cell.symbol);
                g.drawText(str, (int)px, (int)py, (int)cellW, (int)cellH,
                          juce::Justification::centred, false);
            }
        }
#else
        g.fillAll(isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain);
#endif
        return;
    }

    g.fillAll(isDarkTheme ? juce::Colour(0xFF07080B) : GoodMeterLookAndFeel::bgMain);

#if MARATHON_ART_STYLE
    if (bgCanvas != nullptr)
    {
        juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain);
        int gridH = bgCanvas->getHeight();
        int gridW = bgCanvas->getWidth();
        auto bounds = getLocalBounds().toFloat();
        float cellW = bounds.getWidth() / gridW;
        float cellH = bounds.getHeight() / gridH;

        for (int y = 0; y < gridH; ++y)
        {
            for (int x = 0; x < gridW; ++x)
            {
                auto cell = bgCanvas->getCell(x, y);
                if (cell.symbol == U' ')
                    continue;

                auto symbolColour = isDarkTheme
                    ? cell.color.withMultipliedAlpha(cell.brightness)
                    : GoodMeterLookAndFeel::textMain.withAlpha(0.045f + cell.brightness * 0.110f);
                g.setColour(symbolColour);
                g.setFont(monoFont);
                juce::String str = juce::String::charToString(cell.symbol);
                g.drawText(str,
                           juce::roundToInt(x * cellW),
                           juce::roundToInt(y * cellH),
                           juce::roundToInt(cellW),
                           juce::roundToInt(cellH),
                           juce::Justification::centred,
                           false);
            }
        }
    }
#endif

    auto videoPlate = videoBounds.toFloat();
    auto videoPlateFill = isDarkTheme ? juce::Colours::black.withAlpha(0.92f)
                                      : juce::Colour(0xFFFFFFFF).withAlpha(0.72f);
    auto videoPlateOutline = isDarkTheme ? juce::Colours::white.withAlpha(0.10f)
                                         : GoodMeterLookAndFeel::textMain.withAlpha(0.14f);
    auto videoPlateShadow = isDarkTheme ? juce::Colours::black.withAlpha(0.16f)
                                        : juce::Colours::black.withAlpha(0.04f);

    g.setColour(videoPlateShadow);
    g.fillRoundedRectangle(videoPlate.translated(0.0f, 2.0f), 18.0f);
    g.setColour(videoPlateFill);
    g.fillRoundedRectangle(videoPlate, 18.0f);
    g.setColour(videoPlateOutline);
    g.drawRoundedRectangle(videoPlate.reduced(0.5f), 18.0f, 1.0f);

    auto drawerArea = getDrawerBounds().toFloat();

    if (drawerOpen)
    {
        auto overlay = drawerArea;
        g.setColour(isDarkTheme ? juce::Colour(0xFF121620).withAlpha(0.18f)
                                : juce::Colours::white.withAlpha(0.20f));
        g.fillRoundedRectangle(overlay, 16.0f);
        g.setColour(isDarkTheme ? juce::Colours::white.withAlpha(0.08f)
                                : GoodMeterLookAndFeel::textMain.withAlpha(0.07f));
        g.drawRoundedRectangle(overlay.reduced(0.5f), 16.0f, 1.0f);
    }
}

void VideoPageComponent::paintOverChildren(juce::Graphics& g)
{
    if (getCurrentMarkerItems == nullptr)
        return;

    const auto markers = getCurrentMarkerItems();
    if (markers.empty() || !progressSlider.isVisible() || !drawerOpen)
        return;

    const double total = getDurationSeconds();
    if (total <= 0.001)
        return;

    auto rail = progressSlider.getBounds().toFloat();
    const float centerY = rail.getCentreY();
    for (const auto& marker : markers)
    {
        const float t = (float) juce::jlimit(0.0, 1.0, marker.seconds / total);
        const float x = rail.getX() + rail.getWidth() * t;
        const auto glowColour = marker.colour.withAlpha(isDarkTheme ? 0.20f : 0.14f);
        const auto dotColour = marker.colour.withAlpha(isDarkTheme ? 0.94f : 0.78f);
        g.setColour(glowColour);
        g.fillEllipse(x - 4.6f, centerY - 4.6f, 9.2f, 9.2f);
        g.setColour(dotColour);
        g.fillEllipse(x - 2.05f, centerY - 2.05f, 4.1f, 4.1f);
    }
}

void VideoPageComponent::resized()
{
    auto bounds = getLocalBounds();
    auto safeArea = bounds.reduced(12, 0);
    auto landscape = getWidth() > getHeight();

    if (landscape != lastLandscapeLayout)
    {
        drawerOpen = false;
        lastLandscapeLayout = landscape;
    }

    const int topInset = landscape ? 28 : 54;
    const int titleHeight = 22;
    safeArea.removeFromTop(topInset);

    auto titleRow = safeArea.removeFromTop(titleHeight);
    fileNameLabel.setBounds(titleRow.reduced(12, 0));
    safeArea.removeFromTop(8);

    juce::Rectangle<int> drawerArea;
    juce::Rectangle<int> availableMediaBounds;
    juce::Rectangle<int> presentedBounds;

    videoBounds = safeArea.reduced(0, landscape ? 0 : 4);
    drawerArea = getDrawerBounds();
    availableMediaBounds = videoBounds.reduced(landscape ? 10 : 0, landscape ? 8 : 0);

    presentedBounds = getPresentedVideoBounds(availableMediaBounds);

    if (!landscape && hasVideoLoaded)
    {
        nativePlayer->getHost().setBounds(presentedBounds);
        nativePlayer->getHost().setVisible(true);
        tapOverlay.setBounds(presentedBounds);
        hintLabel.setBounds(presentedBounds.reduced(20, 20));
    }
    else if (hasVideoLoaded)
    {
        nativePlayer->getHost().setBounds(availableMediaBounds);
        nativePlayer->getHost().setVisible(true);
        tapOverlay.setBounds(availableMediaBounds);
        hintLabel.setBounds(availableMediaBounds.reduced(20, 20));
    }
    else
    {
        // No video: hide player to show Marathon background
        nativePlayer->getHost().setVisible(false);
        nativePlayer->getHost().setBounds(juce::Rectangle<int>(0, 0, 1, 1));
        tapOverlay.setBounds(juce::Rectangle<int>());
        hintLabel.setBounds(juce::Rectangle<int>());
    }

    topMeterBounds = {};
    bottomMeterBounds = {};

    if (!landscape && hasVideoLoaded)
    {
        auto topArea = juce::Rectangle<int>(availableMediaBounds.getX(), availableMediaBounds.getY(),
                                            availableMediaBounds.getWidth(),
                                            juce::jmax(0, presentedBounds.getY() - availableMediaBounds.getY()));
        auto bottomArea = juce::Rectangle<int>(availableMediaBounds.getX(), presentedBounds.getBottom(),
                                               availableMediaBounds.getWidth(),
                                               juce::jmax(0, availableMediaBounds.getBottom() - presentedBounds.getBottom()));

        topMeterBounds = topArea.reduced(8, 6);
        bottomMeterBounds = bottomArea.reduced(8, 6);

        if (topMeterBounds.getHeight() < 60)
            topMeterBounds = {};
        if (bottomMeterBounds.getHeight() < 60)
            bottomMeterBounds = {};
    }

    if (topMeterCard != nullptr)
    {
        topMeterCard->setBounds(topMeterBounds);
        topMeterCard->setVisible(!topMeterBounds.isEmpty());
    }

    if (bottomMeterCard != nullptr)
    {
        bottomMeterCard->setBounds(bottomMeterBounds);
        bottomMeterCard->setVisible(!bottomMeterBounds.isEmpty());
    }

    topMeterSwipe.setBounds(topMeterBounds);
    bottomMeterSwipe.setBounds(bottomMeterBounds);
    topMeterSwipe.setVisible(!topMeterBounds.isEmpty() && !drawerOpen);
    bottomMeterSwipe.setVisible(!bottomMeterBounds.isEmpty() && !drawerOpen);

    const bool overlayMode = drawerOpen;
    const auto transportTextCol = isDarkTheme ? juce::Colour(0xFFF3EEE4)
                                              : GoodMeterLookAndFeel::textMain;
    const auto transportBgCol = isDarkTheme ? juce::Colour(0xFF1E2230)
                                            : GoodMeterLookAndFeel::bgMain;
    const auto labelColour = transportTextCol.withAlpha(overlayMode ? (isDarkTheme ? 0.96f : 0.90f)
                                                                     : (isDarkTheme ? 0.78f : 0.78f));
    const auto buttonColour = transportTextCol.withAlpha(overlayMode ? (isDarkTheme ? 0.96f : 0.92f)
                                                                      : (isDarkTheme ? 0.92f : 1.0f));
    const auto trackColour = transportTextCol.withAlpha(overlayMode ? (isDarkTheme ? 0.38f : 0.22f)
                                                                     : (isDarkTheme ? 0.28f : 0.15f));
    const auto railColour = transportTextCol.withAlpha(overlayMode ? (isDarkTheme ? 0.14f : 0.08f)
                                                                    : (isDarkTheme ? 0.10f : 0.08f));
    const auto thumbColour = transportTextCol.withAlpha(isDarkTheme ? 0.96f : 1.0f);

    currentTimeLabel.setColour(juce::Label::textColourId, labelColour);
    remainingTimeLabel.setColour(juce::Label::textColourId, labelColour);

    rewindBtn.setColour(juce::TextButton::textColourOffId, buttonColour);
    skipBackBtn.setColour(juce::TextButton::textColourOffId, buttonColour);
    skipFwdBtn.setColour(juce::TextButton::textColourOffId, buttonColour);
    stopBtn.setColour(juce::TextButton::textColourOffId, buttonColour);

    progressSlider.setColour(juce::Slider::thumbColourId, thumbColour);
    progressSlider.setColour(juce::Slider::trackColourId, trackColour);
    progressSlider.setColour(juce::Slider::backgroundColourId, railColour);
    volumeSlider.setColour(juce::Slider::thumbColourId, thumbColour);
    volumeSlider.setColour(juce::Slider::trackColourId, trackColour);
    volumeSlider.setColour(juce::Slider::backgroundColourId, railColour);
    playPauseBtn.setColours(thumbColour,
                            isDarkTheme ? transportBgCol : GoodMeterLookAndFeel::bgMain);

    const bool showDrawerContent = drawerOpen;
    currentTimeLabel.setVisible(showDrawerContent);
    remainingTimeLabel.setVisible(showDrawerContent);
    progressSlider.setVisible(showDrawerContent);
    rewindBtn.setVisible(showDrawerContent);
    skipBackBtn.setVisible(showDrawerContent);
    playPauseBtn.setVisible(showDrawerContent);
    skipFwdBtn.setVisible(showDrawerContent);
    stopBtn.setVisible(showDrawerContent);
    volumeSlider.setVisible(showDrawerContent);

    if (!showDrawerContent)
        return;

    auto content = drawerArea.reduced(landscape ? 16 : 12, landscape ? 8 : 4);
    auto progressRow = content.removeFromTop(22);
    currentTimeLabel.setBounds(progressRow.removeFromLeft(50));
    remainingTimeLabel.setBounds(progressRow.removeFromRight(72));
    progressSlider.setBounds(progressRow.reduced(4, 5));
    content.removeFromTop(4);

    auto controlsRow = content.removeFromTop(content.getHeight());
    const int smallBtnW = landscape ? 34 : 36;
    const int playBtnW = landscape ? 42 : 46;
    const int gap = 4;
    const int volumeW = landscape ? 88 : 84;

    auto rightArea = controlsRow.removeFromRight(volumeW);
    volumeSlider.setBounds(rightArea.reduced(0, 7));

    auto buttonStripW = smallBtnW * 4 + playBtnW + gap * 4;
    auto buttonArea = controlsRow.withSizeKeepingCentre(buttonStripW, controlsRow.getHeight());

    rewindBtn.setBounds(buttonArea.removeFromLeft(smallBtnW));
    buttonArea.removeFromLeft(gap);
    skipBackBtn.setBounds(buttonArea.removeFromLeft(smallBtnW));
    buttonArea.removeFromLeft(gap);
    playPauseBtn.setBounds(buttonArea.removeFromLeft(playBtnW).withTrimmedTop(1).withTrimmedBottom(1));
    buttonArea.removeFromLeft(gap);
    skipFwdBtn.setBounds(buttonArea.removeFromLeft(smallBtnW));
    buttonArea.removeFromLeft(gap);
    stopBtn.setBounds(buttonArea.removeFromLeft(smallBtnW));

    // Keep the floating transport actually interactive: swipe overlays are hidden
    // while open, and the controls are brought above any surviving siblings.
    currentTimeLabel.toFront(false);
    remainingTimeLabel.toFront(false);
    progressSlider.toFront(false);
    rewindBtn.toFront(false);
    skipBackBtn.toFront(false);
    playPauseBtn.toFront(false);
    skipFwdBtn.toFront(false);
    stopBtn.toFront(false);
    volumeSlider.toFront(false);
}

#if MARATHON_ART_STYLE
void VideoPageComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    if (isMarkerModeActive != nullptr
        && isMarkerModeActive()
        && addMarkerAtCurrentPosition != nullptr)
    {
        addMarkerAtCurrentPosition();
        return;
    }
}

void VideoPageComponent::mouseDown(const juce::MouseEvent& e)
{
    if (hasVideoLoaded)
        return;

    auto bounds = getLocalBounds().toFloat();
    float cellW = bounds.getWidth() / bgCanvas->getWidth();
    float cellH = bounds.getHeight() / bgCanvas->getHeight();

    longPressCenterX = (int)(e.position.x / cellW);
    longPressCenterY = (int)(e.position.y / cellH);
    dragStartX = longPressCenterX;
    dragStartY = longPressCenterY;
    pressStartTime = juce::Time::getMillisecondCounterHiRes();
    wasDragged = false;
    longPressActive = false;
    longPressRadius = 0.0f;
}

void VideoPageComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (hasVideoLoaded)
        return;

    auto bounds = getLocalBounds().toFloat();
    float cellW = bounds.getWidth() / bgCanvas->getWidth();
    float cellH = bounds.getHeight() / bgCanvas->getHeight();

    int newX = (int)(e.position.x / cellW);
    int newY = (int)(e.position.y / cellH);

    longPressCenterX = newX;
    longPressCenterY = newY;

    int dx = newX - dragStartX;
    int dy = newY - dragStartY;
    int dragDist = (int)std::sqrt(dx*dx + dy*dy);

    if (dragDist > 1)
        wasDragged = true;
}

void VideoPageComponent::mouseUp(const juce::MouseEvent&)
{
    if (hasVideoLoaded)
        return;

    double pressDuration = juce::Time::getMillisecondCounterHiRes() - pressStartTime;

    int dx = longPressCenterX - dragStartX;
    int dy = longPressCenterY - dragStartY;
    int dragDist = (int)std::sqrt(dx*dx + dy*dy);

    if (wasDragged && dragDist > 3)
    {
        triggerFanRipple(dragStartX, dragStartY, dx, dy, dragDist);
    }
    else if (!wasDragged && pressDuration < 200.0 && !rippleActive)
    {
        rippleCenterX = longPressCenterX;
        rippleCenterY = longPressCenterY;
        rippleRadius = 0.0f;
        rippleVelocity = 0.5f;
        rippleActive = true;
    }

    longPressActive = false;
    longPressRadius = 0.0f;
    longPressWaveCount = 0;
    pressStartTime = 0.0;
}

void VideoPageComponent::randomizeBackground()
{
    static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
    juce::Random rng;
    const auto preset = MarathonField::Preset::video;

    for (int y = 0; y < bgCanvas->getHeight(); ++y)
    {
        int consecutiveCount = 0;
        char32_t lastSymbol = 0;

        for (int x = 0; x < bgCanvas->getWidth(); ++x)
        {
            if (MarathonField::shouldLeaveBlank(x, y, bgCanvas->getWidth(), bgCanvas->getHeight(), preset))
            {
                bgCanvas->setCell(x, y, U' ', juce::Colours::white, 0, 0.0f);
                lastSymbol = U' ';
                consecutiveCount = 0;
                continue;
            }

            int idx = rng.nextInt(10);
            char32_t sym = symbols[idx];

            if (sym == lastSymbol)
            {
                consecutiveCount++;
                if (consecutiveCount >= 3)
                {
                    do {
                        idx = rng.nextInt(9);
                        sym = symbols[idx];
                    } while (sym == lastSymbol);
                        consecutiveCount = 0;
                }
            }
            else
            {
                consecutiveCount = 0;
            }

            if (x % 7 == 0 && (sym == U'.' || sym == U'·'))
                sym = U'□';
            else if (y % 6 == 0 && (sym == U'.' || sym == U'·'))
                sym = U'/';

            lastSymbol = sym;
            float brightness = MarathonField::brightnessForCell(x, y, bgCanvas->getWidth(), bgCanvas->getHeight(), preset);
            bgCanvas->setCell(x, y, sym, juce::Colours::white, 0, brightness);
        }
    }
}

void VideoPageComponent::rippleUpdate()
{
    static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
    juce::Random rng;

    int w = bgCanvas->getWidth();
    int h = bgCanvas->getHeight();

    int currentRadius = (int)rippleRadius;
    int minX = juce::jmax(0, rippleCenterX - currentRadius - 1);
    int maxX = juce::jmin(w - 1, rippleCenterX + currentRadius + 1);
    int minY = juce::jmax(0, rippleCenterY - currentRadius - 1);
    int maxY = juce::jmin(h - 1, rippleCenterY + currentRadius + 1);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            int dx = x - rippleCenterX;
            int dy = y - rippleCenterY;
            int dist = (int)std::sqrt(dx*dx + dy*dy);

            if (dist == currentRadius)
            {
                int idx = rng.nextInt(10);
                float brightness = 0.8f + rng.nextFloat() * 0.2f;
                bgCanvas->setCell(x, y, symbols[idx], juce::Colours::white, 0, brightness);
            }
            else if (dist < currentRadius)
            {
                auto cell = bgCanvas->getCell(x, y);
                if (cell.brightness > 0.25f)
                {
                    float newBrightness = juce::jmax(0.25f, cell.brightness - 0.05f);
                    bgCanvas->setCell(x, y, cell.symbol, juce::Colours::white, 0, newBrightness);
                }
            }
        }
    }

    rippleVelocity += rippleAcceleration;
    rippleRadius += rippleVelocity;

    int maxDist = (int)std::sqrt(w*w + h*h);
    if (rippleRadius > maxDist)
    {
        rippleActive = false;
    }

    repaint();
}

void VideoPageComponent::triggerFanRipple(int originX, int originY, int dx, int dy, int dragDist)
{
    fanOriginX = originX;
    fanOriginY = originY;

    float len = std::sqrt(dx*dx + dy*dy);
    if (len > 0.01f)
    {
        fanDirectionX = dx / len;
        fanDirectionY = dy / len;
    }
    else
    {
        fanDirectionX = 1.0f;
        fanDirectionY = 0.0f;
    }

    fanMaxRadius = dragDist * 1.5f;
    fanRadius = 0.0f;
    fanVelocity = 0.5f;
    fanRippleActive = true;
}

void VideoPageComponent::updateFanRipple()
{
    static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
    juce::Random rng;

    int w = bgCanvas->getWidth();
    int h = bgCanvas->getHeight();
    int currentRadius = (int)fanRadius;

    int minX = juce::jmax(0, fanOriginX - currentRadius - 1);
    int maxX = juce::jmin(w - 1, fanOriginX + currentRadius + 1);
    int minY = juce::jmax(0, fanOriginY - currentRadius - 1);
    int maxY = juce::jmin(h - 1, fanOriginY + currentRadius + 1);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            int dx = x - fanOriginX;
            int dy = y - fanOriginY;
            float dist = std::sqrt(dx*dx + dy*dy);

            if (std::abs(dist - fanRadius) < 1.0f)
            {
                float dotProduct = (dx * fanDirectionX + dy * fanDirectionY) / juce::jmax(0.01f, dist);
                float angle = std::acos(juce::jlimit(-1.0f, 1.0f, dotProduct));

                if (angle <= fanAngle / 2.0f)
                {
                    int idx = rng.nextInt(10);
                    float brightness = 0.75f + rng.nextFloat() * 0.2f;
                    bgCanvas->setCell(x, y, symbols[idx], juce::Colours::white, 0, brightness);
                }
            }
        }
    }

    fanVelocity += 0.15f;
    fanRadius += fanVelocity;

    if (fanRadius > fanMaxRadius)
        fanRippleActive = false;

    repaint();
}

void VideoPageComponent::updateLongPressRipple()
{
    static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
    juce::Random rng;

    int w = bgCanvas->getWidth();
    int h = bgCanvas->getHeight();
    int currentRadius = (int)longPressRadius;

    int minX = juce::jmax(0, longPressCenterX - currentRadius - 1);
    int maxX = juce::jmin(w - 1, longPressCenterX + currentRadius + 1);
    int minY = juce::jmax(0, longPressCenterY - currentRadius - 1);
    int maxY = juce::jmin(h - 1, longPressCenterY + currentRadius + 1);

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            int dx = x - longPressCenterX;
            int dy = y - longPressCenterY;
            int dist = (int)std::sqrt(dx*dx + dy*dy);

            if (dist == currentRadius)
            {
                int idx = rng.nextInt(10);
                float brightness = 0.7f + rng.nextFloat() * 0.2f;
                bgCanvas->setCell(x, y, symbols[idx], juce::Colours::white, 0, brightness);
            }
        }
    }

    repaint();
}
#else
void VideoPageComponent::mouseDown(const juce::MouseEvent&)
{
}

void VideoPageComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    if (isMarkerModeActive != nullptr
        && isMarkerModeActive()
        && addMarkerAtCurrentPosition != nullptr)
    {
        addMarkerAtCurrentPosition();
    }
}
#endif

bool VideoPageComponent::loadVideo(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    juce::String error;
    if (!nativePlayer->load(file, error))
    {
        currentFileName = juce::URL::removeEscapeChars(file.getFileName());
        currentFilePath.clear();
        hasVideoLoaded = false;
        fileNameLabel.setText(currentFileName, juce::dontSendNotification);
        hintLabel.setText(error.isNotEmpty() ? error : "This video format is not supported on this device yet",
                          juce::dontSendNotification);
        hintLabel.setVisible(true);
        setPlayButtonVisualState(false);
        repaint();
        return false;
    }

    currentFilePath = file.getFullPathName();
    currentFileName = juce::URL::removeEscapeChars(file.getFileName());
    syncedAudioPath.clear();
    syncedAudioLoaded = false;
    hasVideoLoaded = true;
    forcedPausePosition = 0.0;
    userRequestedPlayingState = false;
    playbackIntentHoldFrames = 0;
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = 0.0;
    lastVideoRecoveryMs = 0;
    fileNameLabel.setText(currentFileName, juce::dontSendNotification);
    hintLabel.setVisible(false);
    nativePlayer->setPosition(0.0);
    attachSyncedAudioIfAvailable();
    if (!syncedAudioLoaded)
        nativePlayer->setVolume((float) volumeSlider.getValue());
    else
        syncAudioTransportToPosition(0.0, false);
    setPlayButtonVisualState(false);
    drawerOpen = false;
    resized();
    repaint();
    return true;
}

void VideoPageComponent::clearVideo()
{
    if (syncedAudioLoaded && audioEngine.getCurrentFilePath() == syncedAudioPath)
    {
        audioEngine.pause();
        audioEngine.seek(0.0);
    }

    nativePlayer->clear();
    hasVideoLoaded = false;
    currentFilePath.clear();
    currentFileName.clear();
    syncedAudioPath.clear();
    syncedAudioLoaded = false;
    forcedPausePosition = 0.0;
    userRequestedPlayingState = false;
    playbackIntentHoldFrames = 0;
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = 0.0;
    lastVideoRecoveryMs = 0;
    fileNameLabel.setText("No video loaded", juce::dontSendNotification);
    hintLabel.setText("Import a video on Page 1 or load one from History", juce::dontSendNotification);
    hintLabel.setVisible(true);
    setPlayButtonVisualState(false);
    currentTimeLabel.setText("0:00", juce::dontSendNotification);
    remainingTimeLabel.setText("-0:00", juce::dontSendNotification);
    progressSlider.setValue(0.0, juce::dontSendNotification);
    drawerOpen = false;
    resized();
}

juce::String VideoPageComponent::getCurrentVideoPath() const
{
    return currentFilePath;
}

juce::String VideoPageComponent::getTransportDisplayName() const
{
    return currentFileName.isNotEmpty() ? currentFileName : "No video loaded";
}

bool VideoPageComponent::hasLoadedVideo() const
{
    return hasVideoLoaded;
}

bool VideoPageComponent::ownsSharedAudioTransport() const
{
    return hasVideoLoaded
        && syncedAudioLoaded
        && syncedAudioPath.isNotEmpty()
        && audioEngine.getCurrentFilePath() == syncedAudioPath;
}

bool VideoPageComponent::isTransportPlaying() const
{
    return hasVideoLoaded && nativePlayer != nullptr && nativePlayer->isPlaying();
}

double VideoPageComponent::getTransportPositionSeconds() const
{
    if (!hasVideoLoaded || nativePlayer == nullptr)
        return 0.0;

    return juce::jlimit(0.0, getDurationSeconds(), nativePlayer->getPosition());
}

double VideoPageComponent::getTransportDurationSeconds() const
{
    return hasVideoLoaded ? getDurationSeconds() : 0.0;
}

void VideoPageComponent::playTransport()
{
    if (!hasVideoLoaded || nativePlayer == nullptr)
        return;

    forcedPausePosition = 0.0;
    userRequestedPlayingState = true;
    playbackIntentHoldFrames = 8;
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = nativePlayer->getPosition();
    setPlayButtonVisualState(true);
    attachSyncedAudioIfAvailable();
    syncAudioTransportToPosition(nativePlayer->getPosition(), false);
    nativePlayer->play();
    if (syncedAudioLoaded)
        audioEngine.play();
}

void VideoPageComponent::pauseTransport()
{
    if (!hasVideoLoaded || nativePlayer == nullptr)
        return;

    forcedPausePosition = nativePlayer->getPosition();
    userRequestedPlayingState = false;
    playbackIntentHoldFrames = 0;
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = forcedPausePosition;
    setPlayButtonVisualState(false);
    nativePlayer->pause();
    if (std::abs(nativePlayer->getPosition() - forcedPausePosition) > 0.02)
        nativePlayer->setPosition(forcedPausePosition, true);

    if (syncedAudioLoaded)
    {
        audioEngine.pause();
        audioEngine.seek(forcedPausePosition);
    }

    refreshVideoPlaybackSurface(false, "pause-refresh");
}

void VideoPageComponent::rewindTransport()
{
    if (!hasVideoLoaded || nativePlayer == nullptr)
        return;

    userRequestedPlayingState = false;
    playbackIntentHoldFrames = 0;
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = 0.0;
    setPlayButtonVisualState(false);
    nativePlayer->pause();
    nativePlayer->queueSmoothSeek(0.0, true);
    queueProgressSeek(0.0);
}

void VideoPageComponent::seekTransport(double seconds)
{
    if (!hasVideoLoaded || nativePlayer == nullptr)
        return;

    auto target = juce::jlimit(0.0, getDurationSeconds(), seconds);
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = target;
    nativePlayer->queueSmoothSeek(target, true);
    queueProgressSeek(target);
}

void VideoPageComponent::jumpToEndTransport()
{
    if (!hasVideoLoaded || nativePlayer == nullptr)
        return;

    auto target = juce::jmax(0.0, getDurationSeconds() - 0.05);
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = target;
    nativePlayer->queueSmoothSeek(target, true);
    queueProgressSeek(target);
}

void VideoPageComponent::queueProgressSeek(double seconds)
{
    pendingProgressSeekSeconds = juce::jmax(0.0, seconds);
    progressSeekPending = true;
}

void VideoPageComponent::flushQueuedProgressSeek(bool force)
{
    if (!progressSeekPending || !hasVideoLoaded || nativePlayer == nullptr)
        return;

    if (progressScrubDragging && !force)
        return;

    if (nativePlayer->hasPendingSeek())
        return;

    const auto nowMs = juce::Time::getMillisecondCounter();
    if (!force && static_cast<std::int32_t>(nowMs - lastProgressSeekCommitMs) < 35)
        return;

    const auto target = juce::jlimit(0.0, getDurationSeconds(), pendingProgressSeekSeconds);
    syncAudioTransportToPosition(target, nativePlayer->isPlaying());
    lastProgressSeekCommitMs = nowMs;
    progressSeekPending = false;
}

void VideoPageComponent::refreshVideoPlaybackSurface(bool preservePlaybackState, const juce::String&)
{
    if (!hasVideoLoaded || nativePlayer == nullptr || currentFilePath.isEmpty())
        return;

    juce::String error;
    double refreshPosition = 0.0;

    if (syncedAudioLoaded && audioEngine.getCurrentFilePath().isNotEmpty())
        refreshPosition = audioEngine.getCurrentPosition();
    else
        refreshPosition = nativePlayer->getPosition();

    refreshPosition = juce::jlimit(0.0, getDurationSeconds(), refreshPosition);

    if (!nativePlayer->reloadPreservingState(refreshPosition, preservePlaybackState, error))
    {
        DBG("GOODMETER video refresh failed: " + error);
        return;
    }

    if (syncedAudioLoaded)
        syncAudioTransportToPosition(refreshPosition, preservePlaybackState);

    userRequestedPlayingState = preservePlaybackState;
    stagnantVideoFrameCount = 0;
    stagnantVideoRecoveryAttempts = 0;
    lastObservedVideoPosition = refreshPosition;
    lastVideoRecoveryMs = juce::Time::getMillisecondCounter();
    setPlayButtonVisualState(preservePlaybackState);
    repaint();
}

void VideoPageComponent::timerCallback()
{
#if MARATHON_ART_STYLE
    if (rippleActive)
    {
        rippleUpdate();
    }

    if (fanRippleActive)
    {
        updateFanRipple();
    }

    if (longPressActive)
    {
        float speedFactor = juce::jmin(1.0f, longPressWaveCount / 26.0f);
        float currentSpeed = 0.15f + speedFactor * 0.45f;

        longPressRadius += currentSpeed;

        float maxRadius = longPressMaxRadius;
        if (longPressWaveCount >= 20)
        {
            float extraRadius = (longPressWaveCount - 20) * 0.08f;
            maxRadius += extraRadius;
        }

        if (longPressRadius > maxRadius)
        {
            longPressRadius = 0.0f;
            longPressWaveCount++;
        }

        updateLongPressRipple();
    }
    else if (!longPressActive && pressStartTime > 0.0)
    {
        double pressDuration = juce::Time::getMillisecondCounterHiRes() - pressStartTime;
        if (pressDuration > 200.0)
        {
            longPressActive = true;
            longPressRadius = 0.0f;
            longPressWaveCount = 0;
        }
    }

    if (!hasVideoLoaded)
    {
        autoRippleTimer += 0.033f;  // 30Hz timer
        if (autoRippleTimer >= 15.0f && !rippleActive)
        {
            autoRippleTimer = 0.0f;

            int w = bgCanvas->getWidth();
            int h = bgCanvas->getHeight();

            switch (autoRipplePhase)
            {
                case 0: rippleCenterX = 0; rippleCenterY = 0; break;
                case 1: rippleCenterX = 0; rippleCenterY = h - 1; break;
                case 2: rippleCenterX = w - 1; rippleCenterY = h - 1; break;
                case 3: rippleCenterX = w - 1; rippleCenterY = 0; break;
                case 4: rippleCenterX = w / 2; rippleCenterY = 0; break;
                case 5: rippleCenterX = w - 1; rippleCenterY = h - 1; break;
                case 6: rippleCenterX = w - 1; rippleCenterY = 0; break;
                case 7: rippleCenterX = 0; rippleCenterY = 0; break;
                case 8: rippleCenterX = 0; rippleCenterY = h - 1; break;
                case 9: rippleCenterX = w / 2; rippleCenterY = h - 1; break;
            }

            rippleRadius = 0.0f;
            rippleVelocity = 0.5f;
            rippleActive = true;
            autoRipplePhase = (autoRipplePhase + 1) % 10;
        }
    }
#endif

    if (!syncedAudioLoaded)
        attachSyncedAudioIfAvailable();

    const float peakL = processor.peakLevelL.load(std::memory_order_relaxed);
    const float peakR = processor.peakLevelR.load(std::memory_order_relaxed);
    const float rmsL = processor.rmsLevelL.load(std::memory_order_relaxed);
    const float rmsR = processor.rmsLevelR.load(std::memory_order_relaxed);
    const float momentary = processor.lufsLevel.load(std::memory_order_relaxed);
    const float shortTerm = processor.lufsShortTerm.load(std::memory_order_relaxed);
    const float integrated = processor.lufsIntegrated.load(std::memory_order_relaxed);
    const float phase = processor.phaseCorrelation.load(std::memory_order_relaxed);
    const float luRangeVal = processor.luRange.load(std::memory_order_relaxed);

    if (topLevelsMeter != nullptr)
        topLevelsMeter->updateMetrics(peakL, peakR, momentary, shortTerm, integrated, luRangeVal);
    if (bottomLevelsMeter != nullptr)
        bottomLevelsMeter->updateMetrics(peakL, peakR, momentary, shortTerm, integrated, luRangeVal);
    if (topVuMeter != nullptr)
        topVuMeter->updateVU(rmsL, rmsR);
    if (bottomVuMeter != nullptr)
        bottomVuMeter->updateVU(rmsL, rmsR);
    if (topPhaseMeter != nullptr)
        topPhaseMeter->updateCorrelation(phase);
    if (bottomPhaseMeter != nullptr)
        bottomPhaseMeter->updateCorrelation(phase);

    if (topMeterCard != nullptr && topMeterAlpha < 1.0f)
    {
        topMeterAlpha = juce::jmin(1.0f, topMeterAlpha + 0.16f);
        topMeterCard->setAlpha(topMeterAlpha);
    }

    if (bottomMeterCard != nullptr && bottomMeterAlpha < 1.0f)
    {
        bottomMeterAlpha = juce::jmin(1.0f, bottomMeterAlpha + 0.16f);
        bottomMeterCard->setAlpha(bottomMeterAlpha);
    }

    if (!hasVideoLoaded)
    {
        setPlayButtonVisualState(false);
        return;
    }

    flushQueuedProgressSeek(false);

    auto duration = getDurationSeconds();
    auto position = juce::jlimit(0.0, duration, nativePlayer->getPosition());
    if (progressScrubDragging && duration > 0.01)
        position = juce::jlimit(0.0, duration, progressSlider.getValue() * duration);

    currentTimeLabel.setText(fmtTime(position), juce::dontSendNotification);
    remainingTimeLabel.setText("-" + fmtTime(juce::jmax(0.0, duration - position)), juce::dontSendNotification);

    if (!progressScrubDragging && duration > 0.01)
        progressSlider.setValue(position / duration, juce::dontSendNotification);

    auto nowPlaying = nativePlayer->isPlaying();

    if (userRequestedPlayingState && syncedAudioLoaded && audioEngine.isPlaying()
        && !progressScrubDragging && !nativePlayer->hasPendingSeek())
    {
        const bool nearEnd = duration > 0.01 && position >= duration - 0.05;
        if (!nearEnd)
        {
            if (std::abs(position - lastObservedVideoPosition) < 0.0015)
                ++stagnantVideoFrameCount;
            else
            {
                stagnantVideoFrameCount = 0;
                stagnantVideoRecoveryAttempts = 0;
            }

            lastObservedVideoPosition = position;

            const auto nowMs = juce::Time::getMillisecondCounter();
            if (stagnantVideoFrameCount >= 12
                && static_cast<std::int32_t>(nowMs - lastVideoRecoveryMs) > 250)
            {
                auto audioPos = juce::jlimit(0.0, duration, audioEngine.getCurrentPosition());
                auto recoveryTarget = std::abs(audioPos - position) > 0.02 ? audioPos : position;
                ++stagnantVideoRecoveryAttempts;

                if (stagnantVideoRecoveryAttempts >= 2)
                    refreshVideoPlaybackSurface(true, "watchdog-reload");
                else
                {
                    nativePlayer->setPosition(recoveryTarget, false);
                    nativePlayer->play();
                }

                lastVideoRecoveryMs = nowMs;
                lastObservedVideoPosition = recoveryTarget;
                stagnantVideoFrameCount = 0;
            }
        }
        else
        {
            stagnantVideoFrameCount = 0;
            stagnantVideoRecoveryAttempts = 0;
            lastObservedVideoPosition = position;
        }
    }
    else
    {
        stagnantVideoFrameCount = 0;
        stagnantVideoRecoveryAttempts = 0;
        lastObservedVideoPosition = position;
    }

    if (playbackIntentHoldFrames > 0)
    {
        --playbackIntentHoldFrames;

        if (syncedAudioLoaded && userRequestedPlayingState && !audioEngine.isPlaying())
            audioEngine.play();
    }
    else if (syncedAudioLoaded)
    {
        if (nowPlaying && !audioEngine.isPlaying())
            audioEngine.play();
        else if (!nowPlaying && audioEngine.isPlaying())
            audioEngine.pause();
    }

    const bool reachedEnd = duration > 0.01 && position >= duration - 0.05;
    if (userRequestedPlayingState && !nowPlaying && playbackIntentHoldFrames <= 0 && reachedEnd)
    {
        userRequestedPlayingState = false;
        setPlayButtonVisualState(false);
    }
}

juce::String VideoPageComponent::fmtTime(double seconds)
{
    if (seconds < 0.0)
        seconds = 0.0;

    auto totalSeconds = (int) std::round(seconds);
    auto hours = totalSeconds / 3600;
    auto minutes = (totalSeconds / 60) % 60;
    auto remainSeconds = totalSeconds % 60;

    if (hours > 0)
        return juce::String(hours) + ":"
             + juce::String(minutes).paddedLeft('0', 2) + ":"
             + juce::String(remainSeconds).paddedLeft('0', 2);

    return juce::String((totalSeconds / 60)) + ":" + juce::String(remainSeconds).paddedLeft('0', 2);
}

double VideoPageComponent::getDurationSeconds() const
{
    auto duration = nativePlayer->getDuration();
    if (!std::isfinite(duration) || duration < 0.0)
        return 0.0;
    return duration;
}

int VideoPageComponent::getDrawerHeight(bool landscape) const
{
    if (!drawerOpen)
        return 0;

    return landscape ? 74 : 82;
}

juce::Rectangle<int> VideoPageComponent::getDrawerBounds() const
{
    auto landscape = getWidth() > getHeight();

    if (landscape)
    {
        if (!drawerOpen)
            return {};

        auto overlay = videoBounds.reduced(18, 12);
        return overlay.removeFromBottom(getDrawerHeight(true));
    }

    if (!drawerOpen)
        return {};

    auto overlay = videoBounds.reduced(12, 10);
    return overlay.removeFromBottom(getDrawerHeight(false));
}

void VideoPageComponent::setPlayButtonVisualState(bool shouldShowPlaying)
{
    if (playPauseBtn.playing == shouldShowPlaying)
        return;

    playPauseBtn.playing = shouldShowPlaying;
    playPauseBtn.repaint();
}

void VideoPageComponent::setDrawerOpen(bool shouldOpen)
{
    if (drawerOpen == shouldOpen)
        return;

    drawerOpen = shouldOpen;
    resized();
    repaint();
}
