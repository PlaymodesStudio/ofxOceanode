#pragma once

// Main-thread CPU wall times. Independent of OF/ImGui so accounting can be tested
// deterministically. Records and scopes outlive nodes safely during graph edits.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace oceanodeProfiling {
enum class Phase : unsigned { Update, Draw, Parameters, Gui, Count };
constexpr unsigned phaseCount = static_cast<unsigned>(Phase::Count);

struct Measurement {
    double selfNs = 0;
    double inclusiveNs = 0;
    std::uint64_t calls = 0;
};

struct Sample {
    std::array<Measurement, phaseCount> phases{};
    double selfNs() const {
        double result = 0;
        for(const auto& phase : phases) result += phase.selfNs;
        return result;
    }
    double inclusiveNs() const {
        double result = 0;
        for(const auto& phase : phases) result += phase.inclusiveNs;
        return result;
    }
    void add(const Sample& other, int sign = 1) {
        for(unsigned i = 0; i < phaseCount; ++i){
            phases[i].selfNs += sign * other.phases[i].selfNs;
            phases[i].inclusiveNs += sign * other.phases[i].inclusiveNs;
            if(sign > 0) phases[i].calls += other.phases[i].calls;
            else phases[i].calls -= other.phases[i].calls;
        }
    }
};

class Session;
struct Record;
struct PublicationSample {
    std::uint64_t count = 0, segment = 0;
    std::int64_t firstNs = 0, lastNs = 0;
};

struct Publication {
    std::weak_ptr<Record> owner;
    std::function<std::string()> name;
    bool outputCapable = true, bidirectional = true;
    PublicationSample pending;
    std::vector<PublicationSample> history;
};

struct PublicationStats {
    std::uint64_t count = 0;
    double perFrame = 0, perSecond = -1, intervalMs = -1;
};

struct Record;
struct ListenerBinding {
    std::weak_ptr<Record> owner;
    virtual ~ListenerBinding() = default;
    virtual void setEnabled(bool enabled) = 0;
};
struct Record {
    const std::thread::id ownerThread = std::this_thread::get_id();
    std::weak_ptr<Session> session;
    std::atomic<bool> enabled{false};
    bool registered = false;
    std::vector<std::weak_ptr<ListenerBinding>> bindings;
    std::vector<std::weak_ptr<Publication>> publications;
    Sample pending;
    Sample total;
    std::vector<Sample> history;
    // Cleared by the model destructor. UI snapshots copy strings, never a model
    // pointer, and can therefore survive preset loads and node deletion.
    struct Identity { std::string name, path; };
    std::function<Identity()> identity;

    void setEnabled(bool value) {
        enabled.store(value, std::memory_order_relaxed);
        bindings.erase(std::remove_if(bindings.begin(), bindings.end(), [&](const std::weak_ptr<ListenerBinding>& weak){
            auto binding = weak.lock();
            if(!binding || binding->owner.lock().get() != this) return true;
            binding->setEnabled(value);
            return false;
        }), bindings.end());
    }

    double peakSelfNs() const {
        double result = 0;
        for(const auto& sample : history) result = std::max(result, sample.selfNs());
        return result;
    }
    template<class Fn> void forEachPublication(Fn fn) {
        publications.erase(std::remove_if(publications.begin(), publications.end(), [&](const std::weak_ptr<Publication>& weak){
            auto publication = weak.lock();
            if(!publication || publication->owner.lock().get() != this) return true;
            fn(*publication);
            return false;
        }), publications.end());
    }
};

class Session : public std::enable_shared_from_this<Session> {
public:
    using Clock = std::int64_t (*)();
    explicit Session(Clock clock = steadyNow) : clock(clock) {}
    ~Session() {
        for(const auto& weak : records)
            if(auto record = weak.lock()) if(record->session.expired()) record->setEnabled(false);
    }

    static std::int64_t steadyNow() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void attach(const std::shared_ptr<Record>& record) {
        if(record->session.lock().get() == this) return;
        // First registration is constant time. Repeated preset loads must not
        // introduce a scan of all live nodes for every newly created node.
        if(record->registered){
            records.erase(std::remove_if(records.begin(), records.end(), [&](const std::weak_ptr<Record>& weak){
                return weak.expired() || weak.lock() == record;
            }), records.end());
        }
        record->registered = true;
        if(records.size() >= nextPrune){
            forEachRecord([](Record&){});
            nextPrune = std::max<std::size_t>(64, records.size() * 2);
        }
        record->session = shared_from_this();
        record->pending = {};
        record->total = {};
        record->history.clear();
        if(recording) record->history.resize(window);
        record->forEachPublication([this](Publication& publication){ resetPublication(publication); });
        record->setEnabled(recording);
        records.push_back(record);
    }

    bool isRecording() const { return recording; }
    std::uint64_t frameCount() const { return frames; }
    std::size_t sampleFrames() const { return std::min<std::uint64_t>(frames, window); }
    std::size_t windowFrames() const { return window; }
    std::uint64_t incompleteSpans() const { return incomplete; }
    std::uint64_t scopeEpoch() const { return epoch; }
    double sampledSeconds() const { return durationNs / 1e9; }

    void preparePublication(Publication& publication) {
        if(recording && publication.outputCapable && publication.history.size() != window)
            publication.history.assign(window, {});
    }

    PublicationStats publicationStats(const Publication& publication) const {
        PublicationStats stats;
        std::uint64_t intervals = 0, previousSegment = 0;
        std::int64_t previousNs = 0;
        double intervalSumNs = 0;
        bool previous = false;
        // Walk the ring in timestamp order; idle frames still contribute to
        // the rate denominator, and discarded frames break interval continuity.
        for(std::size_t i = 0; i < publication.history.size(); ++i){
            const auto& sample = publication.history[(cursor + i) % publication.history.size()];
            if(!sample.count) continue;
            stats.count += sample.count;
            intervalSumNs += sample.lastNs - sample.firstNs;
            intervals += sample.count - 1;
            if(previous && sample.segment == previousSegment){
                intervalSumNs += sample.firstNs - previousNs;
                ++intervals;
            }
            previous = true;
            previousNs = sample.lastNs;
            previousSegment = sample.segment;
        }
        if(sampleFrames()) stats.perFrame = static_cast<double>(stats.count) / sampleFrames();
        if(durationNs > 0) stats.perSecond = stats.count * 1e9 / durationNs;
        if(intervals) stats.intervalMs = intervalSumNs / intervals / 1e6;
        return stats;
    }
    std::vector<std::shared_ptr<Record>> liveRecords() {
        std::vector<std::shared_ptr<Record>> result;
        forEachRecord([](Record&){});
        for(const auto& weak : records)
            if(auto record = weak.lock()) if(record->identity) result.push_back(record);
        return result;
    }

    void setRecording(bool enabled) {
        if(recording == enabled) return;
        // Stop at a boundary; a partially captured frame must not skew averages.
        discardOpenSpans();
        recording = enabled;
        if(enabled) stack.reserve(128);
        frameOpen = false;
        frameInvalid = false;
        forEachRecord([enabled](Record& record){
            record.pending = {};
            record.forEachPublication([](Publication& publication){ publication.pending = {}; });
            record.setEnabled(enabled);
        });
        if(enabled) reset();
    }

    void reset(std::size_t framesInWindow = 0) {
        discardOpenSpans();
        if(framesInWindow) window = framesInWindow;
        frames = 0;
        ++epoch;
        cursor = 0;
        incomplete = 0;
        frameOpen = false;
        frameInvalid = false;
        durationNs = 0;
        durations.assign(recording ? window : 0, 0);
        ++publicationSegment;
        forEachRecord([this](Record& record){
            record.pending = {};
            record.total = {};
            record.history.assign(recording ? window : 0, {});
            record.forEachPublication([this](Publication& publication){ resetPublication(publication); });
        });
    }

    // Called once by the root Oceanode update, before OSC/time/automation. Draw
    // and GUI events belong to that same frame; Macro containers never call it.
    void nextFrame() {
        if(!recording) return;
        const auto boundaryNs = clock();
        const bool balanced = stack.empty();
        discardOpenSpans();
        if(frameOpen && balanced && !frameInvalid){
            forEachRecord([this](Record& record){
                if(record.history.size() != window) record.history.assign(window, {});
                record.total.add(record.history[cursor], -1);
                record.total.add(record.pending);
                record.history[cursor] = record.pending;
                record.pending = {};
                record.forEachPublication([this](Publication& publication){
                    preparePublication(publication);
                    if(!publication.history.empty()) publication.history[cursor] = publication.pending;
                    publication.pending = {};
                });
            });
            durationNs -= durations[cursor];
            durations[cursor] = std::max<std::int64_t>(0, boundaryNs - frameStartNs);
            durationNs += durations[cursor];
            cursor = (cursor + 1) % window;
            ++frames;
        }else{
            forEachRecord([](Record& record){
                record.pending = {};
                record.forEachPublication([](Publication& publication){ publication.pending = {}; });
            });
            ++publicationSegment;
        }
        frameStartNs = boundaryNs;
        frameOpen = true;
        frameInvalid = false;
        ++epoch;
    }

    std::uint64_t begin(const std::shared_ptr<Record>& record, Phase phase) {
        if(!recording || !frameOpen) return 0;
        bool outermost = true;
        for(const auto& entry : stack)
            if(entry.record == record) { outermost = false; break; }
        const auto token = ++nextToken;
        stack.push_back({record, phase, token, clock(), 0, outermost});
        return token;
    }

    void end(std::uint64_t token) {
        if(!token || stack.empty()) return;
        if(stack.back().token != token){
            // Consumed OF events can skip our final listener. Never leave an
            // orphan scope charging unrelated nodes for the rest of the frame.
            if(std::any_of(stack.begin(), stack.end(), [token](const Entry& e){return e.token == token;})){
                discardOpenSpans();
                frameInvalid = true;
            }
            return;
        }
        auto entry = std::move(stack.back());
        stack.pop_back();
        const double elapsed = std::max<std::int64_t>(0, clock() - entry.start);
        auto& measurement = entry.record->pending.phases[static_cast<unsigned>(entry.phase)];
        measurement.selfNs += std::max(0.0, elapsed - entry.childrenNs);
        // Re-entering the same node (including its output notifications) must
        // not count its inclusive duration twice.
        if(entry.outermost) measurement.inclusiveNs += elapsed;
        ++measurement.calls;
        if(!stack.empty()) stack.back().childrenNs += elapsed;
    }

    void publish(Publication& publication, std::uint64_t token) {
        if(!publication.outputCapable || !token || stack.empty() || stack.back().token != token) return;
        // Reuse the existing listener-entry timestamp; publication counting
        // introduces no additional per-notification clock reads.
        const auto timestamp = stack.back().start;
        auto& sample = publication.pending;
        if(!sample.count) { sample.firstNs = timestamp; sample.segment = publicationSegment; }
        sample.lastNs = timestamp;
        ++sample.count;
    }

private:
    void resetPublication(Publication& publication) {
        publication.pending = {};
        publication.history.assign(recording && publication.outputCapable ? window : 0, {});
    }
    struct Entry {
        std::shared_ptr<Record> record;
        Phase phase;
        std::uint64_t token;
        std::int64_t start;
        double childrenNs;
        bool outermost;
    };
    template<class Fn> void forEachRecord(Fn fn) {
        records.erase(std::remove_if(records.begin(), records.end(), [&](const std::weak_ptr<Record>& weak){
            auto record = weak.lock();
            if(!record || record->session.lock().get() != this) return true;
            fn(*record);
            return false;
        }), records.end());
    }
    void discardOpenSpans() {
        incomplete += stack.size();
        stack.clear();
    }

    Clock clock;
    std::vector<std::weak_ptr<Record>> records;
    std::vector<Entry> stack;
    bool recording = false;
    bool frameOpen = false;
    bool frameInvalid = false;
    std::size_t window = 120;
    std::size_t cursor = 0;
    std::uint64_t frames = 0;
    std::uint64_t nextToken = 0;
    std::uint64_t incomplete = 0;
    std::uint64_t epoch = 0;
    std::size_t nextPrune = 64;
    std::vector<std::int64_t> durations;
    std::int64_t frameStartNs = 0;
    double durationNs = 0;
    std::uint64_t publicationSegment = 0;
};

class Scope {
public:
    Scope() = default;
    Scope(const std::shared_ptr<Record>& record, Phase phase) { start(record, phase); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&& other) noexcept : session(std::move(other.session)), token(other.token), epoch(other.epoch) { other.token = 0; }
    ~Scope() { finish(); }

    void start(const std::shared_ptr<Record>& record, Phase phase) {
        // Worker-thread parameter notifications are intentionally excluded.
        // Check this before touching session state or the main-thread stack.
        if(!record || !record->enabled.load(std::memory_order_relaxed) ||
           record->ownerThread != std::this_thread::get_id()) return;
        finish();
        session = record->session.lock();
        if(session) { token = session->begin(record, phase); epoch = session->scopeEpoch(); }
        if(!token) session.reset();
    }
    void finish() {
        if(isCurrent()) session->end(token);
        session.reset();
        token = 0;
    }
    bool isCurrent() const { return session && token && epoch == session->scopeEpoch(); }
    void publish(Publication& publication) {
        if(isCurrent()) session->publish(publication, token);
    }
private:
    std::shared_ptr<Session> session;
    std::uint64_t token = 0;
    std::uint64_t epoch = 0;
};
}
