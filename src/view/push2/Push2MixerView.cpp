#include "Push2MixerView.h"

Push2MixerView::Push2MixerView(View &view, Tracks &tracks, Project &project, StatefulAudioProcessorWrappers &processorWrappers, Push2MidiCommunicator &push2MidiCommunicator)
    : Push2TrackManagingView(view, tracks, project, push2MidiCommunicator),
      processorWrappers(processorWrappers),
      volumesLabel(0, true, push2MidiCommunicator), pansLabel(1, true, push2MidiCommunicator),
      volumeParametersPanel(1), panParametersPanel(1) {
    volumesLabel.setText("Volumes", dontSendNotification);
    pansLabel.setText("Pans", dontSendNotification);
    volumesLabel.setUnderlined(true);
    pansLabel.setUnderlined(true);

    addAndMakeVisible(volumesLabel);
    addAndMakeVisible(pansLabel);
    addChildComponent(volumeParametersPanel);
    addChildComponent(panParametersPanel);
    selectPanel(&volumeParametersPanel);
}

Push2MixerView::~Push2MixerView() {
    volumeParametersPanel.clearParameters();
    panParametersPanel.clearParameters();
}

void Push2MixerView::resized() {
    Push2TrackManagingView::resized();

    auto r = getLocalBounds();
    auto top = r.removeFromTop(HEADER_FOOTER_HEIGHT);
    auto labelWidth = getWidth() / NUM_COLUMNS;
    volumesLabel.setBounds(top.removeFromLeft(labelWidth));
    pansLabel.setBounds(top.removeFromLeft(labelWidth));
    r.removeFromBottom(HEADER_FOOTER_HEIGHT);
    volumeParametersPanel.setBounds(r);
    panParametersPanel.setBounds(r);
}

void Push2MixerView::aboveScreenButtonPressed(int buttonIndex) {
    if (buttonIndex == 0)
        selectPanel(&volumeParametersPanel);
    else if (buttonIndex == 1)
        selectPanel(&panParametersPanel);
}

void Push2MixerView::encoderRotated(int encoderIndex, float changeAmount) {
    if (auto *parameter = selectedParametersPanel->getParameterOnCurrentPageAt(encoderIndex)) {
        parameter->setValue(std::clamp(parameter->getValue() + changeAmount, 0.0f, 1.0f));
    }
}

void Push2MixerView::updateEnabledPush2Buttons() {
    Push2TrackManagingView::updateEnabledPush2Buttons();
    volumesLabel.setVisible(isVisible());
    pansLabel.setVisible(isVisible());
    if (isVisible())
        push2.activateWhiteLedButton(Push2::mix);
    else
        push2.enableWhiteLedButton(Push2::mix);
}

void Push2MixerView::onChildAdded(Track *track) {
    Push2TrackManagingView::onChildAdded(track);
    updateParameters();
}

void Push2MixerView::onChildRemoved(Track *track, int oldIndex) {
    Push2TrackManagingView::onChildRemoved(track, oldIndex);
    updateParameters();
}

void Push2MixerView::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    Push2TrackManagingView::valueTreePropertyChanged(tree, i);
    if (i == ProcessorIDs::initialized && tree[i])
        updateParameters();
}

void Push2MixerView::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) {
    Push2TrackManagingView::valueTreeChildRemoved(exParent, child, index);
    if (Processor::isType(child)) {
        updateParameters();
    }
}

void Push2MixerView::updateParameters() {
    volumeParametersPanel.clearParameters();
    panParametersPanel.clearParameters();

    const auto *focusedTrack = tracks.getFocusedTrack();
    if (focusedTrack != nullptr && focusedTrack->isMaster()) {
        const auto *trackOutputProcessor = focusedTrack->getOutputProcessor();
        if (auto *processorWrapper = processorWrappers.getProcessorWrapperForProcessor(trackOutputProcessor)) {
            volumeParametersPanel.addParameter(processorWrapper->getParameter(1));
            panParametersPanel.addParameter(processorWrapper->getParameter(0));
        }
    } else {
        for (const auto *track: tracks.getChildren()) {
            if (!track->isMaster()) {
                const auto *trackOutputProcessor = track->getOutputProcessor();
                if (auto *processorWrapper = processorWrappers.getProcessorWrapperForProcessor(trackOutputProcessor)) {
                    // TODO use param identifiers instead of indexes
                    volumeParametersPanel.addParameter(processorWrapper->getParameter(1));
                    panParametersPanel.addParameter(processorWrapper->getParameter(0));
                } else {
                    volumeParametersPanel.addParameter(nullptr);
                    panParametersPanel.addParameter(nullptr);
                }
            }
        }
    }
}

void Push2MixerView::selectPanel(ParametersPanel *panel) {
    if (selectedParametersPanel == panel) return;

    if (selectedParametersPanel != nullptr)
        selectedParametersPanel->setVisible(false);

    selectedParametersPanel = panel;
    if (selectedParametersPanel != nullptr)
        selectedParametersPanel->setVisible(true);

    volumesLabel.setSelected(false);
    pansLabel.setSelected(false);

    if (selectedParametersPanel == &volumeParametersPanel)
        volumesLabel.setSelected(true);
    else if (selectedParametersPanel == &panParametersPanel)
        pansLabel.setSelected(true);
}
