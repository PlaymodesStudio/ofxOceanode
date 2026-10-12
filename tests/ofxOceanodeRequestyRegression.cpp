// Run from the addon root:
// clang++ -std=c++17 -I../../libs/json/include tests/ofxOceanodeRequestyRegression.cpp -o /tmp/oceanode-requesty-test && /tmp/oceanode-requesty-test
#include "../src/ofxOceanodeRequesty.h"
#include <cassert>
#include <sstream>
#include <iostream>
using namespace ofxOceanodeRequesty;

template<class F> void fails(F f, const std::string& message) {
	bool failed = false;
	try { f(); } catch(const std::runtime_error& e) { failed = std::string(e.what()).find(message) != std::string::npos; }
	assert(failed);
}
int main() {
	std::istringstream config("# Requesty\n```yaml\napi_key: \"rqsty-sk-example\"\n```\n");
	assert(parseKey(config) == "rqsty-sk-example");
	std::istringstream blank("api_key: \"\"\n"); assert(parseKey(blank).empty());
	std::istringstream single("api_key: 'rqsty-sk-example'\r\n"); assert(parseKey(single) == "rqsty-sk-example");
	std::istringstream invalid("api_key: \"incomplete\n"); assert(parseKey(invalid).empty());
	std::istringstream prose("api_key: paste your key here\n"); assert(parseKey(prose).empty());
	const Json catalogue = {{"data", Json::array({
		{{"id", "zai/glm-5.3-flash"}, {"api", "chat"}, {"max_output_tokens", 128000},
		 {"context_window", 1000000}, {"input_price", 1.5e-7}, {"output_price", 5e-7}},
		{{"id", "anthropic/claude-sonnet-4-5"}, {"max_output_tokens", 2048}},
		{{"id", "openai/gpt-4.1"}}, {{"id", "zai/glm-5.3-flash"}},
		{{"id", "glm-5.3-flash"}}, {{"id", "openai/embedding"}, {"api", "embedding"}},
		{{"id", "policy/unused"}}, {{"id", ""}}, {{"id", 12}}
	})}};
	const auto models = parseModels(catalogue);
	assert(models.size() == 4);
	assert(models.front().id == "anthropic/claude-sonnet-4-5");
	assert(models.front().maxOutputTokens == 2048);
	assert(models[1].provider == "Requesty");
	assert(models.back().provider == "zai" && models.back().name == "glm-5.3-flash");
	assert(models.back().maxOutputTokens == 8192);
	assert(formatContextWindow(models.back().contextWindow) == "1,000,000");
	assert(formatPricePerMillion(models.back().inputPrice) == "$0.15");
	assert(formatPricePerMillion(models.back().outputPrice) == "$0.5");
	assert(formatContextWindow(models.front().contextWindow) == "n/a");
	assert(formatPricePerMillion(models.front().inputPrice) == "n/a");
	const Json tiered = {{"pricing", Json::array({
		{{"input_price", 6e-6}, {"output_price", 25e-6}, {"prompt_tokens_threshold", 200000}},
		{{"input_price", 3e-6}, {"output_price", 15e-6}, {"prompt_tokens_threshold", 0}}
	})}};
	assert(formatPricePerMillion(readPriceRange(tiered, "input_price")) == "$3-6");
	assert(formatPricePerMillion(readPriceRange(tiered, "output_price")) == "$15-25");
	assert(formatPricePerMillion(readPriceRange({{"input_price", 0}}, "input_price")) == "$0");
	assert(formatPricePerMillion(readPriceRange({{"input_price", -1}}, "input_price")) == "n/a");
	assert(formatPricePerMillion(readPriceRange({{"input_price", nullptr}}, "input_price")) == "n/a");
	assert(formatPricePerMillion(readPriceRange({{"input_price", "unknown"}}, "input_price")) == "n/a");
	assert(formatContextWindow(200000) == "200,000");
	assert(formatContextWindow(0) == "n/a");
	assert(parseModels({{"data", Json::array()}}).empty());
	fails([] { parseModels({}); }, "catalogue");
	Json response = {{"stop_reason", "end_turn"}, {"content", Json::array({
		{{"type", "thinking"}, {"thinking", "Reasoning must never become source"}},
		{{"type", "text"}, {"text", "$o1 = $i1 * 2;"}},
		{{"type", "text"}, {"text", "$o2 = $i1 + 1"}}
	})}};
	assert(parseText(response) == "$o1 = $i1 * 2;\n$o2 = $i1 + 1");
	response["stop_reason"] = "max_tokens";
	fails([&] { parseText(response); }, "output limit");
	response["stop_reason"] = "tool_use";
	fails([&] { parseText(response); }, "tool");
	fails([] { parseText({{"content", Json::array({{{"type", "thinking"}, {"thinking", "only reasoning"}}})}}); }, "no text");
	fails([] { parseText({{"content", Json::array({{{"type", "text"}, {"text", "   "}}})}}); }, "no text");
	assert(parseHttpResponse("{\"data\":[]}", 200).contains("data"));
	fails([] { parseHttpResponse("{\"error\":{\"message\":\"Invalid key\"}}", 401); }, "401: Invalid key");
	fails([] { parseHttpResponse("<html>Bad Gateway</html>", 502); }, "502");
	fails([] { parseHttpResponse("not JSON", 200); }, "invalid JSON");
	assert(configQuote("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"");
	std::cout << "Requesty configuration, catalogue, response and error regressions passed.\n";
}
