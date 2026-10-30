//
//  frame.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 20/12/23.
//

#ifndef frame_h
#define frame_h

#include "ofxOceanodeTime.h"
#include <functional>

// The conversion functions a buffer uses to store a T1 sample inside a T2
// container and to read it back. One instance is owned by the buffer; frames
// only point at it, so a frame costs the container, a timestamp and a pointer.
template <typename T1, typename T2 = T1>
struct bufferFunctions {
    using Assign = std::function<void(T1&, T2&)>;
    using Return = std::function<T1(T2&)>;
    Assign assign;
    Return returnValue;
};

template <typename T1, typename T2 = T1>
class bufferFrame {
public:
    using BufferAssignFunction = typename bufferFunctions<T1, T2>::Assign;
    using BufferReturnFunction = typename bufferFunctions<T1, T2>::Return;

    bufferFrame(const bufferFunctions<T1, T2> *_functions, T1 &_object, Timestamp _timestamp)
    : functions(_functions), timestamp(_timestamp){
        functions->assign(_object, object);
    }

    // Reuses the existing container, so an allocated resource such as an fbo
    // is written in place instead of being recreated.
    void update(T1 &_object, Timestamp _timestamp){
        timestamp = _timestamp;
        functions->assign(_object, object);
    }

    T1 getObjectRef(){
        return functions->returnValue(object);
    }

    T2 &getContainer(){ return object; }
    const T2 &getContainer() const { return object; }

    Timestamp getTimestamp() const { return timestamp; }

private:
    const bufferFunctions<T1, T2> *functions;
    T2 object;
    Timestamp timestamp;
};

#endif /* frame_h */
