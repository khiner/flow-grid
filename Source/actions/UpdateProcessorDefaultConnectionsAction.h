#pragma once

#include "JuceHeader.h"
#include "CreateConnectionAction.h"

#include <Identifiers.h>
#include <StatefulAudioProcessorContainer.h>
#include <state_managers/InputStateManager.h>

// Disconnect external audio/midi inputs (unless `addDefaultConnections` is true and
// the default connection would stay the same).
// If `addDefaultConnections` is true, then for both audio and midi connection types:
//   * Find the topmost effect processor (receiving audio/midi) in the focused track
//   * Connect external device inputs to its most-upstream connected processor (including itself)
// (Note that it is possible for the same focused track to have a default audio-input processor different
// from its default midi-input processor.)
struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    
    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            ConnectionsStateManager &connectionsManager, StatefulAudioProcessorContainer &audioProcessorContainer)
            : CreateOrDeleteConnectionsAction(connectionsManager) {
        for (auto connectionType : {audio, midi}) {
            auto processorToConnectTo = connectionsManager.findProcessorToFlowInto(processor.getParent(), processor, connectionType);
            auto nodeIdToConnectTo = StatefulAudioProcessorContainer::getNodeIdForState(processorToConnectTo);

            auto disconnectDefaultsAction = DisconnectProcessorAction(connectionsManager, processor, connectionType, true, false, false, true, nodeIdToConnectTo);
            coalesceWith(disconnectDefaultsAction);
            if (makeInvalidDefaultsIntoCustom && !disconnectDefaultsAction.connectionsToDelete.isEmpty()) {
                for (const auto& connectionToConvert : disconnectDefaultsAction.connectionsToDelete) {
                    auto customConnection = connectionToConvert.createCopy();
                    customConnection.setProperty(IDs::isCustomConnection, true, nullptr);
                    connectionsToCreate.add(customConnection);
                }
            } else {
                coalesceWith(DefaultConnectProcessorAction(processor, nodeIdToConnectTo, connectionType, connectionsManager, audioProcessorContainer));
            }
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE(UpdateProcessorDefaultConnectionsAction)
};
