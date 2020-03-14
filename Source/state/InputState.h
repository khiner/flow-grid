#pragma once

#include <PluginManager.h>
#include <actions/DeleteProcessorAction.h>
#include <actions/CreateProcessorAction.h>
#include "JuceHeader.h"
#include "StatefulAudioProcessorContainer.h"
#include "Stateful.h"

class InputState : public Stateful, private ChangeListener, private ValueTree::Listener {
public:
    InputState(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer,
               PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : tracks(tracks), connections(connections), audioProcessorContainer(audioProcessorContainer),
              pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
        input = ValueTree(IDs::INPUT);
        input.addListener(this);
        deviceManager.addChangeListener(this);
    }

    ~InputState() override {
        deviceManager.removeChangeListener(this);
        input.removeListener(this);
    }

    ValueTree &getState() override { return input; }

    void loadFromState(const ValueTree &state) override {
        input.copyPropertiesFrom(state, nullptr);
        Utilities::moveAllChildren(state, input, nullptr);
    }

    void initializeDefault() {
        auto inputProcessor = CreateProcessorAction::createProcessor(pluginManager.getAudioInputDescription());
        input.appendChild(inputProcessor, &undoManager);
    }

    ValueTree getAudioInputProcessorState() const {
        return input.getChildWithProperty(IDs::name, pluginManager.getAudioInputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return input.getChildWithProperty(IDs::deviceName, push2MidiDeviceName);
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) {
            return StatefulAudioProcessorContainer::getNodeIdForState(getAudioInputProcessorState());
        } else if (connectionType == midi) {
            return StatefulAudioProcessorContainer::getNodeIdForState(getPush2MidiInputProcessor());
        } else {
            return {};
        }
    }

private:
    ValueTree input;

    TracksState &tracks;
    ConnectionsState &connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void syncInputDevicesWithDeviceManager() {
        Array<ValueTree> inputProcessorsToDelete;
        for (const auto &inputProcessor : input) {
            if (inputProcessor.hasProperty(IDs::deviceName)) {
                const String &deviceName = inputProcessor[IDs::deviceName];
                if (!MidiInput::getDevices().contains(deviceName) || !deviceManager.isMidiInputEnabled(deviceName)) {
                    inputProcessorsToDelete.add(inputProcessor);
                }
            }
        }
        for (const auto &inputProcessor : inputProcessorsToDelete) {
            undoManager.perform(new DeleteProcessorAction(inputProcessor, tracks, connections, audioProcessorContainer));
        }
        for (const auto &deviceName : MidiInput::getDevices()) {
            if (deviceManager.isMidiInputEnabled(deviceName) &&
                !input.getChildWithProperty(IDs::deviceName, deviceName).isValid()) {
                auto midiInputProcessor = CreateProcessorAction::createProcessor(MidiInputProcessor::getPluginDescription());
                midiInputProcessor.setProperty(IDs::deviceName, deviceName, nullptr);
                input.addChild(midiInputProcessor, -1, &undoManager);
            }
        }
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (source == &deviceManager) {
            deviceManager.updateEnabledMidiInputsAndOutputs();
            syncInputDevicesWithDeviceManager();
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            // TODO the undomanager behavior around this needs more thinking.
            //  This should be done along with the work to keep disabled IO devices in the graph if they still have connections
            input.setProperty(IDs::deviceName, config.inputDeviceName, nullptr);
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == MidiInputProcessor::name() && !deviceManager.isMidiInputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], true);
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[IDs::name] == MidiInputProcessor::name() && deviceManager.isMidiInputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], false);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::deviceName && tree == input) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.inputDeviceName = tree[IDs::deviceName];
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
