#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include <push2/Push2MidiCommunicator.h>
#include "push2/Push2DisplayBridge.h"
#include "ProcessorGraph.h"
#include "Push2ProcessorView.h"
#include "Push2ProcessorSelector.h"
#include "Push2ComponentBase.h"

class Push2Component :
        public Timer,
        public Push2ComponentBase,
        private ProjectChangeListener {
public:
    explicit Push2Component(Project &project, Push2MidiCommunicator &push2MidiCommunicator, ProcessorGraph &audioGraphBuilder)
            : Push2ComponentBase(project, push2MidiCommunicator), audioGraphBuilder(audioGraphBuilder),
              processorView(project, push2MidiCommunicator), processorSelector(project, push2MidiCommunicator) {
        startTimer(60);

        addChildComponent(processorView);
        addChildComponent(processorSelector);

        project.addProjectChangeListener(this);
        setBounds(0, 0, Push2Display::WIDTH, Push2Display::HEIGHT);
        processorView.setBounds(getLocalBounds());
        processorSelector.setBounds(getLocalBounds());
        push2MidiCommunicator.setAddTrackButtonEnabled(true);
    }

    ~Push2Component() override {
        setVisible(false);
        project.removeProjectChangeListener(this);
    }

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        push2MidiCommunicator.setAddTrackButtonEnabled(false);
        push2MidiCommunicator.setAddDeviceButtonEnabled(false);
    }

    void masterEncoderRotated(float changeAmount) override {
        auto *masterGainParameter = audioGraphBuilder.getMasterGainProcessor()->getParameter(1);
        if (masterGainParameter != nullptr)
            masterGainParameter->setValue(masterGainParameter->getValue() + changeAmount);
    }
    
    void encoderRotated(int encoderIndex, float changeAmount) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->encoderRotated(encoderIndex, changeAmount);
        }
    }
    
    void undoButtonPressed(bool shiftHeld) override {
        if (shiftHeld)
            project.getUndoManager().redo();
        else
            project.getUndoManager().undo();
    }
    
    void addTrackButtonPressed(bool shiftHeld) override {
        getCommandManager().invokeDirectly(shiftHeld ? CommandIDs::insertTrackWithoutMixer : CommandIDs::insertTrack, false);
    }
    
    void deleteButtonPressed() override {
        getCommandManager().invokeDirectly(CommandIDs::deleteSelected, false);
    }
    
    void addDeviceButtonPressed() override {
        selectChild(&processorSelector);
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->aboveScreenButtonPressed(buttonIndex);
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->belowScreenButtonPressed(buttonIndex);
        }
    }

    void arrowPressed(Direction direction) override {
        if (currentlyViewingChild != nullptr) {
            currentlyViewingChild->arrowPressed(direction);
            return;
        }
        switch (direction) {
            case Direction::up: return project.moveSelectionUp();
            case Direction::down: return project.moveSelectionDown();
            case Direction::left: return project.moveSelectionLeft();
            case Direction::right: return project.moveSelectionRight();
            default: return;
        }
    }

private:
    Push2DisplayBridge displayBridge;
    ProcessorGraph &audioGraphBuilder;

    Push2ProcessorView processorView;
    Push2ProcessorSelector processorSelector;

    Push2ComponentBase *currentlyViewingChild {};

    void selectChild(Push2ComponentBase* child) {
        if (currentlyViewingChild == child)
            return;

        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(false);

        currentlyViewingChild = child;
        if (currentlyViewingChild != nullptr)
            currentlyViewingChild->setVisible(true);
    }

    // Render a frame and send it to the Push 2 display
    void inline drawFrame() {
        static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

        auto &g = displayBridge.getGraphics();
        g.fillAll(CLEAR_COLOR);
        paintEntireComponent(g, true);
        displayBridge.writeFrameToDisplay();
    }

    void timerCallback() override { drawFrame(); }

    void itemSelected(const ValueTree& item) override {
        if (item.hasType(IDs::PROCESSOR)) {
            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(item)) {
                processorView.setProcessor(processorWrapper);
                selectChild(&processorView);
            }
            push2MidiCommunicator.setAddDeviceButtonEnabled(true);
        } else if (item.hasType(IDs::TRACK) || item.hasType(IDs::MASTER_TRACK)) {
            push2MidiCommunicator.setAddDeviceButtonEnabled(true);
        } else {
            selectChild(nullptr);
            processorView.setProcessor(nullptr);
            push2MidiCommunicator.setAddDeviceButtonEnabled(false);
        }
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedProcessor() || item == project.getSelectedTrack()) {
            itemSelected(ValueTree());
        }
    }
};
