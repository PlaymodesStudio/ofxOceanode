//
//  frame.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef frame_h
#define frame_h

#include "ofxOceanodeTime.h"

template <typename T1, typename T2 = T1>
class bufferFrame {
public:
    using BufferAssignFunction = std::function<void(T1&, T2&)>;
    using BufferReturnFunction = std::function<T1(T2&)>;
    
    bufferFrame(T1 _object, BufferAssignFunction _assignFunction, BufferReturnFunction _returnFunction) : assignFunction(_assignFunction), returnFunction(_returnFunction){
        timestamp.update();
        assignFunction(_object, object);
    }
    
    bufferFrame(T1 _object, Timestamp _timestamp, BufferAssignFunction _assignFunction, BufferReturnFunction _returnFunction) : assignFunction(_assignFunction), returnFunction(_returnFunction){
        timestamp = _timestamp;
        assignFunction(_object, object);
    }
    
    void update(T1 _object){
        timestamp.update();
        assignFunction(_object, object);
    }
    
    void update(T1 _object, Timestamp _timestamp){
        timestamp = _timestamp;
        assignFunction(_object, object);
    }

    T1 getObjectRef(){
        return returnFunction(object);
    }
    
    Timestamp getTimestamp(){return timestamp;};
    
private:
    BufferAssignFunction assignFunction;
    BufferReturnFunction returnFunction;
    
    T2 object;
    Timestamp timestamp;
};

#endif /* frame_h */
