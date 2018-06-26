#include <JuceHeader.h>
#include <drow/drow_Utilities.h>
#include <drow/Identifiers.h>
#include <drow/ValueTreeItems.h>
#include <drow/SelectionPanel.h>

class ValueTreesDemo : public Component,
                       public DragAndDropContainer,
                       private Button::Listener,
                       private Timer {
public:
    ValueTreesDemo(ValueTree editToUse, UndoManager &undoManager) : undoManager(undoManager) {
        addAndMakeVisible(tree);
        tree.setColour(TreeView::backgroundColourId,
                       getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));

        tree.setDefaultOpenness(true);
        tree.setMultiSelectEnabled(true);

        tree.setRootItem((rootItem = std::make_unique<Edit>(editToUse, undoManager)).get());

        addAndMakeVisible(*(selectionPanel = std::make_unique<SelectionPanel>(tree, *rootItem)));

        addAndMakeVisible(undoButton);
        addAndMakeVisible(redoButton);
        undoButton.addListener(this);
        redoButton.addListener(this);

        startTimer(500);

        setSize(800, 600);
    }

    ~ValueTreesDemo() {
        tree.setRootItem(nullptr);
    }

    void resized() override {
        Rectangle<int> r(getLocalBounds().reduced(4));

        Rectangle<int> buttons(r.removeFromBottom(22));
        undoButton.setBounds(buttons.removeFromLeft(100));
        buttons.removeFromLeft(6);
        redoButton.setBounds(buttons.removeFromLeft(100));

        r.removeFromBottom(4);
        selectionPanel->setBounds(r.removeFromBottom(120));

        r.removeFromBottom(4);
        tree.setBounds(r);
    }

    void deleteSelectedItems() {
        auto selectedItems(Helpers::getSelectedTreeViewItems<ValueTreeItem>(tree));

        for (int i = selectedItems.size(); --i >= 0;) {
            ValueTree &v = *selectedItems.getUnchecked(i);

            if (v.getParent().isValid())
                v.getParent().removeChild(v, &undoManager);
        }
    }

    bool keyPressed(const KeyPress &key) override {
        if (key == KeyPress::deleteKey) {
            deleteSelectedItems();
            return true;
        }

        if (key == KeyPress('z', ModifierKeys::commandModifier, 0)) {
            undoManager.undo();
            return true;
        }

        if (key == KeyPress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)) {
            undoManager.redo();
            return true;
        }

        return Component::keyPressed(key);
    }

    void buttonClicked(Button *b) override {
        if (b == &undoButton)
            undoManager.undo();
        else if (b == &redoButton)
            undoManager.redo();
    }

private:
    TreeView tree;
    TextButton undoButton{"Undo"}, redoButton{"Redo"};
    std::unique_ptr<Edit> rootItem;
    UndoManager &undoManager;

    std::unique_ptr<SelectionPanel> selectionPanel;

    void timerCallback() override {
        undoManager.beginNewTransaction();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreesDemo)
};