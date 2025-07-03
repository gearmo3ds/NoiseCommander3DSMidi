/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class NcMidiAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        private juce::Timer

{
public:
    NcMidiAudioProcessorEditor (NcMidiAudioProcessor&);
    ~NcMidiAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    juce::String selfIpAddress;
    juce::String dsIpAddress = "192.168.2.100";
    juce::TextEditor midiLog;
    void timerCallback() override;

    juce::TextButton clearButton { "Clear" };
    juce::Slider maxLinesSlider;
    int maxLines = 30;

    juce::ToggleButton enableLoggingToggle { "Enable Logging" };
    bool loggingEnabled = true;
    bool initialized = false;

    juce::ComboBox selfIpSelector;
    juce::TextEditor dsIpSelector;
    juce::Label ipLabel;

    juce::TextButton discoverButton;
    void startDiscovery();
    bool isDiscovering = false;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NcMidiAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NcMidiAudioProcessorEditor);
};
