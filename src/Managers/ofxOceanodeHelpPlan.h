#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace ofxOceanodeHelpPlan {
// Validate before constructing any node. The assistant can only add a small,
// acyclic graph whose connections refer to nodes in this proposal.
inline void validate(const nlohmann::json& plan, const std::function<bool(const std::string&)>& registered) {
    auto require = [](bool ok, const std::string& why){ if(!ok) throw std::runtime_error(why); };
    require(plan.is_object(), "Expected an object containing answer, nodes and connections.");
    require(plan.contains("answer") && plan["answer"].is_string(), "Missing answer text.");
    require(plan.contains("nodes") && plan["nodes"].is_array(), "Missing nodes array.");
    require(plan.contains("connections") && plan["connections"].is_array(), "Missing connections array.");
    require(plan["nodes"].size() <= 24 && plan["connections"].size() <= 48, "Use at most 24 nodes and 48 connections.");
    std::set<std::string> ids, inputs;
    std::map<std::string, std::vector<std::string>> edges;
    for(const auto& node : plan["nodes"]) {
        require(node.is_object() && node.contains("id") && node["id"].is_string() &&
                node.contains("type") && node["type"].is_string(), "Each node needs a string id and type.");
        const auto id = node["id"].get<std::string>();
        const auto type = node["type"].get<std::string>();
        require(!id.empty() && id.size() <= 128 && ids.insert(id).second, "Node IDs must be nonempty and unique.");
        require(registered(type), "Unknown node type: " + type);
        require(!node.contains("values"), "Parameter values are not supported; create nodes and connections only.");
    }
    for(const auto& edge : plan["connections"]) {
        require(edge.is_object(), "Each connection must be an object.");
        for(const auto* key : {"from", "output", "to", "input"})
            require(edge.contains(key) && edge[key].is_string() && !edge[key].get<std::string>().empty(),
                    std::string("Connection needs ") + key + ".");
        const auto from = edge["from"].get<std::string>(), to = edge["to"].get<std::string>();
        require(ids.count(from) && ids.count(to), "Connections must reference proposed node IDs.");
        require(inputs.insert(to + "\n" + edge["input"].get<std::string>()).second, "An input can have only one connection.");
        edges[from].push_back(to);
    }
    std::set<std::string> visiting, visited;
    std::function<void(const std::string&)> visit = [&](const std::string& id){
        require(!visiting.count(id), "Feedback cycles are not supported by the assistant.");
        if(visited.count(id)) return;
        visiting.insert(id);
        for(const auto& to : edges[id]) visit(to);
        visiting.erase(id);
        visited.insert(id);
    };
    for(const auto& id : ids) visit(id);
}
}
