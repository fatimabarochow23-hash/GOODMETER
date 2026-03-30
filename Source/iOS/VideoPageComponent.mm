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
        videoView.backgroundColor = UIColor.blackColor;
        videoView.playerLayer.videoGravity = AVLayerVideoGravityResizeAspect;
        videoView.tapHandler = ^{
            if (onVideoTapped != nullptr)
                onVideoTapped();
        };
        host.setView(videoView);
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

    void setPosition(double seconds)
    {
        if (player == nil)
            return;

        auto target = CMTimeMakeWithSeconds(seconds, 600);
        [player seekToTime:target toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
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

    juce::String getCurrentPath() const { return currentPath; }
    juce::Point<float> getPresentationSize() const
    {
        return { static_cast<float>(presentationSize.width), static_cast<float>(presentationSize.height) };
    }
    bool isSourcePortrait() const { return sourceWasPortrait; }

    std::function<void()> onVideoTapped;

private:
    juce::UIViewComponent host;
    GOODMETERVideoView* videoView = nil;
    AVPlayer* player = nil;
    AVPlayerItem* playerItem = nil;
    double durationSeconds = 0.0;
    juce::String currentPath;
    CGSize presentationSize = CGSizeZero;
    bool sourceWasPortrait = false;
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
    void setPosition(double) {}
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
    nativePlayer = std::make_unique<NativeVideoPlayer>();
    addAndMakeVisible(nativePlayer->getHost());
    tapOverlay.onTap = [this]()
    {
        if (hasVideoLoaded)
            setDrawerOpen(!drawerOpen);
    };
    addAndMakeVisible(tapOverlay);
    nativePlayer->onVideoTapped = [this]()
    {
        if (hasVideoLoaded)
            setDrawerOpen(!drawerOpen);
    };

    topMeterSwipe.onSwipe = [this](int direction) { cycleMeterSlot(true, direction); };
    bottomMeterSwipe.onSwipe = [this](int direction) { cycleMeterSlot(false, direction); };
    addAndMakeVisible(topMeterSwipe);
    addAndMakeVisible(bottomMeterSwipe);

    fileNameLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    fileNameLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
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
    progressSlider.onValueChange = [this]()
    {
        if (progressSlider.isMouseButtonDown())
        {
            auto duration = getDurationSeconds();
            if (duration > 0.01)
            {
                auto target = progressSlider.getValue() * duration;
                nativePlayer->setPosition(target);
                syncAudioTransportToPosition(target, nativePlayer->isPlaying());
            }
        }
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

        nativePlayer->setPosition(0.0);
        syncAudioTransportToPosition(0.0, nativePlayer->isPlaying());
    };
    skipBackBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        auto pos = juce::jmax(0.0, nativePlayer->getPosition() - 5.0);
        nativePlayer->setPosition(pos);
        syncAudioTransportToPosition(pos, nativePlayer->isPlaying());
    };
    playPauseBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        if (nativePlayer->isPlaying())
        {
            forcedPausePosition = nativePlayer->getPosition();
            userRequestedPlayingState = false;
            playbackIntentHoldFrames = 0;
            setPlayButtonVisualState(false);
            nativePlayer->pause();

            juce::Component::SafePointer<VideoPageComponent> safeThis(this);
            const double pausePosition = forcedPausePosition;
            juce::Timer::callAfterDelay(1, [safeThis, pausePosition]()
            {
                if (safeThis == nullptr || safeThis->userRequestedPlayingState)
                    return;

                safeThis->nativePlayer->pause();
                if (std::abs(safeThis->nativePlayer->getPosition() - pausePosition) > 0.02)
                    safeThis->nativePlayer->setPosition(pausePosition);

                if (safeThis->syncedAudioLoaded)
                {
                    safeThis->audioEngine.pause();
                    safeThis->audioEngine.seek(pausePosition);
                }
            });
        }
        else
        {
            forcedPausePosition = 0.0;
            userRequestedPlayingState = true;
            playbackIntentHoldFrames = 8;
            setPlayButtonVisualState(true);
            attachSyncedAudioIfAvailable();
            syncAudioTransportToPosition(nativePlayer->getPosition(), false);
            nativePlayer->play();
            if (syncedAudioLoaded)
                audioEngine.play();
        }
    };
    skipFwdBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        auto duration = getDurationSeconds();
        auto pos = juce::jmin(duration, nativePlayer->getPosition() + 5.0);
        nativePlayer->setPosition(pos);
        syncAudioTransportToPosition(pos, nativePlayer->isPlaying());
    };
    stopBtn.onClick = [this]()
    {
        if (!hasVideoLoaded)
            return;

        auto duration = getDurationSeconds();
        auto target = juce::jmax(0.0, duration - 0.05);
        nativePlayer->setPosition(target);
        syncAudioTransportToPosition(target, nativePlayer->isPlaying());
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
}

VideoPageComponent::~VideoPageComponent()
{
    stopTimer();
    clearVideo();
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
    g.fillAll(GoodMeterLookAndFeel::bgMain);

    auto drawerArea = getDrawerBounds().toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.92f));
    g.fillRoundedRectangle(videoBounds.toFloat(), 18.0f);

    if (drawerOpen)
    {
        auto overlay = drawerArea;
        g.setColour(juce::Colour(0xFF121620).withAlpha(0.88f));
        g.fillRoundedRectangle(overlay, 16.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(overlay.reduced(0.5f), 16.0f, 1.0f);
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

    if (drawerOpen && !drawerArea.isEmpty())
        availableMediaBounds = availableMediaBounds.withTrimmedBottom(drawerArea.getHeight() + (landscape ? 8 : 6));

    presentedBounds = getPresentedVideoBounds(availableMediaBounds);

    if (!landscape && hasVideoLoaded)
    {
        nativePlayer->getHost().setBounds(presentedBounds);
        tapOverlay.setBounds(presentedBounds);
        hintLabel.setBounds(presentedBounds.reduced(20, 20));
    }
    else
    {
        nativePlayer->getHost().setBounds(availableMediaBounds);
        tapOverlay.setBounds(availableMediaBounds);
        hintLabel.setBounds(availableMediaBounds.reduced(20, 20));
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
    topMeterSwipe.setVisible(!topMeterBounds.isEmpty());
    bottomMeterSwipe.setVisible(!bottomMeterBounds.isEmpty());

    const bool overlayMode = drawerOpen;
    const auto labelColour = overlayMode ? juce::Colours::white.withAlpha(0.88f)
                                         : GoodMeterLookAndFeel::textMuted;
    const auto buttonColour = overlayMode ? juce::Colours::white.withAlpha(0.78f)
                                          : GoodMeterLookAndFeel::textMain;
    const auto trackColour = overlayMode ? juce::Colours::white.withAlpha(0.58f)
                                         : GoodMeterLookAndFeel::textMain.withAlpha(0.15f);
    const auto railColour = overlayMode ? juce::Colours::white.withAlpha(0.16f)
                                        : GoodMeterLookAndFeel::textMain.withAlpha(0.08f);
    const auto thumbColour = overlayMode ? juce::Colours::white.withAlpha(0.92f)
                                         : GoodMeterLookAndFeel::textMain;

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
    playPauseBtn.setColours(overlayMode ? juce::Colours::white.withAlpha(0.95f) : GoodMeterLookAndFeel::textMain,
                            overlayMode ? juce::Colour(0xFF11151F) : GoodMeterLookAndFeel::bgMain);

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
    currentTimeLabel.setBounds(progressRow.removeFromLeft(34));
    remainingTimeLabel.setBounds(progressRow.removeFromRight(38));
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
}

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

void VideoPageComponent::timerCallback()
{
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

    auto duration = getDurationSeconds();
    auto position = juce::jlimit(0.0, duration, nativePlayer->getPosition());

    currentTimeLabel.setText(fmtTime(position), juce::dontSendNotification);
    remainingTimeLabel.setText("-" + fmtTime(juce::jmax(0.0, duration - position)), juce::dontSendNotification);

    if (!progressSlider.isMouseButtonDown() && duration > 0.01)
        progressSlider.setValue(position / duration, juce::dontSendNotification);

    auto nowPlaying = nativePlayer->isPlaying();

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
    auto minutes = totalSeconds / 60;
    auto remainSeconds = totalSeconds % 60;
    return juce::String(minutes) + ":" + juce::String(remainSeconds).paddedLeft('0', 2);
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
