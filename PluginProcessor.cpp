/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h>

//==============================================================================
NcMidiAudioProcessor::NcMidiAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties() )
#endif
{
    udpSocket.bindToPort(9001); // Optional: bind to an ephemeral port
    udpSocket.setEnablePortReuse(true); // Optional

    const int listenPort = 9000; // or whatever port your 3DS sends to
    udpReceiver = std::make_unique<juce::DatagramSocket>(/* enableBroadcasting = */ false);

    // Now bind to a specific port
    if (!udpReceiver->bindToPort(listenPort))
    {
        DBG("Failed to bind UDP socket to port " << listenPort);
    }

    juce::PropertiesFile::Options options;
    options.applicationName     = "NoiseCommander3DS_VST3";
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Application Support";  // optional
    appProperties.setStorageParameters(options);

    juce::PropertiesFile* props = appProperties.getUserSettings();
    juce::String dsIpAddress = props->getValue("3ds_ip", "192.168.1.0");
    //DBG("Using 3DS IP-Address " + dsIpAddress);
    set3DSIPAddress(dsIpAddress);
}

NcMidiAudioProcessor::~NcMidiAudioProcessor()
{
    if (udpReceiver != nullptr)
    {
        udpReceiver->shutdown();  // optional, depending on your usage
        udpReceiver = nullptr;    // cleanup
    }
}

//==============================================================================
const juce::String NcMidiAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NcMidiAudioProcessor::acceptsMidi() const
{
    return true;
}

bool NcMidiAudioProcessor::producesMidi() const
{
    return true;
}

bool NcMidiAudioProcessor::isMidiEffect() const
{
    return true;
}

double NcMidiAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NcMidiAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NcMidiAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NcMidiAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NcMidiAudioProcessor::getProgramName (int index)
{
    return {};
}

void NcMidiAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NcMidiAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void NcMidiAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NcMidiAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void NcMidiAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{

    // Clear audio buffer if your plugin doesn't process audio
     buffer.clear();

//     DBG("test msg");

    // Midi Out -> UDP
    const int targetPort = 9001;

    for (const auto metadata : midiMessages)
    {
        const juce::MidiMessage& msg = metadata.getMessage();
        pushMidiMessage("Out: " + msg.getDescription());
        const void* data = static_cast<const void*>(msg.getRawData());
        int size = msg.getRawDataSize();

        // Only send if it's a real MIDI message (optional: filter SysEx, etc.)
        if (size > 0)
        {
//            DBG("Sending msg");
            udpSocket.write(targetIP, targetPort, data, size);
        }
    }

    // UDP -> Midi in
    // Poll UDP socket for data:
    if (udpReceiver && udpReceiver->waitUntilReady(true, 0))  // true = read ready, 0 = no wait
    {
        char udpBuffer[1024];
        int bytesRead = udpReceiver->read(udpBuffer, sizeof(udpBuffer), false);
//        DBG("In: " << (int)udpBuffer[0]
//                << " " << (int)udpBuffer[1]
//                << " " << (int)udpBuffer[2]);

//        DBG("Bytes read: " << bytesRead);

        if (bytesRead >= 3 && (udpBuffer[0] & 0x80))  // status byte in MSB
        {
            juce::MidiMessage msg(udpBuffer, bytesRead, juce::Time::getMillisecondCounterHiRes());
//            pushMidiMessage(msg);  // To be shown in GUI
            pushMidiMessage("In: " + msg.getDescription());
//            DBG("Pushing ...");
//            DBG(msg.getDescription());

            if (msg.isNoteOn() || msg.isNoteOff() || msg.isController() || msg.isSysEx())
            {
                midiMessages.addEvent(msg, 0);
            }
        }
    }



    return;

    // Generate Clock

    static int cntr = 0;

    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (playHead->getCurrentPosition(pos))
        {
//            DBG("pos.isPlaying = " + juce::String(pos.isPlaying ? "true" : "false"));
//            DBG("ppqPosition = " + juce::String(pos.ppqPosition, 4));


            if (pos.isPlaying)
            {
                double currentPpq = pos.ppqPosition;

                if (!wasPlaying)
                {
                    // Playback just started; sync previousPpq
                    previousPpq = currentPpq;
                    wasPlaying = true;
                    // Force one tick immediately at playback start
                    ppqTicksAccumulated += 1.0;
                }

                double ppqDelta = currentPpq - previousPpq;

                // Convert ppqDelta to MIDI clock ticks (24 per quarter note)
                ppqTicksAccumulated += ppqDelta;// * 24.0;

                int ticksToSend = (int)ppqTicksAccumulated;

                if (ticksToSend > 0)
                {
                    for (int i = 0; i < ticksToSend; ++i)
                    {
                        // Send MIDI clock message 0xF8
                        DBG("Send clock " << cntr);
                        cntr++;
                        // udpSocket.write(targetIP, targetPort, &clockByte, 1);
                    }

                    // Remove sent ticks from accumulator
                    ppqTicksAccumulated -= ticksToSend;
                }

                previousPpq = currentPpq;
            }
            else
            {
                // Reset on stop/pause
                wasPlaying = false;
                ppqTicksAccumulated = 0.0;
//                previousPpq = pos.ppqPosition;
            }
        }
    }

}

//==============================================================================
bool NcMidiAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NcMidiAudioProcessor::createEditor()
{
    return new NcMidiAudioProcessorEditor (*this);
}

//==============================================================================
void NcMidiAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void NcMidiAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void NcMidiAudioProcessor::pushMidiMessage(const juce::MidiMessage &message)
{
    const juce::ScopedLock sl(messageLock);
    lastMidiMessage = message.getDescription();
}

juce::String NcMidiAudioProcessor::getLastMidiMessage()
{
    const juce::ScopedLock sl(messageLock);
    return lastMidiMessage;
}

void NcMidiAudioProcessor::pushMidiMessage(const juce::String &msg)
{
    std::scoped_lock lock(midiQueueMutex);
     midiMessageQueue.push_back(msg);

     // Optional: cap history size
     const size_t maxSize = 1000;
     if (midiMessageQueue.size() > maxSize)
         midiMessageQueue.pop_front();
}

std::deque<juce::String> NcMidiAudioProcessor::popMidiMessages()
{
    std::scoped_lock lock(midiQueueMutex);
    auto messages = std::move(midiMessageQueue);
    midiMessageQueue.clear();
    return messages;
}

void NcMidiAudioProcessor::set3DSIPAddress(const juce::String &value)
{
    targetIP = value;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NcMidiAudioProcessor();
}
