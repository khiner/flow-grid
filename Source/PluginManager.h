#pragma once

#include <view/PluginWindow.h>
#include "JuceHeader.h"
#include "processors/InternalPluginFormat.h"
#include "state/Identifiers.h"
#include "ApplicationPropertiesAndCommandManager.h"

class PluginManager : private ChangeListener {
public:
    PluginManager() {
        std::unique_ptr<XmlElement> savedPluginList(getUserSettings()->getXmlValue(PLUGIN_LIST_FILE_NAME));

        if (savedPluginList != nullptr) {
            knownPluginListExternal.recreateFromXml(*savedPluginList);
        }

        for (auto &pluginType : internalPluginDescriptions) {
            knownPluginListInternal.addType(pluginType);
            if (!InternalPluginFormat::isIoProcessorName(pluginType.name))
                userCreatablePluginListInternal.addType(pluginType);
        }

        pluginSortMethod = (KnownPluginList::SortMethod) getUserSettings()->getIntValue("pluginSortMethod", KnownPluginList::sortByCategory);
        knownPluginListExternal.addChangeListener(this);

        formatManager.addDefaultFormats();
        formatManager.addFormat(new InternalPluginFormat());
    }

    PluginListComponent *makePluginListComponent() {
        const File &deadMansPedalFile = getUserSettings()->getFile().getSiblingFile("RecentlyCrashedPluginsList");
        return new PluginListComponent(formatManager, knownPluginListExternal, deadMansPedalFile, getUserSettings(), true);
    }

    std::unique_ptr<PluginDescription> getDescriptionForIdentifier(const String &identifier) {
        auto description = knownPluginListInternal.getTypeForIdentifierString(identifier);
        return description != nullptr ? std::move(description) : knownPluginListExternal.getTypeForIdentifierString(identifier);
    }

    PluginDescription &getAudioInputDescription() {
        return internalFormat.audioInDesc;
    }

    PluginDescription &getAudioOutputDescription() {
        return internalFormat.audioOutDesc;
    }

    KnownPluginList &getKnownPluginListExternal() {
        return knownPluginListExternal;
    }

    KnownPluginList &getUserCreatablePluginListInternal() {
        return userCreatablePluginListInternal;
    }

    KnownPluginList::SortMethod getPluginSortMethod() const {
        return pluginSortMethod;
    }

    AudioPluginFormatManager &getFormatManager() {
        return formatManager;
    }

    void setPluginSortMethod(const KnownPluginList::SortMethod pluginSortMethod) {
        this->pluginSortMethod = pluginSortMethod;
    }

    void addPluginsToMenu(PopupMenu &menu, const ValueTree &track) const {
        StringArray disabledPluginIds;

        PopupMenu internalSubMenu;
        PopupMenu externalSubMenu;

        userCreatablePluginListInternal.addToMenu(internalSubMenu, getPluginSortMethod(), disabledPluginIds);
        knownPluginListExternal.addToMenu(externalSubMenu, getPluginSortMethod(), {}, String(), userCreatablePluginListInternal.getNumTypes());

        menu.addSubMenu("Internal", internalSubMenu, true);
        menu.addSeparator();
        menu.addSubMenu("External", externalSubMenu, true);
    }

    const PluginDescription *getChosenType(const int menuId) const {
        // TODO use `getTypes()[i]` instead (`getType` is deprecated)
        int internalPluginListIndex = userCreatablePluginListInternal.getIndexChosenByMenu(menuId);
        if (internalPluginListIndex != -1)
            return userCreatablePluginListInternal.getType(internalPluginListIndex);
        int externalPluginListIndex = knownPluginListExternal.getIndexChosenByMenu(menuId - userCreatablePluginListInternal.getNumTypes());
        if (externalPluginListIndex != -1)
            return knownPluginListExternal.getType(externalPluginListIndex);
        return nullptr;
    }

    static bool isGeneratorOrInstrument(const PluginDescription *description) {
        return description->isInstrument || description->category.equalsIgnoreCase("generator") || description->category.equalsIgnoreCase("synth");
    }

    bool doesTrackAlreadyHaveGeneratorOrInstrument(const ValueTree &track) {
        for (const auto &child : track)
            if (auto existingDescription = getDescriptionForIdentifier(child.getProperty(IDs::id)))
                if (isGeneratorOrInstrument(existingDescription.get()))
                    return true;
        return false;
    }

    ResizableWindow *getOrCreateWindowFor(AudioProcessorGraph::Node *node, PluginWindow::Type type) {
        jassert(node != nullptr);

        for (auto *w : activePluginWindows)
            if (w->node->nodeID == node->nodeID && w->type == type)
                return w;

        if (auto *processor = node->getProcessor())
            return activePluginWindows.add(new PluginWindow(node, type, activePluginWindows));

        return nullptr;
    }

    void closeWindowFor(AudioProcessorGraph::NodeID nodeID) {
        for (int i = activePluginWindows.size(); --i >= 0;)
            if (activePluginWindows.getUnchecked(i)->node->nodeID == nodeID)
                activePluginWindows.remove(i);
    }

    bool closeAnyOpenPluginWindows() {
        bool wasEmpty = activePluginWindows.isEmpty();
        activePluginWindows.clear();
        return !wasEmpty;
    }

private:
    const String PLUGIN_LIST_FILE_NAME = "pluginList";

    InternalPluginFormat internalFormat;
    KnownPluginList knownPluginListExternal;
    KnownPluginList knownPluginListInternal;
    KnownPluginList userCreatablePluginListInternal;

    KnownPluginList::SortMethod pluginSortMethod;
    AudioPluginFormatManager formatManager;

    OwnedArray<PluginWindow> activePluginWindows;

    void changeListenerCallback(ChangeBroadcaster *changed) override {
        if (changed == &knownPluginListExternal) {
            // save the plugin list every time it gets changed, so that if we're scanning
            // and it crashes, we've still saved the previous ones
            std::unique_ptr<XmlElement> savedPluginList(knownPluginListExternal.createXml());

            if (savedPluginList != nullptr) {
                getUserSettings()->setValue(PLUGIN_LIST_FILE_NAME, savedPluginList.get());
                getApplicationProperties().saveIfNeeded();
            }
        }
    }
};
