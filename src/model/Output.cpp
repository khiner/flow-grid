#include "Output.h"

#include "action/CreateProcessor.h"
#include "processors/MidiOutputProcessor.h"

Output::Output(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
        : pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
    state.addListener(this);
}

Output::~Output() {
    state.removeListener(this);
}

Array<ValueTree> Output::syncOutputDevicesWithDeviceManager() {
    Array<ValueTree> outputProcessorsToDelete;
    for (const auto &outputProcessor : state) {
        if (outputProcessor.hasProperty(ProcessorIDs::deviceName)) {
            const String &deviceName = Processor::getDeviceName(outputProcessor);
            if (!MidiOutput::getDevices().contains(deviceName) || !deviceManager.isMidiOutputEnabled(deviceName)) {
                outputProcessorsToDelete.add(outputProcessor);
            }
        }
    }
    for (const auto &deviceName : MidiOutput::getDevices()) {
        if (deviceManager.isMidiOutputEnabled(deviceName) &&
            !state.getChildWithProperty(ProcessorIDs::deviceName, deviceName).isValid()) {
            auto midiOutputProcessor = CreateProcessor::createProcessor(MidiOutputProcessor::getPluginDescription());
            Processor::setDeviceName(midiOutputProcessor, deviceName);
            state.addChild(midiOutputProcessor, -1, &undoManager);
        }
    }
    return outputProcessorsToDelete;
}

void Output::initializeDefault() {
    auto outputProcessor = CreateProcessor::createProcessor(pluginManager.getAudioOutputDescription());
    state.appendChild(outputProcessor, &undoManager);
}

void Output::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (Processor::isMidiOutputProcessor(child) &&
        !deviceManager.isMidiOutputEnabled(Processor::getDeviceName(child)))
        deviceManager.setMidiOutputEnabled(Processor::getDeviceName(child), true);
}

void Output::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (Processor::isMidiOutputProcessor(child) &&
        deviceManager.isMidiOutputEnabled(Processor::getDeviceName(child)))
        deviceManager.setMidiOutputEnabled(Processor::getDeviceName(child), false);
}

void Output::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (i == ProcessorIDs::deviceName && tree == state) {
        AudioDeviceManager::AudioDeviceSetup config;
        deviceManager.getAudioDeviceSetup(config);
        config.outputDeviceName = Processor::getDeviceName(tree);
        deviceManager.setAudioDeviceSetup(config, true);
    }
}