//
//  ofxOceanodeShared.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 17/04/2020.
//

#ifndef ofxOceanodeShared_h
#define ofxOceanodeShared_h

class ofxOceanodeShared{
public:
    static unsigned int getDockspaceID(){
        return getInstance().getDockspaceIDFromInstance();
    }
    
    static unsigned int getCentralNodeID(){
        return getInstance().getCentralNodeIDFromInstance();
    }
    
    static unsigned int getLeftNodeID(){
        return getInstance().getLeftNodeIDFromInstance();
    }
    
    static void setDockspaceID(unsigned int _did){
        auto &instance = getInstance();
        if(instance.getDockspaceIDFromInstance() == 0){
            instance.setDockspaceIDFromInstance(_did);
        }
    }
    
    static void setCentralNodeID(unsigned int _cnid){
        auto &instance = getInstance();
        if(instance.getCentralNodeIDFromInstance() == 0){
            instance.setCentralNodeIDFromInstance(_cnid);
        }
    }
    
    static void setLeftNodeID(unsigned int _lnid){
        auto &instance = getInstance();
        if(instance.getLeftNodeIDFromInstance() == 0){
            instance.setLeftNodeIDFromInstance(_lnid);
        }
    }
    
private:
    ofxOceanodeShared(){};
    
    static ofxOceanodeShared &getInstance(){
        static ofxOceanodeShared instance;
        return instance;
    }
    
    unsigned int getDockspaceIDFromInstance(){
        return dockspace_id;
    }
    unsigned int getCentralNodeIDFromInstance(){
        return centralNode_id;
    }
    unsigned int getLeftNodeIDFromInstance(){
        return leftNode_id;
    }
    
    void setDockspaceIDFromInstance(unsigned int _did){dockspace_id = _did;};
    void setCentralNodeIDFromInstance(unsigned int _cnid){centralNode_id = _cnid;};
    void setLeftNodeIDFromInstance(unsigned int _lnid){leftNode_id = _lnid;};
    
    unsigned int dockspace_id = 0;
    unsigned int centralNode_id = 0;
    unsigned int leftNode_id = 0;
};

#endif /* ofxOceanodeShared_h */
