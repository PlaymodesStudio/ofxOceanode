//
//  ofxOceanodeContainerInternal.h
//  ofxOceanode
//
//  Declarations shared only between the ofxOceanodeContainer*.cpp translation
//  units. Nothing else should include this header.
//

#pragma once

#include <string>
#include <vector>

namespace ofxOceanodeContainerInternal {

struct ConnectionFailure {
    std::string connection;
    std::string reason;
};

// Queues the modal that ofxOceanodeContainer::drawPendingConnectionErrorDialog
// shows for the connections a preset load could not create.
void queueConnectionErrorDialog(std::string canvasIdPath, std::string canvasNamePath,
                                std::vector<ConnectionFailure> failedConnections);

}
