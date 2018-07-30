#include <memory>

#pragma once

#include "JuceHeader.h"

class ProcessorEditor : public AudioProcessorEditor {
public:
    explicit ProcessorEditor(AudioProcessor *const p) : AudioProcessorEditor(p) {
        jassert(p != nullptr);
        setOpaque(true);

        view.setViewedComponent(new ParametersPanel(*p, p->getParameters()));
        addAndMakeVisible(view);

        view.setScrollBarsShown(true, false);
        setSize(view.getViewedComponent()->getWidth() + view.getVerticalScrollBar().getWidth(),
                jmin(view.getViewedComponent()->getHeight(), 400));
    }

    ~ProcessorEditor() override = default;

    void paint(Graphics &g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        view.setBounds(getLocalBounds());
    }

private:
    Viewport view;

    class ParameterListener : private AudioProcessorParameter::Listener,
                              private AudioProcessorListener,
                              private Timer {
    public:
        ParameterListener(AudioProcessor &p, AudioProcessorParameter &param)
                : processor(p), parameter(param) {
            parameter.addListener(this);
            startTimer(100);
        }

        ~ParameterListener() override {
            parameter.removeListener(this);
        }

        AudioProcessorParameter &getParameter() noexcept {
            return parameter;
        }

        virtual void handleNewParameterValue() = 0;

    private:
        //==============================================================================
        void parameterValueChanged(int, float) override {
            parameterValueHasChanged = 1;
        }

        void parameterGestureChanged(int, bool) override {}

        //==============================================================================
        void audioProcessorParameterChanged(AudioProcessor *, int index, float) override {
            if (index == parameter.getParameterIndex())
                parameterValueHasChanged = 1;
        }

        void audioProcessorChanged(AudioProcessor *) override {}

        //==============================================================================
        void timerCallback() override {
            if (parameterValueHasChanged.compareAndSetBool(0, 1)) {
                handleNewParameterValue();
                startTimerHz(50);
            } else {
                startTimer(jmin(250, getTimerInterval() + 10));
            }
        }

        AudioProcessor &processor;
        AudioProcessorParameter &parameter;
        Atomic<int> parameterValueHasChanged{0};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterListener)
    };

    class BooleanParameterComponent final : public Component,
                                            private ParameterListener {
    public:
        BooleanParameterComponent(AudioProcessor &processor, AudioProcessorParameter &param)
                : ParameterListener(processor, param) {
            // Set the initial value.
            handleNewParameterValue();

            button.onClick = [this]() { buttonClicked(); };

            addAndMakeVisible(button);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds();
            area.removeFromLeft(8);
            button.setBounds(area.reduced(0, 10));
        }

    private:
        void handleNewParameterValue() override {
            auto parameterState = getParameterState(getParameter().getValue());

            if (button.getToggleState() != parameterState)
                button.setToggleState(parameterState, dontSendNotification);
        }

        void buttonClicked() {
            if (getParameterState(getParameter().getValue()) != button.getToggleState()) {
                getParameter().beginChangeGesture();
                getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
                getParameter().endChangeGesture();
            }
        }

        bool getParameterState(float value) const noexcept {
            return value >= 0.5f;
        }

        ToggleButton button;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BooleanParameterComponent)
    };

    class SwitchParameterComponent final : public Component,
                                           private ParameterListener {
    public:
        SwitchParameterComponent(AudioProcessor &processor, AudioProcessorParameter &param)
                : ParameterListener(processor, param) {
            auto *leftButton = buttons.add(new TextButton());
            auto *rightButton = buttons.add(new TextButton());

            for (auto *button : buttons) {
                button->setRadioGroupId(293847);
                button->setClickingTogglesState(true);
            }

            leftButton->setButtonText(getParameter().getText(0.0f, 16));
            rightButton->setButtonText(getParameter().getText(1.0f, 16));

            leftButton->setConnectedEdges(Button::ConnectedOnRight);
            rightButton->setConnectedEdges(Button::ConnectedOnLeft);

            // Set the initial value.
            leftButton->setToggleState(true, dontSendNotification);
            handleNewParameterValue();

            rightButton->onStateChange = [this]() { rightButtonChanged(); };

            for (auto *button : buttons)
                addAndMakeVisible(button);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds().reduced(0, 8);
            area.removeFromLeft(8);

            for (auto *button : buttons)
                button->setBounds(area.removeFromLeft(80));
        }

    private:
        void handleNewParameterValue() override {
            bool newState = getParameterState();

            if (buttons[1]->getToggleState() != newState) {
                buttons[1]->setToggleState(newState, dontSendNotification);
                buttons[0]->setToggleState(!newState, dontSendNotification);
            }
        }

        void rightButtonChanged() {
            auto buttonState = buttons[1]->getToggleState();

            if (getParameterState() != buttonState) {
                getParameter().beginChangeGesture();

                if (getParameter().getAllValueStrings().isEmpty()) {
                    getParameter().setValueNotifyingHost(buttonState ? 1.0f : 0.0f);
                } else {
                    // When a parameter provides a list of strings we must set its
                    // value using those strings, rather than a float, because VSTs can
                    // have uneven spacing between the different allowed values and we
                    // want the snapping behaviour to be consistent with what we do with
                    // a combo box.
                    String selectedText = buttonState ? buttons[1]->getButtonText() : buttons[0]->getButtonText();
                    getParameter().setValueNotifyingHost(getParameter().getValueForText(selectedText));
                }

                getParameter().endChangeGesture();
            }
        }

        bool getParameterState() {
            if (getParameter().getAllValueStrings().isEmpty())
                return getParameter().getValue() > 0.5f;

            auto index = getParameter().getAllValueStrings()
                    .indexOf(getParameter().getCurrentValueAsText());

            if (index < 0) {
                // The parameter is producing some unexpected text, so we'll do
                // some linear interpolation.
                index = roundToInt(getParameter().getValue());
            }

            return index == 1;
        }

        OwnedArray<TextButton> buttons;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchParameterComponent)
    };

    class ChoiceParameterComponent final : public Component,
                                           private ParameterListener {
    public:
        ChoiceParameterComponent(AudioProcessor &processor, AudioProcessorParameter &param)
                : ParameterListener(processor, param),
                  parameterValues(getParameter().getAllValueStrings()) {
            box.addItemList(parameterValues, 1);

            // Set the initial value.
            handleNewParameterValue();

            box.onChange = [this]() { boxChanged(); };
            addAndMakeVisible(box);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds();
            area.removeFromLeft(8);
            box.setBounds(area.reduced(0, 10));
        }

    private:
        void handleNewParameterValue() override {
            auto index = parameterValues.indexOf(getParameter().getCurrentValueAsText());

            if (index < 0) {
                // The parameter is producing some unexpected text, so we'll do
                // some linear interpolation.
                index = roundToInt(getParameter().getValue() * (parameterValues.size() - 1));
            }

            box.setSelectedItemIndex(index);
        }

        void boxChanged() {
            if (getParameter().getCurrentValueAsText() != box.getText()) {
                getParameter().beginChangeGesture();

                // When a parameter provides a list of strings we must set its
                // value using those strings, rather than a float, because VSTs can
                // have uneven spacing between the different allowed values.
                getParameter().setValueNotifyingHost(getParameter().getValueForText(box.getText()));

                getParameter().endChangeGesture();
            }
        }

        ComboBox box;
        const StringArray parameterValues;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceParameterComponent)
    };

    class SliderParameterComponent final : public Component,
                                           private ParameterListener {
    public:
        SliderParameterComponent(AudioProcessor &processor, AudioProcessorParameter &param)
                : ParameterListener(processor, param) {
            if (getParameter().getNumSteps() != AudioProcessor::getDefaultNumParameterSteps())
                slider.setRange(0.0, 1.0, 1.0 / (getParameter().getNumSteps() - 1.0));
            else
                slider.setRange(0.0, 1.0);

            slider.setScrollWheelEnabled(false);
            addAndMakeVisible(slider);

            valueLabel.setColour(Label::outlineColourId, slider.findColour(Slider::textBoxOutlineColourId));
            valueLabel.setBorderSize({1, 1, 1, 1});
            valueLabel.setJustificationType(Justification::centred);
            addAndMakeVisible(valueLabel);

            // Set the initial value.
            handleNewParameterValue();

            slider.onValueChange = [this]() { sliderValueChanged(); };
            slider.onDragStart = [this]() { sliderStartedDragging(); };
            slider.onDragEnd = [this]() { sliderStoppedDragging(); };
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds().reduced(0, 10);

            valueLabel.setBounds(area.removeFromRight(80));

            area.removeFromLeft(6);
            slider.setBounds(area);
        }

    private:
        void updateTextDisplay() {
            valueLabel.setText(getParameter().getCurrentValueAsText(), dontSendNotification);
        }

        void handleNewParameterValue() override {
            if (!isDragging) {
                slider.setValue(getParameter().getValue(), dontSendNotification);
                updateTextDisplay();
            }
        }

        void sliderValueChanged() {
            auto newVal = (float) slider.getValue();

            if (getParameter().getValue() != newVal) {
                if (!isDragging)
                    getParameter().beginChangeGesture();

                getParameter().setValueNotifyingHost((float) slider.getValue());
                updateTextDisplay();

                if (!isDragging)
                    getParameter().endChangeGesture();
            }
        }

        void sliderStartedDragging() {
            isDragging = true;
            getParameter().beginChangeGesture();
        }

        void sliderStoppedDragging() {
            isDragging = false;
            getParameter().endChangeGesture();
        }

        Slider slider{Slider::LinearHorizontal, Slider::TextEntryBoxPosition::NoTextBox};
        Label valueLabel;
        bool isDragging = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliderParameterComponent)
    };

    class ParameterDisplayComponent : public Component {
    public:
        ParameterDisplayComponent(AudioProcessor &processor, AudioProcessorParameter &param)
                : parameter(param) {
            parameterName.setText(parameter.getName(128), dontSendNotification);
            parameterName.setJustificationType(Justification::centredRight);
            addAndMakeVisible(parameterName);

            parameterLabel.setText(parameter.getLabel(), dontSendNotification);
            addAndMakeVisible(parameterLabel);

            if (param.isBoolean()) {
                // The AU, AUv3 and VST (only via a .vstxml file) SDKs support
                // marking a parameter as boolean. If you want consistency across
                // all  formats then it might be best to use a
                // SwitchParameterComponent instead.
                parameterComp = std::make_unique<BooleanParameterComponent>(processor, param);
            } else if (param.getNumSteps() == 2) {
                // Most hosts display any parameter with just two steps as a switch.
                parameterComp = std::make_unique<SwitchParameterComponent>(processor, param);
            } else if (!param.getAllValueStrings().isEmpty()) {
                // If we have a list of strings to represent the different states a
                // parameter can be in then we should present a dropdown allowing a
                // user to pick one of them.
                parameterComp = std::make_unique<ChoiceParameterComponent>(processor, param);
            } else {
                // Everything else can be represented as a slider.
                parameterComp = std::make_unique<SliderParameterComponent>(processor, param);
            }

            addAndMakeVisible(parameterComp.get());

            setSize(400, 40);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds();

            parameterName.setBounds(area.removeFromLeft(100));
            parameterLabel.setBounds(area.removeFromRight(50));
            parameterComp->setBounds(area);
        }

    private:
        AudioProcessorParameter &parameter;
        Label parameterName, parameterLabel;
        std::unique_ptr<Component> parameterComp;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterDisplayComponent)
    };

    class ParametersPanel : public Component {
    public:
        ParametersPanel(AudioProcessor &processor, const OwnedArray<AudioProcessorParameter> &parameters) {
            for (auto *param : parameters)
                if (param->isAutomatable())
                    addAndMakeVisible(paramComponents.add(new ParameterDisplayComponent(processor, *param)));

            if (auto *comp = paramComponents[0])
                setSize(comp->getWidth(), comp->getHeight() * paramComponents.size());
            else
                setSize(400, 100);
        }

        void paint(Graphics &g) override {
            g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
        }

        void resized() override {
            auto area = getLocalBounds();

            for (auto *comp : paramComponents)
                comp->setBounds(area.removeFromTop(comp->getHeight()));
        }

    private:
        OwnedArray<ParameterDisplayComponent> paramComponents;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametersPanel)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorEditor)
};
