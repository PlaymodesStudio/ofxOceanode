//
//  ofxOceanodeShared.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 17/04/2020.
//

#ifndef ofxOceanodeShared_h
#define ofxOceanodeShared_h

struct macroCategory{
	macroCategory(std::string _name = "Parent") : name(_name){};
	std::string name;
	std::vector<pair<string, string>> macros;
	std::vector<macroCategory> categories;
};

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
	
	static macroCategory getMacroDirectoryStructure(){
		return getInstance().macroDirectoryStructure;
	}
	
	static void readMacros(){
		ofDirectory dir;
		if(!dir.doesDirectoryExist("Macros")){
			dir.createDirectory("Macros");
		}
		getInstance().macroDirectoryStructure = macroCategory();
		readDirectory("Macros", getInstance().macroDirectoryStructure);
	}
	
	static void readDirectory(string dirName, macroCategory &category){
		ofDirectory dir;
		dir.open(dirName);
		dir.sort();
		for(auto &folder : dir.getFiles()){
			if(folder.isDirectory()){
				ofDirectory dir2(folder.path());
				dir2.sort();
				if(dir2.size() != 0 && !dir2.getFile(0).isDirectory()){
					category.macros.emplace_back(make_pair(folder.getFileName(), folder.path()));
				}else{
					category.categories.emplace_back(folder.getFileName());
					readDirectory(folder.path(), category.categories.back());
				}
				
			}
		}
	}
	
	static void updateMacrosStructure(){
		readMacros();
	}
	
	static void macroUpdated(string macroPath){
		getInstance().macroUpdatedEvent.notify(macroPath);
	}
	
	static ofEvent<string>& getMacroUpdatedEvent(){
		return getInstance().macroUpdatedEvent;
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
	
	macroCategory macroDirectoryStructure;
	
	ofEvent<string> macroUpdatedEvent;
};

#endif /* ofxOceanodeShared_h */
