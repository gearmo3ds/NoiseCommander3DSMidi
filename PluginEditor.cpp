/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::String getLocalIPAddress()
{
    auto interfaces = juce::IPAddress::getAllAddresses();


    for (const auto& address : interfaces)
    {
//        DBG(address.toString());

        if (address != juce::IPAddress::local(false)  // not 127.0.0.1
            && address != juce::IPAddress::local(true) // not ::1
            && address.isIPv6)
        {
            return address.toString();  // Return first non-localhost IPv4
        }
    }

    return "0.0.0.0"; // Fallback
}

juce::StringArray getUsableIPAddresses()
{
    juce::StringArray result;
    auto interfaces = juce::IPAddress::getAllAddresses();

    for (const auto& address : interfaces)
    {
        auto ip = address.toString();

        // Exclude loopback and wildcard
        if (ip == "127.0.0.1" || ip == "0.0.0.0")
            continue;

        result.add(ip);
    }

    return result;
}

//==============================================================================
NcMidiAudioProcessorEditor::NcMidiAudioProcessorEditor (NcMidiAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 400);

    midiLog.setMultiLine(true);
    midiLog.setReadOnly(true);
    midiLog.setScrollbarsShown(true);
    midiLog.setCaretVisible(false);
    midiLog.setPopupMenuEnabled(false);
    midiLog.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    midiLog.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(midiLog);

    // Add Clear button
    addAndMakeVisible(clearButton);
    clearButton.onClick = [this]()
    {
        midiLog.clear();
    };

    // Add Max Lines slider
    addAndMakeVisible(maxLinesSlider);
    maxLinesSlider.setRange(10, 1000, 10);
    maxLinesSlider.setValue(maxLines);
    maxLinesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    maxLinesSlider.onValueChange = [this]()
    {
        maxLines = static_cast<int>(maxLinesSlider.getValue());
    };

    addAndMakeVisible(enableLoggingToggle);
    juce::PropertiesFile* props = audioProcessor.appProperties.getUserSettings();
    loggingEnabled = props->getBoolValue("logging_enabled", true);
    enableLoggingToggle.setToggleState(loggingEnabled, juce::dontSendNotification);
    enableLoggingToggle.onClick = [this]()
    {
        loggingEnabled = enableLoggingToggle.getToggleState();
        juce::PropertiesFile* props = audioProcessor.appProperties.getUserSettings();
        props->setValue("logging_enabled", loggingEnabled);
        props->saveIfNeeded();
    };

    startTimerHz(20); // 20 times per second
}

NcMidiAudioProcessorEditor::~NcMidiAudioProcessorEditor()
{
}

//==============================================================================
void NcMidiAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void NcMidiAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    auto area = getLocalBounds();
    auto topArea = area.removeFromTop(30);
    auto botArea = area.removeFromBottom(60);

    selfIpSelector.setBounds(topArea.removeFromLeft(topArea.getWidth()/2));
    dsIpSelector.setBounds(topArea);

    midiLog.setBounds(area);

    auto row1 = botArea.removeFromTop(30);
    discoverButton.setBounds(row1.removeFromLeft(row1.getWidth()/2));
    clearButton.setBounds(row1);

    auto row2 = botArea;
    maxLinesSlider.setBounds(row2.removeFromLeft(getWidth()/2));
    enableLoggingToggle.setBounds(row2);
}

void NcMidiAudioProcessorEditor::startDiscovery() {
    if (isDiscovering)
        return;

    audioProcessor.pushMidiMessage("Listening for 3DS broadcast signal ...");
    isDiscovering = true;
    std::thread([this]() {
        using namespace juce;

        //DBG("Starting discovery ...");

        DatagramSocket socket;
        if (!socket.bindToPort(5005)) {
            DBG("Failed to bind to port 5005");
            return;
        }

        char buffer[64];
        juce::String senderAddress;
        int senderPort = 0;

        int bytes = socket.read(buffer, sizeof(buffer), true, senderAddress, senderPort);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            juce::String msg(buffer);

            if (msg.contains("HELLO_PC")) {
                //DBG("Got hello from 3DS: " + senderAddress);

                // Optional: send response
                socket.write(senderAddress, 5005, "HELLO_3DS", 9);


                // Update GUI / stored IP
                MessageManager::callAsync([this, senderAddress]() {
                    dsIpSelector.setText(senderAddress, juce::dontSendNotification);
                    audioProcessor.appProperties.getUserSettings()->setValue("3ds_ip", senderAddress);
                    audioProcessor.set3DSIPAddress(senderAddress);
                    audioProcessor.pushMidiMessage("Found 3DS IP-Address " + senderAddress);
                });
            }
        } else {
            DBG("No broadcast received.");
        }
        isDiscovering = false;
    }).detach(); // Run in background thread
}


void NcMidiAudioProcessorEditor::parentHierarchyChanged()
{
    if (isShowing() && !initialized)
    {
        initialized = true;
        DBG("Doing one-time GUI logic");




        // Only here, get IP, update UI, etc.
        auto addresses = getUsableIPAddresses();

        int index = 1;
        for (const auto& ip : addresses) {
            selfIpSelector.addItem(ip, index++);
        }

        selfIpSelector.onChange = [this]() {
            selfIpAddress = selfIpSelector.getText();
//            DBG("User selected IP: " + selfIpAddress);
        };

        addAndMakeVisible(selfIpSelector);

        if (addresses.size() > 0) {
            selfIpAddress = addresses[0];
            selfIpSelector.setSelectedId(1); // First IP in the list
        }

        // Label
//        ipLabel.setText("3DS IP:", juce::dontSendNotification);
//        ipLabel.attachToComponent(&dsIpSelector, true);
//        addAndMakeVisible(ipLabel);

        // 3DS IP-Address Editor
        juce::PropertiesFile* props = audioProcessor.appProperties.getUserSettings();
        DBG("Settings file location: " + props->getFile().getFullPathName());
        dsIpAddress = props->getValue("3ds_ip", "192.168.1.0");
        dsIpSelector.setText(dsIpAddress);
        audioProcessor.set3DSIPAddress(dsIpAddress);
        dsIpSelector.setInputRestrictions(0, "0123456789."); // restrict to digits and dots
        dsIpSelector.onTextChange = [this]() {
            auto text = dsIpSelector.getText().trim();
            DBG(text);
            audioProcessor.set3DSIPAddress(text);

            juce::PropertiesFile* props = audioProcessor.appProperties.getUserSettings();
            props->setValue("3ds_ip", text);
            props->saveIfNeeded();
        };
        addAndMakeVisible(dsIpSelector);

        addAndMakeVisible(discoverButton);
        discoverButton.setButtonText("Discover 3DS");
        discoverButton.onClick = [this]() { startDiscovery(); };
    }
}

void NcMidiAudioProcessorEditor::timerCallback()
{
    if (!loggingEnabled || !isShowing())
        return;

    constexpr int maxMessagesPerTick = 50;

    auto messages = audioProcessor.popMidiMessages();
    int count = 0;

    for (const auto& msg : messages)
    {
        if (count++ >= maxMessagesPerTick)
            break;

        midiLog.moveCaretToEnd();
        midiLog.insertTextAtCaret(msg + "\n");
    }

    // Scrollback trimming only every N ticks
    static int trimCounter = 0;
    if (++trimCounter % 3 == 0)
    {
        juce::String currentText = midiLog.getText();
        juce::StringArray lines;
        lines.addLines(currentText);

        if (lines.size() > maxLines)
        {
            while (lines.size() > maxLines)
                lines.remove(0);

            midiLog.setText(lines.joinIntoString("\n"), juce::dontSendNotification);
            midiLog.moveCaretToEnd();
        }
    }
}
