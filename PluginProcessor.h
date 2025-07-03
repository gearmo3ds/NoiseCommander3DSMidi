/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <deque>
#include <mutex>

//==============================================================================
/**
*/
class NcMidiAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    NcMidiAudioProcessor();
    ~NcMidiAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::DatagramSocket udpSocket;
    juce::String targetIP = "192.168.2.101";
    int targetPort = 9001; // Default DSMIDI UDP port

    // In your plugin processor header
    std::unique_ptr<juce::DatagramSocket> udpReceiver;
    juce::Array<juce::MidiMessage> incomingMidiFrom3DS;

    double previousPpq = 0;
    double ppqTicksAccumulated = 0.0;
    bool wasPlaying = false;

    void pushMidiMessage(const juce::MidiMessage& message);
     juce::String getLastMidiMessage(); // thread-safe getter
     juce::String lastMidiMessage;
     juce::CriticalSection messageLock;

     std::deque<juce::String> midiMessageQueue;
     std::mutex midiQueueMutex;

     void pushMidiMessage(const juce::String& msg);
     std::deque<juce::String> popMidiMessages();

     void set3DSIPAddress(juce::String const& value);

     juce::ApplicationProperties appProperties;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NcMidiAudioProcessor)
};
