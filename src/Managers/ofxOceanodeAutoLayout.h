//
//  ofxOceanodeAutoLayout.h
//  Layered (Sugiyama-style) auto layout for the node canvas.
//
//  Pure geometry: it knows nothing about nodes, parameters or ImGui, so the
//  canvas builds the input (sizes, pin heights, wires, portal links) and
//  applies the returned positions.
//
//  Pipeline, per connected component (real wires only):
//    1. Break cycles (reverse back edges, keeping the user's left-to-right order).
//    2. Rank: longest path, then a median refinement that moves every node to the
//       rank minimizing its total wire length (sources hug their consumers, sinks
//       hug their producers, in-between nodes balance).
//    3. Split wires spanning several columns with lane (dummy) nodes; wires
//       leaving the same output pin share one lane.
//    4. Order nodes inside each column by pin-aware barycenter sweeps plus
//       adjacent swaps, keeping the ordering with the fewest crossings.
//    5. Assign Y so connected pins line up (straight wires) using weighted
//       isotonic regression, which keeps column order and spacing exact, so
//       nodes never overlap.
//  Components are then packed in shelves. Portal-linked components (a sender's
//  subgraph and its receivers' subgraphs) are placed next to each other, but a
//  portal never merges subgraphs into shared columns.
//

#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>
#include <functional>
#include <glm/glm.hpp>

namespace ofxOceanodeAutoLayout {

struct Edge{
    int source;
    int sink;
    float sourcePinY; // Output pin height, relative to the source node's top edge
    float sinkPinY;   // Input pin height, relative to the sink node's top edge
    int sourcePin;    // Output pin id; wires leaving the same pin share their lane
};

struct Settings{
    float horizontalGap = 40;
    float verticalGap = 20;
    float laneGap = 10;      // Room for a long wire crossing a column
    float componentGap = 60;
    float grid = 0;          // > 0 snaps the result to this grid without creating overlaps
};

namespace detail{

struct ComponentLayout{
    std::vector<int> nodes;
    std::vector<glm::vec2> positions; // Relative to the component's top-left
    glm::vec2 size{0, 0};
};

inline float snapUp(float value, float grid){
    return grid > 0 ? std::ceil(value / grid - 1e-4f) * grid : value;
}
inline float snapNearest(float value, float grid){
    return grid > 0 ? std::round(value / grid) * grid : value;
}

// Weighted isotonic regression (pool adjacent violators): minimizes
// sum w_i (y_i - d_i)^2 subject to y_{i+1} >= y_i + minDistance_i.
inline std::vector<float> placeInOrder(const std::vector<float>& desired,
                                       const std::vector<float>& weights,
                                       const std::vector<float>& minDistance){
    const size_t n = desired.size();
    std::vector<float> offset(n, 0);
    for(size_t i = 1; i < n; i++) offset[i] = offset[i - 1] + minDistance[i - 1];
    struct Block{ double weight, weightedSum; size_t count; };
    std::vector<Block> blocks;
    for(size_t i = 0; i < n; i++){
        blocks.push_back({weights[i], weights[i] * (desired[i] - offset[i]), 1});
        while(blocks.size() > 1){
            Block& last = blocks[blocks.size() - 1];
            Block& previous = blocks[blocks.size() - 2];
            if(previous.weightedSum / previous.weight <= last.weightedSum / last.weight) break;
            previous.weight += last.weight;
            previous.weightedSum += last.weightedSum;
            previous.count += last.count;
            blocks.pop_back();
        }
    }
    std::vector<float> result(n);
    size_t i = 0;
    for(const auto& block : blocks){
        const float value = (float)(block.weightedSum / block.weight);
        for(size_t k = 0; k < block.count; k++, i++) result[i] = value + offset[i];
    }
    return result;
}

inline ComponentLayout layoutComponent(const std::vector<int>& componentNodes,
                                       const std::vector<glm::vec2>& sizes,
                                       const std::vector<glm::vec2>& original,
                                       const std::vector<Edge>& allEdges,
                                       const std::vector<int>& componentOf,
                                       int componentId,
                                       const Settings& settings){
    ComponentLayout result;
    result.nodes = componentNodes;
    const int m = (int)componentNodes.size();
    std::map<int, int> local;
    for(int i = 0; i < m; i++) local[componentNodes[i]] = i;

    std::vector<Edge> edges;
    for(const auto& edge : allEdges){
        if(edge.source == edge.sink || componentOf[edge.source] != componentId) continue;
        edges.push_back({local[edge.source], local[edge.sink], edge.sourcePinY, edge.sinkPinY, edge.sourcePin});
    }

    auto originalLess = [&](int a, int b){
        const glm::vec2 pa = original[componentNodes[a]];
        const glm::vec2 pb = original[componentNodes[b]];
        return pa.x != pb.x ? pa.x < pb.x : (pa.y != pb.y ? pa.y < pb.y : a < b);
    };

    // 1. Cycle breaking: a topological order that falls back to the node with the
    // fewest unresolved inputs (leftmost on ties) when only cycles remain.
    std::vector<std::vector<int>> outs(m), ins(m);
    for(const auto& edge : edges){
        outs[edge.source].push_back(edge.sink);
        ins[edge.sink].push_back(edge.source);
    }
    std::vector<int> order;
    std::vector<int> orderIndex(m, -1);
    {
        std::vector<int> remainingIn(m);
        for(int i = 0; i < m; i++) remainingIn[i] = (int)ins[i].size();
        for(int step = 0; step < m; step++){
            int best = -1;
            for(int i = 0; i < m; i++){
                if(orderIndex[i] != -1) continue;
                if(best == -1 || remainingIn[i] < remainingIn[best] ||
                   (remainingIn[i] == remainingIn[best] && originalLess(i, best))) best = i;
            }
            orderIndex[best] = (int)order.size();
            order.push_back(best);
            for(int sink : outs[best]) remainingIn[sink]--;
        }
    }

    // Acyclic weighted neighbour lists (duplicate wires between two nodes add weight)
    std::vector<std::map<int, int>> successors(m), predecessors(m);
    for(const auto& edge : edges){
        int a = edge.source, b = edge.sink;
        if(orderIndex[a] > orderIndex[b]) std::swap(a, b);
        successors[a][b]++;
        predecessors[b][a]++;
    }

    // 2. Ranking
    // Start "as late as possible": every node right next to its earliest consumer,
    // so whole branches (modulators included) hug the node they feed. The refinement
    // below then pulls sinks and anything else with a shorter option back left.
    std::vector<int> rank(m, 0);
    for(auto it = order.rbegin(); it != order.rend(); ++it){
        const int node = *it;
        for(const auto& successor : successors[node]) rank[node] = std::min(rank[node], rank[successor.first] - 1);
    }
    for(int iteration = 0; iteration < 32; iteration++){
        bool changed = false;
        auto relax = [&](int node){
            int low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
            std::vector<std::pair<int, int>> targets;
            for(const auto& p : predecessors[node]){
                low = std::max(low, rank[p.first] + 1);
                targets.push_back({rank[p.first] + 1, p.second});
            }
            for(const auto& s : successors[node]){
                high = std::min(high, rank[s.first] - 1);
                targets.push_back({rank[s.first] - 1, s.second});
            }
            if(targets.empty()) return;
            std::sort(targets.begin(), targets.end());
            int totalWeight = 0;
            for(const auto& t : targets) totalWeight += t.second;
            // Weighted median interval [medianLow, medianHigh]; staying inside it keeps
            // the node's wire length minimal without oscillating between equal choices.
            int accumulated = 0, medianLow = targets.front().first, medianHigh = targets.back().first;
            for(const auto& t : targets){
                accumulated += t.second;
                if(accumulated * 2 >= totalWeight){ medianLow = t.first; break; }
            }
            accumulated = 0;
            for(auto it = targets.rbegin(); it != targets.rend(); ++it){
                accumulated += it->second;
                if(accumulated * 2 >= totalWeight){ medianHigh = it->first; break; }
            }
            // On ties take the rank closest to the consumers: a chain feeding a node
            // further right (e.g. a mixer input) slides right as a whole instead of
            // staying left-aligned and ending in one long wire. medianLow is kept for
            // reference only; sinks always resolve to their producers' side anyway.
            (void)medianLow;
            const int newRank = std::min(std::max(medianHigh, low), high);
            if(newRank != rank[node]){ rank[node] = newRank; changed = true; }
        };
        for(auto it = order.rbegin(); it != order.rend(); ++it) relax(*it);
        for(int node : order) relax(node);
        if(!changed) break;
    }
    // Drop columns left empty by the refinement
    {
        std::vector<int> used(rank.begin(), rank.end());
        std::sort(used.begin(), used.end());
        used.erase(std::unique(used.begin(), used.end()), used.end());
        for(int& r : rank) r = (int)(std::lower_bound(used.begin(), used.end(), r) - used.begin());
    }
    int layerCount = 0;
    for(int r : rank) layerCount = std::max(layerCount, r + 1);

    // 3. Layered graph with lane nodes. Indices [0, m) are real nodes.
    struct LayerEdge{ int from, to; float fromY, toY; float weight; };
    std::vector<int> layerOf(rank.begin(), rank.end());
    std::vector<float> height(m), width(m);
    std::vector<float> sortKey(m);
    for(int i = 0; i < m; i++){
        height[i] = sizes[componentNodes[i]].y;
        width[i] = sizes[componentNodes[i]].x;
        sortKey[i] = original[componentNodes[i]].y;
    }
    std::vector<LayerEdge> layerEdges;
    {
        // Group wires by the output pin they leave from (in DAG direction)
        std::map<std::pair<int, int>, std::vector<std::pair<int, float>>> pinGroups; // (from, pin) -> [(to, toY)]
        std::map<std::pair<int, int>, float> pinHeight;
        int reversedPinId = -1;
        for(const auto& edge : edges){
            int from = edge.source, to = edge.sink;
            float fromY = edge.sourcePinY, toY = edge.sinkPinY;
            int pin = edge.sourcePin;
            if(orderIndex[from] > orderIndex[to]){
                std::swap(from, to);
                std::swap(fromY, toY);
                pin = reversedPinId--;
            }
            pinGroups[{from, pin}].push_back({to, toY});
            pinHeight[{from, pin}] = fromY;
        }
        for(auto& group : pinGroups){
            const int from = group.first.first;
            auto& sinks = group.second;
            std::sort(sinks.begin(), sinks.end(), [&](const std::pair<int, float>& a, const std::pair<int, float>& b){
                return layerOf[a.first] < layerOf[b.first];
            });
            int chainEnd = from;
            float chainEndY = pinHeight[group.first];
            for(const auto& sink : sinks){
                while(layerOf[chainEnd] < layerOf[sink.first] - 1){
                    const int lane = (int)layerOf.size();
                    layerOf.push_back(layerOf[chainEnd] + 1);
                    height.push_back(0);
                    width.push_back(0);
                    sortKey.push_back(sortKey[from] + chainEndY);
                    layerEdges.push_back({chainEnd, lane, chainEndY, 0, 1});
                    chainEnd = lane;
                    chainEndY = 0;
                }
                layerEdges.push_back({chainEnd, sink.first, chainEndY, sink.second, 1});
            }
        }
    }
    const int total = (int)layerOf.size();
    auto isLane = [&](int v){ return v >= m; };
    std::vector<std::vector<int>> inEdges(total), outEdges(total);
    for(int e = 0; e < (int)layerEdges.size(); e++){
        outEdges[layerEdges[e].from].push_back(e);
        inEdges[layerEdges[e].to].push_back(e);
    }
    for(auto& e : layerEdges){
        if(isLane(e.from) || isLane(e.to)) e.weight = 2; // Prefer straight long wires
    }

    // 4. Ordering
    std::vector<std::vector<int>> layers(layerCount);
    for(int v = 0; v < total; v++) layers[layerOf[v]].push_back(v);
    for(auto& layer : layers){
        std::stable_sort(layer.begin(), layer.end(), [&](int a, int b){ return sortKey[a] < sortKey[b]; });
    }
    std::vector<float> slot(total, 0);
    auto updateSlots = [&](int l){ for(int i = 0; i < (int)layers[l].size(); i++) slot[layers[l][i]] = (float)i; };
    for(int l = 0; l < layerCount; l++) updateSlots(l);
    // Pin-aware position: the slot plus where on the node the pin sits, so
    // wires from the same node to different pins can still be told apart.
    auto pinSlot = [&](int v, float pinY){
        if(isLane(v) || height[v] <= 0) return slot[v] + 0.5f;
        return slot[v] + 0.1f + 0.8f * std::min(std::max(pinY / height[v], 0.0f), 1.0f);
    };
    // Crossings between column l and l+1
    auto crossings = [&](int l){
        std::vector<std::pair<float, float>> segments;
        std::vector<float> segmentWeights;
        for(int v : layers[l]){
            for(int e : outEdges[v]){
                segments.push_back({pinSlot(v, layerEdges[e].fromY), pinSlot(layerEdges[e].to, layerEdges[e].toY)});
                segmentWeights.push_back(layerEdges[e].weight);
            }
        }
        double count = 0;
        for(size_t a = 0; a < segments.size(); a++){
            for(size_t b = a + 1; b < segments.size(); b++){
                const float da = segments[a].first - segments[b].first;
                const float db = segments[a].second - segments[b].second;
                if(da * db < 0) count += segmentWeights[a] * segmentWeights[b];
            }
        }
        return count;
    };
    auto totalCrossings = [&](){
        double count = 0;
        for(int l = 0; l + 1 < layerCount; l++) count += crossings(l);
        return count;
    };
    auto sortByBarycenter = [&](int l, bool usePredecessors){
        std::vector<float> key(layers[l].size());
        for(size_t i = 0; i < layers[l].size(); i++){
            const int v = layers[l][i];
            double sum = 0, weight = 0;
            for(int e : (usePredecessors ? inEdges[v] : outEdges[v])){
                const auto& edge = layerEdges[e];
                sum += edge.weight * (usePredecessors ? pinSlot(edge.from, edge.fromY) : pinSlot(edge.to, edge.toY));
                weight += edge.weight;
            }
            key[i] = weight > 0 ? (float)(sum / weight) : slot[v] + 0.5f;
        }
        std::vector<int> permutation(layers[l].size());
        std::iota(permutation.begin(), permutation.end(), 0);
        std::stable_sort(permutation.begin(), permutation.end(), [&](int a, int b){ return key[a] < key[b]; });
        std::vector<int> sorted;
        for(int p : permutation) sorted.push_back(layers[l][p]);
        layers[l] = sorted;
        updateSlots(l);
    };
    auto localCrossings = [&](int l){
        return (l > 0 ? crossings(l - 1) : 0) + (l + 1 < layerCount ? crossings(l) : 0);
    };
    auto transpose = [&](){
        for(int pass = 0; pass < 4; pass++){
            bool improved = false;
            for(int l = 0; l < layerCount; l++){
                for(int i = 0; i + 1 < (int)layers[l].size(); i++){
                    const double before = localCrossings(l);
                    std::swap(layers[l][i], layers[l][i + 1]);
                    updateSlots(l);
                    if(localCrossings(l) < before){
                        improved = true;
                    }else{
                        std::swap(layers[l][i], layers[l][i + 1]);
                        updateSlots(l);
                    }
                }
            }
            if(!improved) break;
        }
    };
    // Transpose is quadratic in wires per column pair; skip it on huge graphs
    const bool useTranspose = layerEdges.size() < 600;
    auto bestLayers = layers;
    double bestCrossings = totalCrossings();
    for(int iteration = 0; iteration < 12 && bestCrossings > 0; iteration++){
        if(iteration % 2 == 0){
            for(int l = 1; l < layerCount; l++) sortByBarycenter(l, true);
        }else{
            for(int l = layerCount - 2; l >= 0; l--) sortByBarycenter(l, false);
        }
        if(useTranspose) transpose();
        const double current = totalCrossings();
        if(current < bestCrossings){
            bestCrossings = current;
            bestLayers = layers;
        }
    }
    layers = bestLayers;
    for(int l = 0; l < layerCount; l++) updateSlots(l);

    // 5. Vertical placement
    std::vector<float> y(total, 0);
    auto distanceBetween = [&](int a, int b){
        const bool laneA = isLane(a), laneB = isLane(b);
        const float gap = (laneA && laneB) ? settings.laneGap : (laneA || laneB) ? settings.verticalGap * 0.5f + settings.laneGap * 0.5f : settings.verticalGap;
        return height[a] + gap;
    };
    for(int l = 0; l < layerCount; l++){
        float cursor = 0;
        for(size_t i = 0; i < layers[l].size(); i++){
            y[layers[l][i]] = cursor;
            if(i + 1 < layers[l].size()) cursor += distanceBetween(layers[l][i], layers[l][i + 1]);
        }
    }
    auto placeLayer = [&](int l, bool usePredecessors, bool useSuccessors){
        const auto& layer = layers[l];
        if(layer.empty()) return;
        std::vector<float> desired(layer.size()), weights(layer.size()), minDistance(layer.size(), 0);
        for(size_t i = 0; i < layer.size(); i++){
            const int v = layer[i];
            double sum = 0, weight = 0;
            if(usePredecessors){
                for(int e : inEdges[v]){
                    const auto& edge = layerEdges[e];
                    sum += edge.weight * (y[edge.from] + edge.fromY - edge.toY);
                    weight += edge.weight;
                }
            }
            if(useSuccessors){
                for(int e : outEdges[v]){
                    const auto& edge = layerEdges[e];
                    sum += edge.weight * (y[edge.to] + edge.toY - edge.fromY);
                    weight += edge.weight;
                }
            }
            if(weight > 0){
                desired[i] = (float)(sum / weight);
                weights[i] = (float)weight;
            }else{
                desired[i] = y[v];
                weights[i] = 0.01f; // Unconnected on this side: just follow neighbours
            }
            if(i + 1 < layer.size()) minDistance[i] = distanceBetween(v, layer[i + 1]);
        }
        const auto placed = placeInOrder(desired, weights, minDistance);
        for(size_t i = 0; i < layer.size(); i++) y[layer[i]] = placed[i];
    };
    for(int iteration = 0; iteration < 8; iteration++){
        for(int l = 1; l < layerCount; l++) placeLayer(l, true, false);
        for(int l = layerCount - 2; l >= 0; l--) placeLayer(l, false, true);
    }
    for(int iteration = 0; iteration < 4; iteration++){
        for(int l = 0; l < layerCount; l++) placeLayer(l, true, true);
        for(int l = layerCount - 1; l >= 0; l--) placeLayer(l, true, true);
    }

    // Columns and final (optionally snapped) coordinates, real nodes only
    std::vector<float> columnWidth(layerCount, 0), columnX(layerCount, 0);
    for(int v = 0; v < m; v++) columnWidth[layerOf[v]] = std::max(columnWidth[layerOf[v]], width[v]);
    float x = 0;
    for(int l = 0; l < layerCount; l++){
        columnX[l] = x;
        x = detail::snapUp(x + columnWidth[l] + settings.horizontalGap, settings.grid);
    }
    float minY = std::numeric_limits<float>::max();
    for(int v = 0; v < m; v++) minY = std::min(minY, y[v]);
    result.positions.assign(m, glm::vec2(0));
    for(int l = 0; l < layerCount; l++){
        float previousBottom = -std::numeric_limits<float>::max();
        for(int v : layers[l]){
            if(isLane(v)) continue;
            float top = detail::snapNearest(y[v] - minY, settings.grid);
            if(top < previousBottom + settings.verticalGap){
                top = detail::snapUp(previousBottom + settings.verticalGap, settings.grid);
            }
            result.positions[v] = glm::vec2(columnX[l], top);
            previousBottom = top + height[v];
            result.size = glm::max(result.size, result.positions[v] + glm::vec2(width[v], height[v]));
        }
    }
    return result;
}

} // namespace detail

// sizes / originalPositions: one entry per node.
// edges: real wires between those nodes.
// groupingLinks: pairs that should be laid out near each other without a wire
// (matching portal sender/receiver).
// Returns new top-left positions; the layout's top-left stays at the original one.
inline std::vector<glm::vec2> layout(const std::vector<glm::vec2>& sizes,
                                     const std::vector<glm::vec2>& originalPositions,
                                     const std::vector<Edge>& edges,
                                     const std::vector<std::pair<int, int>>& groupingLinks,
                                     const Settings& settings){
    const int n = (int)sizes.size();
    std::vector<glm::vec2> positions(originalPositions);
    if(n == 0) return positions;

    // Components over real wires
    std::vector<int> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> find = [&](int a){ return parent[a] == a ? a : parent[a] = find(parent[a]); };
    std::vector<int> degree(n, 0);
    for(const auto& edge : edges){
        if(edge.source == edge.sink) continue;
        parent[find(edge.source)] = find(edge.sink);
        degree[edge.source]++;
        degree[edge.sink]++;
    }
    std::vector<int> componentOf(n, -1);
    std::vector<std::vector<int>> components;
    for(int i = 0; i < n; i++){
        const int root = find(i);
        if(componentOf[root] == -1){
            componentOf[root] = (int)components.size();
            components.push_back({});
        }
        componentOf[i] = componentOf[root];
        components[componentOf[i]].push_back(i);
    }
    const int componentCount = (int)components.size();

    // Order components: reading order of their original top-left, with every
    // portal-linked component pulled in right after the one it is linked to.
    std::vector<glm::vec2> componentOrigin(componentCount, glm::vec2(std::numeric_limits<float>::max()));
    for(int i = 0; i < n; i++) componentOrigin[componentOf[i]] = glm::min(componentOrigin[componentOf[i]], originalPositions[i]);
    std::vector<std::vector<int>> linkedComponents(componentCount);
    for(const auto& link : groupingLinks){
        const int a = componentOf[link.first], b = componentOf[link.second];
        if(a != b){
            linkedComponents[a].push_back(b);
            linkedComponents[b].push_back(a);
        }
    }
    auto componentLess = [&](int a, int b){
        if(componentOrigin[a].y != componentOrigin[b].y) return componentOrigin[a].y < componentOrigin[b].y;
        return componentOrigin[a].x < componentOrigin[b].x;
    };
    std::vector<int> byOrigin(componentCount);
    std::iota(byOrigin.begin(), byOrigin.end(), 0);
    std::sort(byOrigin.begin(), byOrigin.end(), componentLess);
    std::vector<int> componentOrder;
    std::vector<int> looseNodes;
    std::vector<bool> placed(componentCount, false);
    for(int root : byOrigin){
        if(placed[root]) continue;
        const bool isolated = components[root].size() == 1 && degree[components[root][0]] == 0 && linkedComponents[root].empty();
        if(isolated){
            placed[root] = true;
            looseNodes.push_back(components[root][0]);
            continue;
        }
        std::vector<int> queue = {root};
        placed[root] = true;
        for(size_t q = 0; q < queue.size(); q++){
            componentOrder.push_back(queue[q]);
            auto neighbours = linkedComponents[queue[q]];
            std::sort(neighbours.begin(), neighbours.end(), componentLess);
            for(int neighbour : neighbours){
                if(!placed[neighbour]){
                    placed[neighbour] = true;
                    queue.push_back(neighbour);
                }
            }
        }
    }

    std::vector<detail::ComponentLayout> layouts;
    for(int c : componentOrder){
        layouts.push_back(detail::layoutComponent(components[c], sizes, originalPositions, edges, componentOf, c, settings));
    }
    for(int node : looseNodes){
        detail::ComponentLayout single;
        single.nodes = {node};
        single.positions = {glm::vec2(0)};
        single.size = sizes[node];
        layouts.push_back(single);
    }

    // Shelf packing: blocks flow left to right, wrapping at a width that keeps the
    // overall result roughly landscape but never narrower than the widest block.
    float maxWidth = 0;
    double area = 0;
    for(const auto& block : layouts){
        maxWidth = std::max(maxWidth, block.size.x);
        area += (block.size.x + settings.componentGap) * (block.size.y + settings.componentGap);
    }
    const float shelfWidth = std::max(maxWidth, (float)std::sqrt(area * 1.6));
    glm::vec2 origin(std::numeric_limits<float>::max());
    for(const auto& p : originalPositions) origin = glm::min(origin, p);
    origin = glm::vec2(detail::snapNearest(origin.x, settings.grid), detail::snapNearest(origin.y, settings.grid));
    float cursorX = 0, cursorY = 0, shelfHeight = 0;
    for(const auto& block : layouts){
        if(cursorX > 0 && cursorX + block.size.x > shelfWidth){
            cursorX = 0;
            cursorY = detail::snapUp(cursorY + shelfHeight + settings.componentGap, settings.grid);
            shelfHeight = 0;
        }
        for(size_t i = 0; i < block.nodes.size(); i++){
            positions[block.nodes[i]] = origin + glm::vec2(cursorX, cursorY) + block.positions[i];
        }
        cursorX = detail::snapUp(cursorX + block.size.x + settings.componentGap, settings.grid);
        shelfHeight = std::max(shelfHeight, block.size.y);
    }
    return positions;
}

} // namespace ofxOceanodeAutoLayout
