//
//  ofxOceanodeGlobalVariablesController.hpp
//  ofxOceanode
//
//  Created by Eduard Frigola on 18/12/23.
//

#ifndef ofxOceanodeGlobalVariablesController_h
#define ofxOceanodeGlobalVariablesController_h

#include "ofxOceanodeBaseController.h"

class globalVariables;

// Variable types, in the order the Type column lists them.
enum globalVariableType {
    globalVariableType_Float = 0,
    globalVariableType_Int,
    globalVariableType_Bool,
    globalVariableType_String,
    globalVariableType_Color,
    globalVariableType_FloatColor,
    globalVariableType_Count
};

const char* globalVariableTypeName(int type);
// -1 when the parameter is not one of the variable types.
int globalVariableTypeOf(const ofAbstractParameter& parameter);
// A new variable of the given type, holding value as converted from the
// source (numbers convert to each other, colors to each other, anything to
// and from text); otherwise the type's default.
std::shared_ptr<ofAbstractParameter> createGlobalVariable(int type, const std::string& name, const ofAbstractParameter* source = nullptr);

struct globalVariablesGroup : std::enable_shared_from_this<globalVariablesGroup> {
    globalVariablesGroup();
    globalVariablesGroup(string _name, shared_ptr<ofxOceanodeContainer> _container);

    ~globalVariablesGroup();

    void registerModule();
    void unregisterModule();

    void addNode(globalVariables* node);
    void removeNode(globalVariables* node);
    void addParameter(std::shared_ptr<ofAbstractParameter> parameter);
    void addFloatParameter(std::string parameterName, float value = 0);
    void addIntParameter(std::string parameterName, int value = 0);
    void addBoolParameter(std::string parameterName, bool value = false);
    void addStringParameter(std::string parameterName, std::string value = "");
    void addOfColorParameter(std::string parameterName, ofColor value = ofColor::black);
    void addOfFloatColorParameter(std::string parameterName, ofFloatColor value = ofFloatColor::black);
    void removeParameter(std::string parameterName);

    // Variables are matched by escaped name, as node parameters are, so
    // "My Var" and "My_Var" can't both exist.
    bool isParameterNameAvailable(const std::string& parameterName, const ofAbstractParameter* ignore = nullptr) const;
    bool renameParameter(const std::string& oldName, const std::string& newName);
    // Keeps the variable's place in every Globals node and the connections
    // the new type still accepts.
    bool setParameterType(const std::string& parameterName, int type);

    // Registers the group as "Globals <newName>" and replaces each node of
    // the old type, keeping its place, state and connections.
    bool rename(const std::string& newName);

    string name;
    std::vector<std::shared_ptr<ofAbstractParameter>> parameters;
    vector<globalVariables*> nodes;

    shared_ptr<ofxOceanodeContainer> container;
};

class ofxOceanodeGlobalVariablesController: public ofxOceanodeBaseController{
public:
    ofxOceanodeGlobalVariablesController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeGlobalVariablesController(){};

    void draw() override;
    void drawPopups() override;
    bool isMenuController() const override { return true; }

    void save();
    void load();

private:
    void drawGroupTable(const std::shared_ptr<globalVariablesGroup>& group);
    std::shared_ptr<globalVariablesGroup> findGroup(const std::string& groupName) const;
    bool isGroupNameAvailable(const std::string& groupName, const globalVariablesGroup* ignore = nullptr) const;

    shared_ptr<ofxOceanodeContainer> container;

    std::vector<std::shared_ptr<globalVariablesGroup>> groups;
    bool unsavedChanges = false;
    bool newGroupRequested = false;
    bool newVariableRequested = false;
    bool renameGroupRequested = false;
    std::string newVariableGroupName;
    std::string renameGroupName;
    char newGroupName[255] = {};
    char newVariableName[255] = {};
    char groupNameBuffer[255] = {};
    int newVariableType = 0;

    // Inline variable rename: the row being edited and its text.
    const ofAbstractParameter* renamingParameter = nullptr;
    char variableNameBuffer[255] = {};

    // Edits gathered while drawing, applied once the tables are done.
    struct PendingEdit {
        enum Kind { Rename, Retype, Remove } kind;
        std::string group;
        std::string parameter;
        std::string newName;
        int type = 0;
    };
    std::vector<PendingEdit> pendingEdits;
};

#endif /* ofxOceanodeGlobalVariablesController_h */
