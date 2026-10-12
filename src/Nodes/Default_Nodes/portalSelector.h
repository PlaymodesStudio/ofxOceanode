#pragma once

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "portal.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Shared selection and preset logic. The typed subclasses only supply the
// output parameter and listen to the selected portal's Value parameter.
class portalSelectorBase : public ofxOceanodeNodeModel {
public:
    explicit portalSelectorBase(const std::string& typeLabel)
    : ofxOceanodeNodeModel("Portal Selector " + typeLabel) {
        description = "Selects a portal of this type and outputs its data. Global portals are marked with *.";
    }

    ~portalSelectorBase() override {
        selectedValueListener.unsubscribe();
        removedListener.unsubscribe();
    }

    void setup() override {
        addParameterDropdown(selectedPortalIndex, "Portal", 0, {"No Compatible Portals"});
        addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
        addInspectorParameter(selectedPortalScope.set("Selected Portal Scope", ""));
        addInspectorParameter(selectedPortalLocal.set("Selected Portal Local", true));
        addInspectorParameter(globalSearch.set("Global Search", false));
        setupOutput();

        dropdownListener = selectedPortalIndex.newListener([this](int& index) {
            if (!syncingSelection && !ofxOceanodeShared::isPresetLoading()) {
                selectIndex(index);
            }
        });
        searchListener = globalSearch.newListener([this](bool&) { dirty = true; });
        presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this]() {
            restoreSavedSelection = true;
            dirty = true;
            refresh();
        });
        removedListener = ofxOceanodeShared::getPortalWillBeRemovedEvent().newListener(
            [this](abstractPortal*& removed) {
                if (selectedPortal == removed) {
                    selectedValueListener.unsubscribe();
                    selectedPortal = nullptr;
                    resetOutput();
                }
                dirty = true;
            });
        refresh();
    }

    void update(ofEventArgs&) override {
        if (ofxOceanodeShared::isPresetLoading()) return;
        if (dirty || observedRevision != ofxOceanodeShared::getPortalListRevision() ||
            observedScope != getParents()) {
            refresh();
        }
    }

    void presetRecallAfterSettingParameters(ofJson& json) override {
        savedIdentityPresent = json.contains("Selected_Portal_Scope") &&
                               json.contains("Selected_Portal_Local");
        restoreSavedSelection = true;
        dirty = true;
    }

protected:
    ofEventListener selectedValueListener;

    virtual void setupOutput() = 0;
    virtual void resetOutput() = 0;
    virtual void bindValue(abstractPortal* selected) = 0;
    virtual std::vector<abstractPortal*> collectPortals() const = 0;
    virtual void updateNames(const std::vector<std::string>&) {}

private:
    void refresh() {
        if (ofxOceanodeShared::isPresetLoading()) return;

        const auto scope = getParents();
        std::vector<abstractPortal*> nextPortals;
        std::vector<std::string> nextNames;
        std::set<std::tuple<std::string, bool, std::string>> seen;

        for (auto* candidate : collectPortals()) {
            if (!candidate) continue;
            if (!globalSearch.get() && candidate->isLocal() && candidate->getParents() != scope) continue;

            // Portals with the same name, locality and scope share one channel.
            const auto key = std::make_tuple(candidate->getName(), candidate->isLocal(),
                                             candidate->isLocal() ? candidate->getParents() : "");
            if (!seen.insert(key).second) continue;

            std::string label = candidate->getName();
            if (globalSearch.get() && candidate->isLocal() && candidate->getParents() != scope) {
                label = candidate->getParents() + "/" + label;
            }
            if (!candidate->isLocal()) label += " *";
            nextPortals.push_back(candidate);
            nextNames.push_back(label);
        }

        auto* previous = selectedPortal;
        int chosen = -1;
        const auto chooseSaved = [&]() {
            for (std::size_t i = 0; i < nextPortals.size(); ++i) {
                auto* candidate = nextPortals[i];
                if (candidate->getName() == selectedPortalName.get() &&
                    candidate->isLocal() == selectedPortalLocal.get() &&
                    (!candidate->isLocal() || candidate->getParents() == selectedPortalScope.get())) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };
        const bool awaitingSavedPortal = restoreSavedSelection && !selectedPortalName.get().empty();
        if (awaitingSavedPortal) chosen = chooseSaved();
        if (chosen < 0 && previous && !awaitingSavedPortal) {
            for (std::size_t i = 0; i < nextPortals.size(); ++i) {
                if (nextPortals[i] == previous) { chosen = static_cast<int>(i); break; }
            }
        }
        if (chosen < 0 && !savedIdentityPresent && !selectedPortalName.get().empty()) {
            // Old presets saved only the name, without scope/locality.
            for (std::size_t i = 0; i < nextPortals.size(); ++i) {
                if (nextPortals[i]->getName() == selectedPortalName.get()) {
                    chosen = static_cast<int>(i);
                    break;
                }
            }
        }
        if (chosen < 0 && !nextPortals.empty() && !awaitingSavedPortal) chosen = 0;

        compatiblePortals = std::move(nextPortals);
        portalNames = std::move(nextNames);
        std::vector<std::string> dropdownNames = portalNames;
        if (dropdownNames.empty()) dropdownNames.push_back("No Compatible Portals");

        syncingSelection = true;
        getOceanodeParameter(selectedPortalIndex).setDropdownOptions(dropdownNames);
        selectedPortalIndex.setMin(0);
        selectedPortalIndex.setMax(static_cast<int>(dropdownNames.size()) - 1);
        selectedPortalIndex = std::max(0, chosen);
        syncingSelection = false;
        updateNames(dropdownNames);

        if (chosen >= 0) bindSelection(compatiblePortals[chosen]);
        else bindSelection(nullptr);

        observedRevision = ofxOceanodeShared::getPortalListRevision();
        observedScope = scope;
        restoreSavedSelection = awaitingSavedPortal && chosen < 0;
        dirty = false;
    }

    void selectIndex(int index) {
        if (index >= 0 && static_cast<std::size_t>(index) < compatiblePortals.size()) {
            bindSelection(compatiblePortals[index]);
        } else {
            bindSelection(nullptr);
        }
    }

    void bindSelection(abstractPortal* selected) {
        if (selectedPortal != selected) {
            selectedValueListener.unsubscribe();
            selectedPortal = selected;
            if (selected) bindValue(selected);
            else resetOutput();
        }
        if (selected) {
            selectedPortalName = selected->getName();
            selectedPortalScope = selected->isLocal() ? selected->getParents() : "";
            selectedPortalLocal = selected->isLocal();
        }
    }

    ofParameter<int> selectedPortalIndex;
    ofParameter<std::string> selectedPortalName;
    ofParameter<std::string> selectedPortalScope;
    ofParameter<bool> selectedPortalLocal;
    ofParameter<bool> globalSearch;
    ofEventListener dropdownListener;
    ofEventListener searchListener;
    ofEventListener presetLoadedListener;
    ofEventListener removedListener;
    std::vector<abstractPortal*> compatiblePortals;
    std::vector<std::string> portalNames;
    abstractPortal* selectedPortal = nullptr;
    std::uint64_t observedRevision = 0;
    std::string observedScope;
    bool syncingSelection = false;
    bool restoreSavedSelection = false;
    bool savedIdentityPresent = false;
    bool dirty = true;
};

template<typename T> struct portalSelectorNumericVector : std::false_type {};
template<typename T> struct portalSelectorNumericVector<std::vector<T>> : std::bool_constant<std::is_arithmetic<T>::value> {
    using element_type = T;
};

template<typename T>
class portalSelector : public portalSelectorBase {
public:
    portalSelector(const std::string& typeLabel, T defaultValue)
    : portalSelectorBase(typeLabel), defaultValue(std::move(defaultValue)) {}

protected:
    void setupOutput() override {
        if constexpr (std::is_arithmetic<T>::value) {
            addOutputParameter(output.set("Output", defaultValue,
                std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()));
        } else if constexpr (portalSelectorNumericVector<T>::value) {
            using Element = typename portalSelectorNumericVector<T>::element_type;
            addOutputParameter(output.set("Output", defaultValue,
                T{std::numeric_limits<Element>::lowest()},
                T{std::numeric_limits<Element>::max()}));
        } else {
            addOutputParameter(output.set("Output", defaultValue));
        }
        addOutputParameter(outputNames.set("Names", {}));
    }

    void resetOutput() override { output = defaultValue; }

    void bindValue(abstractPortal* selected) override {
        auto* typed = static_cast<portal<T>*>(selected);
        output = typed->getValue();
        selectedValueListener = typed->getValueParameter().newListener([this](T& value) {
            output = value;
        });
    }

    std::vector<abstractPortal*> collectPortals() const override {
        std::vector<abstractPortal*> result;
        for (auto* candidate : ofxOceanodeShared::getAllPortals<T>()) result.push_back(candidate);
        return result;
    }

    void updateNames(const std::vector<std::string>& names) override { outputNames = names; }

private:
    T defaultValue;
    ofParameter<T> output;
    ofParameter<std::vector<std::string>> outputNames;
};

template<>
class portalSelector<void> : public portalSelectorBase {
public:
    explicit portalSelector(const std::string& typeLabel) : portalSelectorBase(typeLabel) {}

protected:
    void setupOutput() override { addOutputParameter(output.set("Output")); }
    void resetOutput() override {}

    void bindValue(abstractPortal* selected) override {
        auto* typed = static_cast<portal<void>*>(selected);
        selectedValueListener = typed->getValueParameter().newListener([this]() {
            output.trigger();
        });
    }

    std::vector<abstractPortal*> collectPortals() const override {
        std::vector<abstractPortal*> result;
        for (auto* candidate : ofxOceanodeShared::getAllPortals<void>()) result.push_back(candidate);
        return result;
    }

private:
    ofParameter<void> output;
};
