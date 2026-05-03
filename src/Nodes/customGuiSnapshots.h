#ifndef customGuiSnapshots_h
#define customGuiSnapshots_h

#include "ofxOceanodeNodeModel.h"

#ifndef OFXOCEANODE_HEADLESS
class ofxOceanodeContainer;
class ofxOceanodeNodeMacro;

class customGuiSnapshots : public ofxOceanodeNodeModel {
public:
    customGuiSnapshots();

    void setup() override;
    void update(ofEventArgs& a) override;
    void draw(ofEventArgs& a) override;
    void presetSave(ofJson& json) override;
    void presetRecallBeforeSettingParameters(ofJson& json) override;
    void setContainer(ofxOceanodeContainer* container) override;

private:
    struct TargetRef {
        std::string canvasId;
        std::string panelId;
        std::string label;
        std::string panelName;
    };

    void refreshTargets();
    void refreshSnapshotOptions();
    void syncSelectionFromStoredIds();
    void renderMatrix(bool withControls);
    void storeSlot(int slot);
    void recallSlot(int slot);
    ofxOceanodeContainer* findContainerForCanvasId(ofxOceanodeContainer* root, const std::string& canvasId) const;
    void collectTargets(ofxOceanodeContainer* root, std::vector<TargetRef>& out) const;
    ofxOceanodeContainer* getSelectedTargetContainer() const;
    const TargetRef* getSelectedTarget() const;
    int getCurrentSnapshotSlot() const;

    ofxOceanodeContainer* ownerContainer = nullptr;
    std::vector<TargetRef> availableTargets;

    ofParameter<int> customGuiIndex;
    ofParameter<int> snapshotSlot;
    ofParameter<bool> showWindow;
    ofParameter<int> matrixRows;
    ofParameter<int> matrixCols;
    ofParameter<bool> showSnapshotNames;
    customGuiRegion snapshotMatrixRegion;

    std::shared_ptr<ofxOceanodeAbstractParameter> customGuiIndexParam;
    std::shared_ptr<ofxOceanodeAbstractParameter> snapshotSlotParam;
    std::shared_ptr<ofxOceanodeAbstractParameter> snapshotMatrixRegionParam;

    std::string selectedCanvasId;
    std::string selectedPanelId;
    bool suppressTargetListener = false;
    bool suppressSnapshotListener = false;
    ofEventListeners listeners;
};
#endif

#endif /* customGuiSnapshots_h */
