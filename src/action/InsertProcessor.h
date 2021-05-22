#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "model/Tracks.h"
#include "model/View.h"

using namespace juce;

// Inserting a processor "pushes" any contiguous set of processors starting at the given slot down.
// (The new processor always "wins" by keeping its given slot.)
// Doesn't take care of any select actions! (Caller is responsible for that.)
struct InsertProcessor : UndoableAction {
    // slot of -1 used for track-level processors (track IO processors that aren't in a lane).
    InsertProcessor(const ValueTree &processor, int toTrackIndex, int toSlot, Tracks &tracks, View &view);

    bool perform() override;
    bool undo() override;

    int getSizeInUnits() override { return (int) sizeof(*this); }

private:
    struct SetProcessorSlotAction : public UndoableAction {
        SetProcessorSlotAction(int trackIndex, const ValueTree &processor, int newSlot,
                               Tracks &tracks, View &view);

        bool perform() override;
        bool undo() override;

    private:
        ValueTree processor;
        int oldSlot, newSlot;
        std::unique_ptr<SetProcessorSlotAction> pushConflictingProcessorAction;

        struct AddProcessorRowAction : public UndoableAction {
            AddProcessorRowAction(int trackIndex, Tracks &tracks, View &view);

            bool perform() override;
            bool undo() override;

        private:
            int trackIndex;
            Tracks &tracks;
            View &view;
        };

        OwnedArray<AddProcessorRowAction> addProcessorRowActions;
    };

    struct AddOrMoveProcessorAction : public UndoableAction {
        AddOrMoveProcessorAction(const ValueTree &processor, int newTrackIndex, int newSlot, Tracks &tracks, View &view);

        bool perform() override;
        bool undo() override;

    private:
        ValueTree processor;
        int oldTrackIndex, newTrackIndex;
        int oldSlot, newSlot;
        int oldIndex, newIndex;
        std::unique_ptr<SetProcessorSlotAction> setProcessorSlotAction;
        Tracks &tracks;
    };

    AddOrMoveProcessorAction addOrMoveProcessorAction;
};