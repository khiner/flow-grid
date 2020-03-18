#pragma once

#include <state/OutputState.h>
#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

// UpdateAllDefaultConnectionsAction should be performed after this
struct InsertAction : UndoableAction {
    InsertAction(bool duplicate, const ValueTree &copiedState, const juce::Point<int> toTrackAndSlot,
                 TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input,
                 StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), view(view), audioProcessorContainer(audioProcessorContainer),
              fromTrackAndSlot(findFromTrackAndSlot(copiedState)), toTrackAndSlot(limitToTrackAndSlot(toTrackAndSlot, copiedState)) {
        auto trackAndSlotDiff = this->toTrackAndSlot - this->fromTrackAndSlot;

        if (!duplicate && tracks.getMasterTrack().isValid() && toTrackAndSlot.x == tracks.getNumNonMasterTracks()) {
            // When inserting into master track, only insert the processors of the first track with selections
            copyProcessorsFromTrack(copiedState.getChild(fromTrackAndSlot.x), tracks.getNumNonMasterTracks(), trackAndSlotDiff.y);
        } else {
            // First pass: insert processors that are selected without their parent track also selected.
            // This is done because adding new tracks changes the track indices relative to their current position.
            for (const auto &copiedTrack : copiedState) {
                if (!copiedTrack[IDs::selected]) {
                    if (duplicate) {
                        duplicateSelectedProcessors(copiedTrack, copiedState);
                    } else if (tracks.isMasterTrack(copiedTrack)) {
                        // Processors copied from master track can only get inserted into master track.
                        const auto &masterTrack = tracks.getMasterTrack();
                        if (masterTrack.isValid())
                            copyProcessorsFromTrack(copiedTrack, tracks.indexOf(masterTrack), trackAndSlotDiff.y);
                    } else {
                        int toTrackIndex = copiedState.indexOf(copiedTrack) + trackAndSlotDiff.x;
                        if (copiedTrack.getNumChildren() > 0) { // create tracks to make room
                            while (toTrackIndex >= tracks.getNumNonMasterTracks()) {
                                addAndPerformAction(new CreateTrackAction(false, false, {}, tracks, view));
                            }
                        }
                        if (toTrackIndex < tracks.getNumNonMasterTracks())
                            copyProcessorsFromTrack(copiedTrack, toTrackIndex, trackAndSlotDiff.y);
                    }
                }
            }
            // Second pass: insert selected tracks (along with their processors)
            const auto selectedTrackIndices = findSelectedNonMasterTrackIndices(copiedState);
            if (duplicate) {
                const auto duplicatedTrackIndices = findDuplicationIndices(selectedTrackIndices);
                for (int i = 0; i < selectedTrackIndices.size(); i++)
                    addAndPerformCreateTrackAction(copiedState.getChild(selectedTrackIndices[i]), duplicatedTrackIndices[i]);
            } else {
                for (int i = 0; i < selectedTrackIndices.size(); i++)
                    addAndPerformCreateTrackAction(copiedState.getChild(selectedTrackIndices[i]),
                                                   selectedTrackIndices[i] + trackAndSlotDiff.x + 1);
            }
        }

        selectAction = std::make_unique<MoveSelectionsAction>(createActions, tracks, connections, view, input, audioProcessorContainer);
        //selectAction->setNewFocusedSlot(newFocusedSlot);

        // Cleanup
        for (int i = createActions.size() - 1; i >= 0; i--) {
            auto *action = createActions.getUnchecked(i);
            if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(action))
                createProcessorAction->undoTemporary();
            else
                action->undo();
        }
    }

    bool perform() override {
        if (createActions.isEmpty())
            return false;

        for (auto *createAction : createActions)
            createAction->perform();
        selectAction->perform();

        return true;
    }

    bool undo() override {
        if (createActions.isEmpty())
            return false;

        selectAction->undo();
        for (int i = createActions.size() - 1; i >= 0; i--)
            createActions.getUnchecked(i)->undo();

        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:

    TracksState &tracks;
    ViewState &view;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    juce::Point<int> fromTrackAndSlot;
    juce::Point<int> toTrackAndSlot;

    OwnedArray<UndoableAction> createActions;

    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(const OwnedArray<UndoableAction> &createActions,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
                : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
            for (int i = 0; i < newTrackSelections.size(); i++) {
                newTrackSelections.setUnchecked(i, false);
                newSelectedSlotsMasks.setUnchecked(i, BigInteger().toString(2));
            }

            for (auto *createAction : createActions) {
                if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(createAction)) {
                    String maskString = newSelectedSlotsMasks.getUnchecked(createProcessorAction->trackIndex);
                    BigInteger mask;
                    mask.parseString(maskString, 2);
                    mask.setBit(createProcessorAction->slot, true);
                    newSelectedSlotsMasks.setUnchecked(createProcessorAction->trackIndex, mask.toString(2));
                } else if (auto *createTrackAction = dynamic_cast<CreateTrackAction *>(createAction)) {
                    newTrackSelections.setUnchecked(createTrackAction->insertIndex, true);
                    const auto &track = tracks.getTrack(createTrackAction->insertIndex);
                    newSelectedSlotsMasks.setUnchecked(createTrackAction->insertIndex, tracks.createFullSelectionBitmask(track));
                }
            }
        }
    };

    std::unique_ptr<SelectAction> selectAction;

    juce::Point<int> findFromTrackAndSlot(const ValueTree &copiedState) {
        int fromTrackIndex = getIndexOfFirstCopiedTrackWithSelections(copiedState);
        if (anyCopiedTrackSelected(copiedState))
            return {fromTrackIndex, 0};

        int fromSlot = INT_MAX;
        for (const auto &track : copiedState) {
            int lowestSelectedSlotForTrack = tracks.getSlotMask(track).findNextSetBit(0);
            if (lowestSelectedSlotForTrack != -1)
                fromSlot = std::min(fromSlot, lowestSelectedSlotForTrack);
        }

        assert(fromSlot != INT_MAX);
        return {fromTrackIndex, fromSlot};
    }

    static juce::Point<int> limitToTrackAndSlot(juce::Point<int> toTrackAndSlot, const ValueTree &copiedState) {
        return {toTrackAndSlot.x, anyCopiedTrackSelected(copiedState) ? 0 : toTrackAndSlot.y};
    }

    static bool anyCopiedTrackSelected(const ValueTree &copiedState) {
        for (const ValueTree &track : copiedState)
            if (track[IDs::selected])
                return true;

        return false;
    }

    std::vector<int> findSelectedNonMasterTrackIndices(const ValueTree &copiedState) const {
        std::vector<int> selectedTrackIndices;
        for (const auto &track : copiedState)
            if (track[IDs::selected] && !tracks.isMasterTrack(track))
                selectedTrackIndices.push_back(copiedState.indexOf(track));
        return selectedTrackIndices;
    }

    int getIndexOfFirstCopiedTrackWithSelections(const ValueTree &copiedState) {
        for (const auto &track : copiedState)
            if (track[IDs::selected] || tracks.trackHasAnySlotSelected(track))
                return copiedState.indexOf(track);

        assert(false); // Copied state, by definition, must have a selection.
    }

    void duplicateSelectedProcessors(const ValueTree &track, const ValueTree &copiedState) {
        const BigInteger slotsMask = TracksState::getSlotMask(track);
        std::vector<int> selectedSlots;
        for (int slot = 0; slot <= std::min(tracks.getMixerChannelSlotForTrack(track) - 1, slotsMask.getHighestBit()); slot++)
            if (slotsMask[slot])
                selectedSlots.push_back(slot);

        auto duplicatedSlots = findDuplicationIndices(selectedSlots);
        int trackIndex = tracks.indexOf(track);
        for (int i = 0; i < selectedSlots.size(); i++)
            addAndPerformCreateProcessorAction(TracksState::getProcessorAtSlot(track, selectedSlots[i]),
                                               copiedState.indexOf(track), duplicatedSlots[i]);
    }

    void copyProcessorsFromTrack(const ValueTree &fromTrack, int toTrackIndex, int slotDiff) {
        const BigInteger slotsMask = TracksState::getSlotMask(fromTrack);
        for (int slot = 0; slot <= slotsMask.getHighestBit(); slot++)
            if (slotsMask[slot])
                addAndPerformCreateProcessorAction(TracksState::getProcessorAtSlot(fromTrack, slot), toTrackIndex, slot + slotDiff);
    }

    void addAndPerformAction(UndoableAction *action) {
        if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(action))
            createProcessorAction->performTemporary();
        else
            action->perform();
        createActions.add(action);
        // TODO focusedSlot
//        if (oldFocusedSlot.x == fromTrackIndex && oldFocusedSlot.y == fromSlot)
//            newFocusedSlot = {toTrackIndex, toSlot};
    }

    void addAndPerformCreateProcessorAction(const ValueTree &processor, int toTrackIndex, int toSlot) {
        addAndPerformAction(new CreateProcessorAction(processor.createCopy(), toTrackIndex, toSlot, tracks, view, audioProcessorContainer));
    }

    void addAndPerformCreateTrackAction(const ValueTree &track, int toTrackIndex) {
        addAndPerformAction(new CreateTrackAction(toTrackIndex, false, false, track, tracks, view));
        for (const auto &processor : track)
            addAndPerformCreateProcessorAction(processor, toTrackIndex, processor[IDs::processorSlot]);
    }

    static std::vector<int> findDuplicationIndices(std::vector<int> currentIndices) {
        auto duplicationIndices = currentIndices;
        int previousIndex = -1;
        int endOfContiguousRange = 0;
        for (int i = 0; i < duplicationIndices.size(); i++) {
            int currentIndex = currentIndices[i];
            if (previousIndex != -1 && currentIndex - previousIndex > 1)
                endOfContiguousRange = i;
            for (int j = endOfContiguousRange; j < duplicationIndices.size(); j++)
                duplicationIndices[j] += 1;
            previousIndex = currentIndex;
        }

        return duplicationIndices;
    }

    JUCE_DECLARE_NON_COPYABLE(InsertAction)
};
