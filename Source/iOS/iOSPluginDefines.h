/*
  ==============================================================================
    iOSPluginDefines.h
    GOODMETER iOS - Plugin macro compatibility for guiapp compilation

    Without juce_audio_plugin_client, plugin macros like JucePlugin_Name are
    undefined. This header provides fallback definitions so PluginProcessor.cpp
    compiles cleanly in the iOS guiapp project.
  ==============================================================================
*/

#pragma once

#if JUCE_IOS

#ifndef JucePlugin_Name
  #define JucePlugin_Name "GOODMETER"
#endif

#ifndef JucePlugin_Build_Standalone
  #define JucePlugin_Build_Standalone 0
#endif

#ifndef JucePlugin_Build_VST3
  #define JucePlugin_Build_VST3 0
#endif

#ifndef JucePlugin_Build_AU
  #define JucePlugin_Build_AU 0
#endif

#endif // JUCE_IOS
