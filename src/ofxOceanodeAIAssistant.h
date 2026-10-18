#pragma once

#include "ofMain.h"
#include "imgui.h"
#include "ofxOceanodeColors.h"
#include "ofxOceanodeRequesty.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <future>
#include <limits>
#include <set>
#include <string>

// Requesty-backed "write/fix my code" assistant shared by code-editing nodes
// (Formula, DynGen, ...). A node owns one instance and supplies a Spec with its
// system prompt, how to build the user prompt and how to apply the result. The
// assistant handles the API key, model picker, async request, popup and preset
// persistence of the chosen model.
//
// Usage from a node:
//   setup():   ai.setup(spec);
//   update():  ai.update();                        // applies results on the main thread
//   GUI:       ai.drawButton(size); ... ai.drawPopup();  // same ImGui ID scope
//   presets:   ai.save(json); ai.load(json);
class ofxOceanodeAIAssistant {
public:
	struct Spec {
		// Popup header, e.g. "Create a Formula expression with Requesty".
		std::string title = "Write code with Requesty";
		// Optional line under the header; evaluated every frame.
		std::function<std::string()> subtitle;
		std::string generateHint = "Describe what you want.";
		std::string fixHint = "Describe the issue, or leave blank for a general repair.";
		std::string generateButton = "Generate";
		std::string fixButton = "Fix code";
		std::string generateModeLabel = "Generate";
		std::string fixModeLabel = "Fix current";
		std::string pendingHint = "The result will replace the current code when ready.";
		// Status shown after a successful apply.
		std::string readyMessage = "AI code ready";
		ImVec2 popupSize = ImVec2(560, 390);

		std::function<std::string()> systemPrompt;
		// Builds the user message. userRequest may be empty in fix mode.
		std::function<std::string(const std::string& userRequest, bool fixMode)> buildPrompt;
		// Applies cleaned code to the node. Return "" on success, or a message
		// that is shown as an error (e.g. the code was applied but does not compile).
		std::function<std::string(const std::string& code)> apply;
	};

	struct TextResult {
		std::string text;
		std::string error;
	};

	// Independent prose requests use the same credentials/model as this editor,
	// and return data without invoking the code-replacement callback.
	std::future<TextResult> generateText(const std::string &systemPrompt,
	                                   const std::string &userPrompt, int maxTokens = 512) const {
		if(apiKey.empty()) throw std::runtime_error("Configure the Requesty API key in the AI tab before generating a description.");
		const auto *selected = selectedModel();
		if(!selected) throw std::runtime_error("Choose an available model in the AI tab before generating a description.");
		const auto key = apiKey;
		const auto modelId = selected->id;
		const int tokens = std::max(1, std::min(maxTokens, selected->maxOutputTokens));
		return std::async(std::launch::async, [key, modelId, tokens, systemPrompt, userPrompt](){
			TextResult result;
			try{
				const ofJson body = {{"model", modelId}, {"max_tokens", tokens}, {"system", systemPrompt},
					{"messages", ofJson::array({{{"role", "user"}, {"content", userPrompt}}})}};
				result.text = ofxOceanodeRequesty::parseText(ofxOceanodeRequesty::request(key, &body));
			}catch(const std::exception &error){ result.error = error.what(); }
			return result;
		});
	}

	static constexpr const char* kDefaultModel = "zai/glm-5.3-flash";

	void setup(const Spec& s) {
		spec = s;
		refreshModels(false);
	}

	// Call from the node's update(). Polls the catalogue and finished requests.
	void update() {
		if(catalogue) catalogue->poll();
		if(!pending.load() || !future.valid()) return;
		if(future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

		Result result = future.get();
		if(!result.error.empty()) {
			finish(result.error, true);
			return;
		}
		const std::string code = cleanCode(result.text);
		if(code.empty()) {
			finish("The model returned no code. The current code was kept unchanged.", true);
			return;
		}
		const std::string applyError = spec.apply ? spec.apply(code) : "";
		finish(applyError.empty() ? spec.readyMessage : applyError, !applyError.empty());
	}

	bool isPending() const { return pending.load(); }
	bool hasError() const { return errorState; }
	const std::string& getStatus() const { return status; }
	void clearStatus() { status.clear(); }
	const std::string& getModel() const { return model; }

	// Draws the "AI" toggle button; opens the popup when clicked.
	bool drawButton(const ImVec2& size = ImVec2(34, 0)) {
		const bool busy = pending.load();
		ImGui::PushStyleColor(ImGuiCol_Button,
			busy ? IM_COL32(40, 20, 65, 255) : IM_COL32(60, 38, 90, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 58, 130, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(115, 78, 165, 255));
		const bool clicked = ImGui::Button(busy ? "...##oceanode_ai" : "AI##oceanode_ai", size) && !busy;
		ImGui::PopStyleColor(3);
		if(clicked) openPopup();
		return clicked;
	}

	// Opens the popup in the current ImGui ID scope.
	void openPopup() {
		// A popup edits a draft; dismissal restores the last retained prompt.
		cancelPromptEdit();
		promptBeforeEdit = promptBuf;
		promptEditing = true;
		promptPopupId = ImGui::GetID(kPopupName);
		ImGui::OpenPopup(kPopupName);
	}

	// Draw every frame, in the same ImGui ID scope as drawButton()/openPopup().
	void drawPopup() {
		ImGui::SetNextWindowSize(spec.popupSize, ImGuiCond_Appearing);
		const ImGuiID popupId = ImGui::GetID(kPopupName);
		if(!ImGui::BeginPopup(kPopupName, ImGuiWindowFlags_None)) {
			// Escape/outside-click dismissal is also cancellation. The same node may
			// render in both Canvas and Custom GUI; only its owning popup can cancel.
			if(promptPopupId == popupId) cancelPromptEdit();
			return;
		}

		ImGui::TextColored(ImVec4(0.72f, 0.60f, 1.0f, 1.0f), "%s", spec.title.c_str());
		ImGui::SameLine();
		if(apiKey.empty()) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "  [No API key]");
		else ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "  [Key loaded]");
		if(spec.subtitle) {
			const std::string subtitle = spec.subtitle();
			if(!subtitle.empty()) ImGui::TextDisabled("%s", subtitle.c_str());
		}
		ImGui::Spacing();
		drawModelPicker();
		ImGui::Spacing();

		auto modeButton = [](const char* label, bool active) {
			ImGui::PushStyleColor(ImGuiCol_Button,
				active ? IM_COL32(65, 40, 95, 255) : IM_COL32(28, 28, 38, 255));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 58, 130, 255));
			const bool clicked = ImGui::Button(label, ImVec2(122, 0));
			ImGui::PopStyleColor(2);
			return clicked;
		};
		if(modeButton((spec.generateModeLabel + "##oceanode_ai_mode").c_str(), !fixMode)) fixMode = false;
		ImGui::SameLine(0, 2);
		if(modeButton((spec.fixModeLabel + "##oceanode_ai_mode").c_str(), fixMode)) fixMode = true;

		ImGui::Separator();
		ImGui::Spacing();

		if(pending.load()) {
			static const char* spinner = "|/-\\";
			static int spinnerIndex = 0;
			spinnerIndex = (spinnerIndex + 1) % 4;
			ImGui::Text("Generating with %s... %c", model.c_str(), spinner[spinnerIndex]);
			ImGui::TextDisabled("%s", spec.pendingHint.c_str());
		} else if(done.load() && errorState) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1));
			ImGui::TextWrapped("%s", status.c_str());
			ImGui::PopStyleColor();
			ImGui::Spacing();
			if(ImGui::Button("Back##oceanode_ai", ImVec2(80, 0))) {
				done.store(false);
				errorState = false;
			}
			ImGui::SameLine();
			if(ImGui::Button("Close##oceanode_ai", ImVec2(80, 0))) {
				done.store(false);
				cancelPromptEdit();
				ImGui::CloseCurrentPopup();
			}
		} else if(done.load()) {
			done.store(false);
			cancelPromptEdit();
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::InputTextMultiline("##oceanode_ai_prompt", promptBuf,
				sizeof(promptBuf), ImVec2(-1, 100), ImGuiInputTextFlags_WordWrap);
			ImGui::TextDisabled("%s", fixMode ? spec.fixHint.c_str() : spec.generateHint.c_str());
			ImGui::Spacing();

			const bool canSubmit = !apiKey.empty() && selectedModel() &&
				(fixMode || promptBuf[0] != '\0');
			if(!canSubmit) ImGui::BeginDisabled();
			const std::string submitLabel =
				(fixMode ? spec.fixButton : spec.generateButton) + "##oceanode_ai_submit";
			if(ImGui::Button(submitLabel.c_str(), ImVec2(100, 0))) request(promptBuf);
			if(!canSubmit) ImGui::EndDisabled();
			ImGui::SameLine(0, 10);
			if(ImGui::Button("Cancel##oceanode_ai", ImVec2(70, 0))) {
				cancelPromptEdit();
				ImGui::CloseCurrentPopup();
			}

			if(apiKey.empty()) {
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(1, 0.65f, 0.3f, 1), "No API key found. Add one of:");
				ImGui::TextDisabled("  data/config/requesty.md: api_key: \"your-key\"");
				ImGui::TextDisabled("  REQUESTY_API_KEY environment variable");
				ImGui::TextDisabled("Then click Refresh models to reload the key.");
			}
		}

		ImGui::EndPopup();
	}

	void save(ofJson& json) const {
		json["requesty_model"] = model;
	}

	void load(const ofJson& json) {
		if(json.contains("requesty_model") && json["requesty_model"].is_string() &&
			!json["requesty_model"].get<std::string>().empty()) {
			model = json["requesty_model"].get<std::string>();
			provider = providerForModel(model);
		}
	}

	// Strips Markdown fences and trailing "..." / "…" continuation markers,
	// which models sometimes append and which no code language here accepts.
	static std::string cleanCode(std::string code) {
		using ofxOceanodeRequesty::trim;
		code = trim(code);

		if(code.rfind("```", 0) == 0) {
			const size_t firstLineEnd = code.find('\n');
			code = firstLineEnd == std::string::npos ? "" : code.substr(firstLineEnd + 1);
			const size_t closingFence = code.rfind("```");
			if(closingFence != std::string::npos) code.erase(closingFence);
			code = trim(code);
		}

		auto isEllipsisOnly = [](const std::string& line) {
			const std::string t = trim(line);
			if(t.empty()) return false;
			for(size_t i = 0; i < t.size();) {
				if(t[i] == '.') { i++; continue; }
				if(t.compare(i, 3, "\xE2\x80\xA6") == 0) { i += 3; continue; }
				return false;
			}
			return true;
		};
		while(!code.empty()) {
			size_t lineStart = code.find_last_of('\n');
			lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
			if(!isEllipsisOnly(code.substr(lineStart))) break;
			code = trim(code.substr(0, lineStart));
		}
		while(true) {
			if(code.size() >= 3 && code.compare(code.size() - 3, 3, "\xE2\x80\xA6") == 0) {
				code.erase(code.size() - 3);
			} else if(code.size() >= 3 && code.compare(code.size() - 3, 3, "...") == 0) {
				code.erase(code.size() - 3);
				while(!code.empty() && code.back() == '.') code.pop_back();
			} else break;
			code = trim(code);
		}
		return code;
	}

private:
	static constexpr const char* kPopupName = "##oceanode_ai_popup";

	using Result = TextResult;

	Spec spec;

	std::string apiKey;
	std::string model = kDefaultModel;
	std::string provider = providerForModel(kDefaultModel);
	char providerSearch[128] = {};
	std::shared_ptr<ofxOceanodeRequesty::Catalogue> catalogue;

	std::future<Result> future;
	std::atomic<bool> pending { false };
	std::atomic<bool> done { false };
	bool errorState = false;
	bool fixMode = false;
	std::string status;

	char promptBuf[2048] = {};
	std::string promptBeforeEdit;
	bool promptEditing = false;
	ImGuiID promptPopupId = 0;

	static std::string providerForModel(const std::string& id) {
		const auto slash = id.find('/');
		return slash == std::string::npos ? "Requesty" : id.substr(0, slash);
	}

	void finish(const std::string& message, bool isError) {
		status = message;
		errorState = isError;
		pending.store(false);
		done.store(true);
	}

	void cancelPromptEdit() {
		if(!promptEditing) return;
		std::snprintf(promptBuf, sizeof(promptBuf), "%s", promptBeforeEdit.c_str());
		promptEditing = false;
		promptPopupId = 0;
	}

	void refreshModels(bool force) {
		// Resolve the app's data path, never an installation-specific absolute path.
		std::ifstream config(ofToDataPath("config/requesty.md"));
		std::string key = ofxOceanodeRequesty::parseKey(config);
		if(key.empty()) {
			const char* environmentKey = std::getenv("REQUESTY_API_KEY");
			if(environmentKey) key = ofxOceanodeRequesty::trim(environmentKey);
		}
		apiKey = key;
		if(key.empty()) { catalogue.reset(); return; }
		catalogue = ofxOceanodeRequesty::sharedCatalogue(key);
		if(force) catalogue->refresh(key);
	}

	const ofxOceanodeRequesty::Model* selectedModel() const {
		if(!catalogue || !catalogue->loaded) return nullptr;
		for(const auto& m : catalogue->models) {
			if(m.id == model && m.provider == provider) return &m;
		}
		return nullptr;
	}

	void request(const std::string& userRequest) {
		if(pending.load()) return;
		if(apiKey.empty()) { finish("No API key", true); return; }
		const auto* selected = selectedModel();
		if(!selected) { finish("Choose an available Requesty model before generating.", true); return; }
		if(!spec.systemPrompt || !spec.buildPrompt) { finish("This node has no AI prompt configured.", true); return; }

		// Submission retains this prompt even if the API request later fails.
		promptBeforeEdit = userRequest;
		pending.store(true);
		done.store(false);
		errorState = false;
		status = "Generating...";

		const std::string key = apiKey;
		const std::string modelId = selected->id;
		const int maxTokens = selected->maxOutputTokens;
		const std::string systemPrompt = spec.systemPrompt();
		const std::string fullPrompt = spec.buildPrompt(userRequest, fixMode);

		future = std::async(std::launch::async,
			[key, modelId, maxTokens, systemPrompt, fullPrompt]() {
				Result result;
				try {
					const ofJson body = {
						{"model", modelId}, {"max_tokens", maxTokens}, {"system", systemPrompt},
						{"messages", ofJson::array({{{"role", "user"}, {"content", fullPrompt}}})}
					};
					result.text = ofxOceanodeRequesty::parseText(ofxOceanodeRequesty::request(key, &body));
				} catch(const std::exception& e) {
					result.error = std::string("Error: ") + e.what();
				}
				return result;
			});
	}

	void drawModelPicker() {
		if(catalogue) catalogue->poll();
		const bool disabled = pending.load();
		if(disabled) ImGui::BeginDisabled();
		std::set<std::string> providers;
		if(catalogue) for(const auto& m : catalogue->models) providers.insert(m.provider);
		ImGui::SetNextItemWidth(150);
		ImGui::SetNextWindowSizeConstraints(ImVec2(260, 0),
			ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
		if(ImGui::BeginCombo("Provider##oceanode_ai", provider.c_str(), ImGuiComboFlags_HeightLarge)) {
			if(ImGui::IsWindowAppearing()) {
				providerSearch[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##provider_search", "Search providers...",
				providerSearch, sizeof(providerSearch));
			ImGui::Separator();
			const auto query = ofToLower(ofxOceanodeRequesty::trim(providerSearch));
			// Keep the search field visible while the filtered results scroll.
			if(ImGui::BeginChild("##provider_results", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10), false)) {
				bool hasMatches = false;
				for(const auto& p : providers) {
					if(!query.empty() && ofToLower(p).find(query) == std::string::npos) continue;
					hasMatches = true;
					if(ImGui::Selectable(p.c_str(), p == provider)) {
						provider = p;
						if(providerForModel(model) != p) {
							for(const auto& m : catalogue->models) {
								if(m.provider == p) { model = m.id; break; }
							}
						}
						ImGui::CloseCurrentPopup();
					}
				}
				if(!hasMatches) ImGui::TextDisabled("No matching providers");
			}
			ImGui::EndChild();
			ImGui::EndCombo();
		}
		const auto slash = model.find('/');
		const auto modelName = slash == std::string::npos ? model : model.substr(slash + 1);
		ImGui::SetNextItemWidth(355);
		const float modelPopupWidth = std::max(355.0f,
			std::min(780.0f, ImGui::GetMainViewport()->WorkSize.x - 24.0f));
		ImGui::SetNextWindowSizeConstraints(ImVec2(modelPopupWidth, 0), ImVec2(modelPopupWidth, 400));
		if(ImGui::BeginCombo("Model##oceanode_ai", modelName.c_str())) {
			// Match the Nodes list stripes rather than the theme's global table colours.
			ImGui::PushStyleColor(ImGuiCol_TableRowBg, OceanodeColors::ZebraEven);
			ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, OceanodeColors::ZebraOdd);
			ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
			if(ImGui::BeginTable("##oceanode_ai_models", 4,
				ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Context (tokens)", ImGuiTableColumnFlags_WidthFixed, 125);
				ImGui::TableSetupColumn("Input ($/M)", ImGuiTableColumnFlags_WidthFixed, 105);
				ImGui::TableSetupColumn("Output ($/M)", ImGuiTableColumnFlags_WidthFixed, 105);
				ImGui::TableHeadersRow();
				if(catalogue) for(const auto& m : catalogue->models) {
					if(m.provider != provider) continue;
					ImGui::PushID(m.id.c_str());
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					const bool selected = m.id == model;
					if(ImGui::Selectable(m.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
						model = m.id;
					if(selected) ImGui::SetItemDefaultFocus();
					if(ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::TextUnformatted(m.id.c_str());
						if(m.inputPrice.max > m.inputPrice.min || m.outputPrice.max > m.outputPrice.min)
							ImGui::TextUnformatted("Price ranges reflect rates for different prompt lengths.");
						ImGui::EndTooltip();
					}
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(ofxOceanodeRequesty::formatContextWindow(m.contextWindow).c_str());
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(ofxOceanodeRequesty::formatPricePerMillion(m.inputPrice).c_str());
					ImGui::TableSetColumnIndex(3);
					ImGui::TextUnformatted(ofxOceanodeRequesty::formatPricePerMillion(m.outputPrice).c_str());
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::PopStyleColor(3);
			ImGui::EndCombo();
		}
		const bool refreshing = catalogue && catalogue->pending;
		if(refreshing) ImGui::BeginDisabled();
		if(ImGui::Button("Refresh models##oceanode_ai")) refreshModels(true);
		if(refreshing) ImGui::EndDisabled();
		if(disabled) ImGui::EndDisabled();
		ImGui::SameLine();
		if(refreshing) ImGui::TextDisabled("Loading models...");
		else if(catalogue && catalogue->loaded)
			ImGui::TextDisabled("%d models", static_cast<int>(catalogue->models.size()));
		if(catalogue && !catalogue->error.empty())
			ImGui::TextWrapped("Could not refresh models: %s", catalogue->error.c_str());
		if(catalogue && catalogue->loaded && !selectedModel())
			ImGui::TextWrapped("Selected model is unavailable. Choose another provider/model or refresh.");
	}
};
