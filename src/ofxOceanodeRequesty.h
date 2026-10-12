#pragma once

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

// Requesty transport and model catalogue shared by ofxOceanodeAIAssistant.
namespace ofxOceanodeRequesty {
using Json = nlohmann::json;

inline std::string trim(const std::string& s) {
	const auto first = s.find_first_not_of(" \t\r\n");
	if(first == std::string::npos) return "";
	return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

inline std::string parseKey(std::istream& file) {
	std::string line;
	while(std::getline(file, line)) {
		line = trim(line);
		if(line.rfind("api_key:", 0) != 0) continue;
		std::string key = trim(line.substr(8));
		if(key.size() >= 2 && (key.front() == '"' || key.front() == '\'')) {
			if(key.back() != key.front()) return "";
			key = trim(key.substr(1, key.size() - 2));
		}
		// A key is a single token; never treat prose or malformed config as a key.
		if(key.find_first_of(" \t\r\n\"'") != std::string::npos) return "";
		return key;
	}
	return "";
}

struct PriceRange {
	double min = -1;
	double max = -1;
};

inline PriceRange readPriceRange(const Json& item, const char* field) {
	PriceRange price;
	auto read = [&](const Json& source) {
		if(!source.is_object() || !source.contains(field) || !source[field].is_number()) return;
		const double value = source[field].get<double>();
		if(!std::isfinite(value) || value < 0) return;
		price.min = price.min < 0 ? value : std::min(price.min, value);
		price.max = std::max(price.max, value);
	};
	read(item);
	if(item.contains("pricing") && item["pricing"].is_array())
		for(const auto& tier : item["pricing"]) read(tier);
	return price;
}

inline std::string formatPricePerMillion(const PriceRange& price) {
	if(price.min < 0) return "n/a";
	auto format = [](double value) {
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(6) << value * 1000000.0;
		std::string text = stream.str();
		while(text.back() == '0') text.pop_back();
		if(text.back() == '.') text.pop_back();
		return text;
	};
	const auto low = format(price.min);
	const auto high = format(price.max);
	return "$" + low + (low != high ? "-" + high : "");
}

inline std::string formatContextWindow(long long tokens) {
	if(tokens <= 0) return "n/a";
	std::string text = std::to_string(tokens);
	for(int position = static_cast<int>(text.size()) - 3; position > 0; position -= 3)
		text.insert(position, ",");
	return text;
}

struct Model {
	std::string id;
	std::string provider;
	std::string name;
	int maxOutputTokens = 8192;
	long long contextWindow = 0;
	PriceRange inputPrice;
	PriceRange outputPrice;
};

inline std::vector<Model> parseModels(const Json& response) {
	if(!response.contains("data") || !response["data"].is_array())
		throw std::runtime_error("Unexpected Requesty model catalogue.");
	std::vector<Model> models;
	std::set<std::string> seen;
	for(const auto& item : response["data"]) {
		if(!item.is_object() || !item.contains("id") || !item["id"].is_string()) continue;
		if(item.contains("api") && item["api"] != "chat") continue;
		const auto id = item["id"].get<std::string>();
		if(id.empty() || id.rfind("policy/", 0) == 0 || !seen.insert(id).second) continue;
		const auto slash = id.find('/');
		Model model;
		model.id = id;
		model.provider = slash == std::string::npos ? "Requesty" : id.substr(0, slash);
		model.name = slash == std::string::npos ? id : id.substr(slash + 1);
		if(model.provider.empty() || model.name.empty()) continue;
		if(item.contains("max_output_tokens") && item["max_output_tokens"].is_number_integer()) {
			const auto limit = item["max_output_tokens"].get<long long>();
			if(limit > 0) model.maxOutputTokens = static_cast<int>(std::min(limit, 8192LL));
		}
		if(item.contains("context_window") && item["context_window"].is_number_integer())
			model.contextWindow = std::max(0LL, item["context_window"].get<long long>());
		model.inputPrice = readPriceRange(item, "input_price");
		model.outputPrice = readPriceRange(item, "output_price");
		models.push_back(std::move(model));
	}
	std::sort(models.begin(), models.end(), [](const Model& a, const Model& b) { return a.id < b.id; });
	return models;
}

inline std::string parseText(const Json& response) {
	if(response.value("stop_reason", "") == "max_tokens")
		throw std::runtime_error("The model reached its output limit. Try a simpler request or another model; the code was kept unchanged.");
	if(response.value("stop_reason", "") == "tool_use")
		throw std::runtime_error("The model requested a tool instead of returning code.");
	if(!response.contains("content") || !response["content"].is_array())
		throw std::runtime_error("Unexpected Requesty response.");
	std::string text;
	for(const auto& block : response["content"]) {
		if(!block.is_object() || block.value("type", "") != "text" ||
			!block.contains("text") || !block["text"].is_string()) continue;
		if(!text.empty()) text += '\n';
		text += block["text"].get<std::string>();
	}
	text = trim(text);
	if(text.empty()) throw std::runtime_error("The model returned no text. Try another model.");
	return text;
}

inline std::string configQuote(const std::string& value) {
	std::string quoted = "\"";
	for(char c : value) {
		switch(c) {
			case '\\': quoted += "\\\\"; break;
			case '"': quoted += "\\\""; break;
			case '\n': quoted += "\\n"; break;
			case '\r': quoted += "\\r"; break;
			case '\t': quoted += "\\t"; break;
			default: quoted += c;
		}
	}
	return quoted + '"';
}

inline std::string shellQuote(const std::string& value) {
	std::string quoted = "'";
	for(char c : value) quoted += c == '\'' ? "'\\''" : std::string(1, c);
	return quoted + '\'';
}

inline Json parseHttpResponse(const std::string& body, int status) {
	const auto object = Json::parse(body, nullptr, false);
	if(status < 200 || status >= 300 || (object.is_object() && object.contains("error"))) {
		std::string message = "Requesty HTTP " + std::to_string(status);
		if(object.is_object() && object.contains("error") && object["error"].is_object() &&
			object["error"].contains("message") && object["error"]["message"].is_string())
			message += ": " + object["error"]["message"].get<std::string>();
		throw std::runtime_error(message);
	}
	if(!object.is_object()) throw std::runtime_error("Requesty returned an invalid JSON response.");
	return object;
}

inline Json request(const std::string& apiKey, const Json* body = nullptr) {
	// Keep credentials out of shell arguments. mkstemp creates a private (0600)
	// curl config, removed on every exit, containing the headers and optional body.
	std::string tempDir = std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp/";
	if(!tempDir.empty() && tempDir.back() != '/') tempDir += '/';
	std::string pattern = tempDir + "oceanode_requesty_XXXXXX";
	std::vector<char> filename(pattern.begin(), pattern.end());
	filename.push_back('\0');
	const int fd = mkstemp(filename.data());
	if(fd < 0) throw std::runtime_error("Cannot create the temporary Requesty request.");
	struct TempFile {
		std::string path;
		~TempFile() { std::remove(path.c_str()); }
	} temp { filename.data() };
	FILE* file = fdopen(fd, "w");
	if(!file) { close(fd); throw std::runtime_error("Cannot write the temporary Requesty request."); }
	std::string config = "url = " + configQuote(body ? "https://router.requesty.ai/v1/messages" : "https://router.requesty.ai/v1/models") + '\n';
	config += "header = " + configQuote("x-api-key: " + apiKey) + '\n';
	if(body) {
		config += "request = \"POST\"\n";
		config += "header = \"content-type: application/json\"\n";
		config += "header = \"anthropic-version: 2023-06-01\"\n";
		config += "data = " + configQuote(body->dump()) + '\n';
	}
	const bool written = fwrite(config.data(), 1, config.size(), file) == config.size();
	const int closed = fclose(file);
	if(!written || closed != 0) throw std::runtime_error("Cannot write the temporary Requesty request.");
	const auto command = "curl --disable -sS --connect-timeout 10 --max-time 90 --config " +
		shellQuote(temp.path) + " --write-out '\n%{http_code}' 2>/dev/null";
	FILE* pipe = popen(command.c_str(), "r");
	if(!pipe) throw std::runtime_error("Could not start curl for Requesty.");
	std::string output;
	char chunk[4096];
	bool tooLarge = false;
	while(fgets(chunk, sizeof(chunk), pipe)) {
		if(output.size() + std::strlen(chunk) <= 16 * 1024 * 1024) output += chunk;
		else tooLarge = true;
	}
	const int status = pclose(pipe);
	if(status != 0) throw std::runtime_error("Requesty request failed or timed out. Check your connection.");
	if(tooLarge) throw std::runtime_error("Requesty response exceeded the size limit.");
	const auto marker = output.rfind('\n');
	if(marker == std::string::npos) throw std::runtime_error("Requesty returned no HTTP status.");
	const int httpStatus = std::atoi(output.c_str() + marker + 1);
	return parseHttpResponse(output.substr(0, marker), httpStatus);
}

struct CatalogueResult {
	std::vector<Model> models;
	std::string error;
};
struct Catalogue {
	std::vector<Model> models;
	std::future<CatalogueResult> future;
	bool loaded = false;
	bool pending = false;
	std::string error;
	// Called and polled on the Oceanode main thread; workers return values only.
	void refresh(const std::string& key) {
		if(pending) return;
		future = std::async(std::launch::async, [key]() {
			CatalogueResult result;
			try { result.models = parseModels(request(key)); }
			catch(const std::exception& e) { result.error = e.what(); }
			return result;
		});
		pending = true;
		error.clear();
	}
	void poll() {
		if(!pending || !future.valid() || future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
		auto result = future.get();
		if(result.error.empty()) { models = std::move(result.models); loaded = true; }
		error = std::move(result.error);
		pending = false;
	}
};

inline std::shared_ptr<Catalogue> sharedCatalogue(const std::string& key) {
	// One catalogue/fetch per credential, shared across all node instances.
	static std::map<std::string, std::shared_ptr<Catalogue>> catalogues;
	auto& catalogue = catalogues[key];
	if(!catalogue) {
		catalogue = std::make_shared<Catalogue>();
		catalogue->refresh(key);
	}
	return catalogue;
}
}
