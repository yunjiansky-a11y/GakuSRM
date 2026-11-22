// GakuSMM — GakuLoader scene model manager panel page.
// Independent IL2CPP scene scan (FindObjectsOfType(Type, includeInactive)),
// naming-rule based folder tree, hide/restore via GameObject.SetActive.

#include "GakuPlugin.h"
#include "imgui.h"

#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>
#include <algorithm>
#include <set>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{
	// ---------------------------------------------------------------- logging
	void Log(const char* format, ...)
	{
		static std::mutex mutex;
		std::lock_guard<std::mutex> lock(mutex);
		wchar_t dll[MAX_PATH]{};
		HMODULE module = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&Log), &module);
		GetModuleFileNameW(module, dll, MAX_PATH);
		std::wstring path(dll);
		const size_t slash = path.find_last_of(L"\\/");
		if (slash != std::wstring::npos) path.resize(slash);
		path += L"\\GakuSMM.log";
		FILE* file = _wfopen(path.c_str(), L"a");
		if (!file) return;
		SYSTEMTIME time{};
		GetLocalTime(&time);
		fprintf(file, "%02d:%02d:%02d.%03d ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
		va_list args;
		va_start(args, format);
		vfprintf(file, format, args);
		va_end(args);
		fputc('\n', file);
		fclose(file);
	}

	// ----------------------------------------------------------------- IL2CPP
	using fn_domain_get = void* (*)();
	using fn_domain_assemblies = void** (*)(void*, size_t*);
	using fn_assembly_image = void* (*)(void*);
	using fn_image_name = const char* (*)(void*);
	using fn_class_from_name = void* (*)(void*, const char*, const char*);
	using fn_method_from_name = void* (*)(void*, const char*, int);
	using fn_invoke = void* (*)(void*, void*, void**, void**);
	using fn_class_type = void* (*)(void*);
	using fn_type_object = void* (*)(void*);
	using fn_array_length = uintptr_t (*)(void*);

	struct Api
	{
		fn_class_from_name classFromName = nullptr;
		fn_method_from_name methodFromName = nullptr;
		fn_invoke invoke = nullptr;
		fn_class_type classType = nullptr;
		fn_type_object typeObject = nullptr;
		fn_array_length arrayLength = nullptr;
		void* gameObjectClass = nullptr;
		void* gameObjectType = nullptr;
		void* findAll = nullptr;
		void* getName = nullptr;
		void* getActive = nullptr;
		void* setActive = nullptr;
		void* instanceId = nullptr;
		bool ready = false;
	};

	Api g_api;

	struct Vec3 { float x, y, z; };

	void* g_cameraClass = nullptr;
	void* g_transformClass = nullptr;
	void* g_cameraMain = nullptr;
	void* g_worldToScreen = nullptr;
	void* g_pixelWidth = nullptr;
	void* g_pixelHeight = nullptr;
	void* g_getTransform = nullptr;
	void* g_getPosition = nullptr;
	void* g_childCount = nullptr;
	void* g_parent = nullptr;
	void* g_getGameObject = nullptr;

	std::set<int> g_selected;
	std::mutex g_selectedMutex;

	std::unordered_map<std::string, std::pair<std::string, std::string>> g_nameMap;

	void LoadNameMap()
	{
		wchar_t dll[MAX_PATH]{};
		HMODULE module = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&LoadNameMap), &module);
		GetModuleFileNameW(module, dll, MAX_PATH);
		std::wstring path(dll);
		const size_t slash = path.find_last_of(L"\\/");
		if (slash != std::wstring::npos) path.resize(slash + 1);
		path += L"GakuSMM_naming.tsv";
		FILE* file = _wfopen(path.c_str(), L"rb");
		if (!file) { Log("[SMM] naming map missing"); return; }
		char line[1024];
		bool header = true;
		while (fgets(line, sizeof(line), file))
		{
			if (header) { header = false; continue; }
			char* first = strchr(line, '\t');
			if (!first) continue;
			char* second = strchr(first + 1, '\t');
			if (!second) continue;
			*first = '\0';
			*second = '\0';
			std::string name(line);
			std::string category(first + 1);
			std::string label(second + 1);
			while (!label.empty() && (label.back() == '\n' || label.back() == '\r')) label.pop_back();
			if (!name.empty()) g_nameMap[name] = {category, label};
		}
		fclose(file);
		Log("[SMM] naming map entries=%zu", g_nameMap.size());
	}

	template <typename T>
	T Resolve(HMODULE module, const char* name) { return reinterpret_cast<T>(GetProcAddress(module, name)); }

	void* Invoke(void* target, void* method, void** args)
	{
		if (!method) return nullptr;
		void* exception = nullptr;
		void* result = nullptr;
		try { result = g_api.invoke(target, method, args, &exception); }
		catch (...) { Log("[SMM] native exception"); return nullptr; }
		if (exception) { Log("[SMM] managed exception"); return nullptr; }
		return result;
	}

	bool InitializeApi()
	{
		if (g_api.ready) return true;
		HMODULE module = GetModuleHandleA("GameAssembly.dll");
		if (!module) return false;
		fn_domain_get domainGet = Resolve<fn_domain_get>(module, "il2cpp_domain_get");
		fn_domain_assemblies domainAssemblies = Resolve<fn_domain_assemblies>(module, "il2cpp_domain_get_assemblies");
		fn_assembly_image assemblyImage = Resolve<fn_assembly_image>(module, "il2cpp_assembly_get_image");
		fn_image_name imageName = Resolve<fn_image_name>(module, "il2cpp_image_get_name");
		g_api.classFromName = Resolve<fn_class_from_name>(module, "il2cpp_class_from_name");
		g_api.methodFromName = Resolve<fn_method_from_name>(module, "il2cpp_class_get_method_from_name");
		g_api.invoke = Resolve<fn_invoke>(module, "il2cpp_runtime_invoke");
		g_api.classType = Resolve<fn_class_type>(module, "il2cpp_class_get_type");
		g_api.typeObject = Resolve<fn_type_object>(module, "il2cpp_type_get_object");
		g_api.arrayLength = Resolve<fn_array_length>(module, "il2cpp_array_length");
		if (!domainGet || !domainAssemblies || !assemblyImage || !imageName || !g_api.classFromName ||
			!g_api.methodFromName || !g_api.invoke || !g_api.classType || !g_api.typeObject || !g_api.arrayLength)
			return false;
		void* domain = domainGet();
		if (!domain) return false;
		size_t count = 0;
		void** assemblies = domainAssemblies(domain, &count);
		if (!assemblies) return false;
		void* coreImage = nullptr;
		for (size_t i = 0; i < count; ++i)
		{
			if (!assemblies[i]) continue;
			void* image = assemblyImage(assemblies[i]);
			const char* name = image ? imageName(image) : nullptr;
			if (name && strcmp(name, "UnityEngine.CoreModule.dll") == 0) { coreImage = image; break; }
		}
		if (!coreImage) return false;
		g_api.gameObjectClass = g_api.classFromName(coreImage, "UnityEngine", "GameObject");
		void* objectClass = g_api.classFromName(coreImage, "UnityEngine", "Object");
		if (!g_api.gameObjectClass || !objectClass) return false;
		g_api.findAll = g_api.methodFromName(objectClass, "FindObjectsOfType", 2);
		g_api.getName = g_api.methodFromName(objectClass, "get_name", 0);
		g_api.getActive = g_api.methodFromName(g_api.gameObjectClass, "get_activeInHierarchy", 0);
		g_api.setActive = g_api.methodFromName(g_api.gameObjectClass, "SetActive", 1);
		g_api.instanceId = g_api.methodFromName(g_api.gameObjectClass, "GetInstanceID", 0);
		if (!g_api.findAll || !g_api.getName || !g_api.getActive || !g_api.setActive || !g_api.instanceId) return false;
		g_cameraClass = g_api.classFromName(coreImage, "UnityEngine", "Camera");
		g_transformClass = g_api.classFromName(coreImage, "UnityEngine", "Transform");
		if (g_cameraClass && g_transformClass)
		{
			g_cameraMain = g_api.methodFromName(g_cameraClass, "get_main", 0);
			g_worldToScreen = g_api.methodFromName(g_cameraClass, "WorldToScreenPoint", 1);
			g_pixelWidth = g_api.methodFromName(g_cameraClass, "get_pixelWidth", 0);
			g_pixelHeight = g_api.methodFromName(g_cameraClass, "get_pixelHeight", 0);
			g_getTransform = g_api.methodFromName(g_api.gameObjectClass, "get_transform", 0);
			g_getPosition = g_api.methodFromName(g_transformClass, "get_position", 0);
			g_childCount = g_api.methodFromName(g_transformClass, "get_childCount", 0);
			g_parent = g_api.methodFromName(g_transformClass, "get_parent", 0);
			g_getGameObject = g_api.methodFromName(g_transformClass, "get_gameObject", 0);
		}
		g_api.gameObjectType = g_api.typeObject(g_api.classType(g_api.gameObjectClass));
		g_api.ready = g_api.gameObjectType != nullptr;
		Log("[SMM] api ready=%d findAll=%p", g_api.ready ? 1 : 0, g_api.findAll);
		return g_api.ready;
	}

	std::string GetName(void* object)
	{
		if (!object || !g_api.getName) return {};
		typedef void* (*GetNameFn)(void*);
		void* result = reinterpret_cast<GetNameFn>(*reinterpret_cast<void**>(g_api.getName))(object);
		if (!result) return {};
		const int length = *reinterpret_cast<int*>(static_cast<char*>(result) + 0x10);
		const wchar_t* chars = reinterpret_cast<const wchar_t*>(static_cast<char*>(result) + 0x14);
		if (length <= 0 || !chars) return {};
		const int bytes = WideCharToMultiByte(CP_UTF8, 0, chars, length, nullptr, 0, nullptr, nullptr);
		std::string text(static_cast<size_t>(bytes), '\0');
		WideCharToMultiByte(CP_UTF8, 0, chars, length, text.data(), bytes, nullptr, nullptr);
		return text;
	}

	bool GetActive(void* object)
	{
		if (!object || !g_api.getActive) return false;
		typedef bool (*GetActiveFn)(void*);
		return reinterpret_cast<GetActiveFn>(*reinterpret_cast<void**>(g_api.getActive))(object);
	}

	int GetChildCount(void* transform)
	{
		if (!transform || !g_childCount) return 0;
		typedef int (*ChildCountFn)(void*);
		return reinterpret_cast<ChildCountFn>(*reinterpret_cast<void**>(g_childCount))(transform);
	}

	std::string GetParentName(void* go)
	{
		if (!go || !g_getTransform || !g_parent || !g_getGameObject) return {};
		typedef void* (*GetTransformFn)(void*);
		typedef void* (*ParentFn)(void*);
		typedef void* (*GetGameObjectFn)(void*);
		void* tr = reinterpret_cast<GetTransformFn>(*reinterpret_cast<void**>(g_getTransform))(go);
		if (!tr) return {};
		void* parent = reinterpret_cast<ParentFn>(*reinterpret_cast<void**>(g_parent))(tr);
		if (!parent) return {};
		void* parentGo = reinterpret_cast<GetGameObjectFn>(*reinterpret_cast<void**>(g_getGameObject))(parent);
		return parentGo ? GetName(parentGo) : std::string();
	}

	int GetDepth(void* transform)
	{
		if (!transform || !g_parent) return 0;
		typedef void* (*ParentFn)(void*);
		auto parentFn = reinterpret_cast<ParentFn>(*reinterpret_cast<void**>(g_parent));
		int depth = 0;
		void* current = transform;
		while (depth < 32)
		{
			void* parent = parentFn(current);
			if (!parent) break;
			current = parent;
			++depth;
		}
		return depth;
	}

	int GetId(void* object)
	{
		if (!object || !g_api.instanceId) return 0;
		typedef int (*GetIdFn)(void*);
		return reinterpret_cast<GetIdFn>(*reinterpret_cast<void**>(g_api.instanceId))(object);
	}

	void SetActive(void* object, bool active)
	{
		if (!object || !g_api.setActive) return;
		typedef void (*SetActiveFn)(void*, bool);
		reinterpret_cast<SetActiveFn>(*reinterpret_cast<void**>(g_api.setActive))(object, active);
	}

	// -------------------------------------------------------------- classify
	std::string PropLabel(const std::string& prop)
	{
		if (prop == "binder") return "文件夹";
		if (prop == "boardpen") return "白板笔";
		if (prop == "dresscurtain") return "更衣帘";
		if (prop == "microphone") return "麦克风";
		if (prop == "microphonerecord") return "录音麦";
		if (prop == "mirror") return "镜子";
		if (prop == "indicatorstick") return "指示棒";
		if (prop == "smartphone") return "手机";
		return prop;
	}

	std::string StripDigits(const std::string& token)
	{
		size_t end = token.size();
		while (end > 0 && token[end - 1] >= '0' && token[end - 1] <= '9') --end;
		return token.substr(0, end);
	}

	std::string MapLookup(const std::string& token, const std::pair<const char*, const char*>* table, size_t count)
	{
		const std::string key = StripDigits(token);
		for (size_t i = 0; i < count; ++i)
			if (key == table[i].first) return table[i].second;
		for (size_t i = 0; i < count; ++i)
			if (key.rfind(table[i].first, 0) == 0) return table[i].second;
		return "";
	}

	std::string TranslateToken(const std::string& token)
	{
		static const std::pair<const char*, const char*> table[] = {
			{"drum", "鼓"}, {"cube", "方块"}, {"tinplate", "铁板"}, {"fence", "围栏"},
			{"curtain", "帘"}, {"door", "门"}, {"window", "窗"}, {"table", "桌"},
			{"chair", "椅"}, {"sofa", "沙发"}, {"shelf", "架"}, {"plant", "植物"},
			{"screen", "屏"}, {"floor", "地板"}, {"wall", "墙"}, {"ceiling", "天花板"},
			{"stage", "舞台"}, {"pole", "杆"}, {"box", "箱"}, {"barrel", "桶"},
			{"tire", "轮胎"}, {"cone", "锥"}, {"sign", "牌"}, {"light", "灯"},
			{"raillight", "轨道灯"}, {"camera", "相机"}, {"truss", "桁架"},
			{"cushion", "坐垫"}, {"book", "书"}, {"poster", "海报"}, {"towel", "毛巾"},
			{"basket", "篮"}, {"bag", "包"}, {"clock", "钟"}, {"mirror", "镜子"},
			{"normal", "普通"}, {"common", "通用"}, {"variant", "变体"},
			{"fin", "决赛"}, {"mid", "中间"}, {"nia", "N.I.A"}, {"hif", "H.I.F"},
			{"body", "身体"}, {"face", "脸"}, {"hair", "头发"},
			{"weed", "杂草"}, {"sweets", "甜点"}, {"file", "文件"}, {"snow", "雪"},
			{"musicstand", "谱架"}, {"tape", "胶带"}, {"seal", "封条"},
			{"brock", "砖"}, {"brick", "砖"}, {"toro", "灯笼"}, {"tent", "帐篷"},
			{"meal", "餐食"}, {"bodyc", "身体部件"}, {"l1", "灯"}
		};
		return MapLookup(token, table, sizeof(table) / sizeof(table[0]));
	}

	std::string TranslateCategory(const std::string& token)
	{
		static const std::pair<const char*, const char*> table[] = {
			{"furniture", "家具"}, {"plant", "植物"}, {"poster", "海报"}, {"book", "书"},
			{"electronics", "电器"}, {"homeobjects", "家居物件"}, {"stationary", "文具"},
			{"office", "办公用品"}, {"cosmetics", "化妆品"}, {"nature", "自然物"},
			{"roomaccessories", "房间装饰"}, {"sports", "运动器材"}, {"tool", "工具"},
			{"kitchenappliances", "厨房电器"}, {"food", "食物"}, {"tableware", "餐具"},
			{"container", "容器"}, {"sweets", "甜点"}, {"costume", "服装"},
			{"school", "学校用品"}, {"cardboard", "纸箱"}, {"rock", "岩石"},
			{"weed", "杂草"}, {"wood", "木材"}, {"cloth", "布料"}, {"share", "共享物件"},
			{"live", "演出"}, {"equipment", "设备"}, {"stg", "舞台"}, {"parts", "部件"},
			{"camera", "相机"}, {"lightwash", "洗墙灯"}, {"light", "灯光"}, {"truss", "桁架"}
		};
		return MapLookup(token, table, sizeof(table) / sizeof(table[0]));
	}

	std::string TranslatePlace(const std::string& token)
	{
		static const std::pair<const char*, const char*> table[] = {
			{"photobooth", "拍照棚"}, {"changingroom", "更衣室"}, {"office", "办公室"},
			{"classroom", "教室"}, {"corridor", "走廊"}, {"hall", "大厅"},
			{"studio", "摄影棚"}, {"dressroom", "更衣室"}, {"dormitory", "宿舍"},
			{"visuallessonroom", "视觉教室"}, {"city", "城市"}, {"garden", "花园"},
			{"road", "道路"}, {"live", "演出"}, {"stage", "舞台"}, {"school", "学校"},
			{"station", "车站"}, {"room", "房间"}, {"kitchen", "厨房"},
			{"equipment", "设备"}, {"camera", "相机"}, {"lightwash", "洗墙灯"},
			{"truss", "桁架"}, {"parts", "部件"}, {"stg", "舞台"}
		};
		return MapLookup(token, table, sizeof(table) / sizeof(table[0]));
	}

	std::string ModelLabel(const std::string& name)
	{
		std::string clean = name;
		const size_t variantPos = clean.find(" Variant");
		if (variantPos != std::string::npos) clean.resize(variantPos);
		if (clean.rfind("mdl_prp_", 0) == 0)
		{
			const std::string rest = clean.substr(8);
			const size_t us = rest.find('_');
			const std::string main = us == std::string::npos ? rest : rest.substr(0, us);
			std::string prop = main.substr(0, main.find('-'));
			std::string tail = main.find('-') == std::string::npos ? "" : main.substr(main.find('-') + 1);
			const size_t dash = tail.rfind('-');
			const std::string num = dash == std::string::npos ? "" : tail.substr(dash + 1);
			std::string label = PropLabel(prop);
			if (!num.empty()) label += " " + num;
			return label;
		}
		if (clean.rfind("mdl_env_cmn-parts_", 0) == 0)
		{
			const std::string rest = clean.substr(18);
			const size_t us = rest.find('_');
			const std::string main = us == std::string::npos ? rest : rest.substr(0, us);
			const std::string node = us == std::string::npos ? "" : rest.substr(us + 1);
			std::string parts[4];
			int count = 0;
			size_t pos = 0;
			while (count < 4)
			{
				const size_t dash = main.find('-', pos);
				parts[count++] = main.substr(pos, dash == std::string::npos ? std::string::npos : dash - pos);
				if (dash == std::string::npos) break;
				pos = dash + 1;
			}
			const std::string kind = count > 0 ? parts[0] : "";
			const std::string place = count > 1 ? parts[1] : "";
			const std::string num = count > 2 ? parts[2] : "";
			std::string label = TranslateCategory(kind);
			if (label.empty()) label = kind;
			const std::string placeLabel = TranslatePlace(place);
			if (!placeLabel.empty()) label += "·" + placeLabel;
			else if (!place.empty()) label += "·" + place;
			if (!num.empty()) label += " " + num;
			std::string nodeLabel = TranslateToken(node);
			if (nodeLabel.empty() && !node.empty()) nodeLabel = node;
			if (!nodeLabel.empty()) label += "·" + nodeLabel;
			return label;
		}
		if (clean.rfind("mdl_env_grp_", 0) == 0)
		{
			const std::string rest = clean.substr(12);
			const std::string stage = rest.substr(0, rest.find('-'));
			const size_t us = rest.find('_');
			const std::string variant = us == std::string::npos ? "" : rest.substr(us + 1);
			std::string label;
			if (stage == "audition") label = "试镜舞台";
			else if (stage == "produce") label = "培育演出";
			else if (stage == "cache") label = "场景缓存组";
			else label = "场景组";
			if (!variant.empty())
			{
				const size_t dash = variant.find('-');
				const std::string v = dash == std::string::npos ? variant : variant.substr(0, dash);
				const std::string vLabel = TranslateToken(v);
				if (!vLabel.empty()) label += "·" + vLabel;
			}
			return label;
		}
		if (clean.rfind("mdl_chr_", 0) == 0)
		{
			const std::string rest = clean.substr(8);
			const std::string part = rest.substr(rest.rfind('_') + 1);
			if (part == "body") return "人物·身体";
			if (part == "face") return "人物·脸";
			if (part == "hair") return "人物·头发";
			return "人物模型";
		}
		if (clean.rfind("fbx_", 0) == 0)
		{
			const std::string inner = clean.substr(4);
			if (inner.rfind("mdl_env_", 0) == 0)
			{
				const std::string label = ModelLabel(inner);
				if (!label.empty()) return label;
			}
			return "场景预制体";
		}
		return "";
	}

	std::string CategoryOf(const std::string& name)
	{
		if (name.rfind("mdl_env_cmn-parts_", 0) == 0)
		{
			const std::string rest = name.substr(18);
			std::string kind = rest.substr(0, rest.find('-'));
			if (kind == "furniture") kind = "家具";
			if (kind == "fence") kind = "围栏";
			return "场景公共部件/" + kind;
		}
		if (name.rfind("mdl_env_grp_", 0) == 0)
		{
			const std::string rest = name.substr(12);
			if (rest.rfind("audition", 0) == 0) return "场景组/试镜舞台";
			if (rest.rfind("produce", 0) == 0) return "场景组/培育演出";
			if (rest.rfind("cache", 0) == 0) return "场景组/缓存";
			return "场景组/其他";
		}
		if (name.rfind("mdl_prp_", 0) == 0)
		{
			const std::string rest = name.substr(8);
			return "道具/" + PropLabel(rest.substr(0, rest.find('-')));
		}
		if (name.rfind("mdl_chr_", 0) == 0) return "人物";
		std::string lower = name;
		for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		if (lower.find("light") != std::string::npos || lower.find("spot") != std::string::npos ||
			lower.find("rim") != std::string::npos || lower.find("ambient") != std::string::npos)
			return "光影";
		return "未分类";
	}

	struct Annotation { const char* category; const char* label; };

	Annotation LookupAnnotation(const std::string& name)
	{
		auto has = [&](const char* token) { return name.find(token) != std::string::npos; };
		if (has("ps_") || has("particle") || has("smoke") || has("fireball") || has("spark") || has("trail") || has("beam"))
			return {"特效", "粒子/特效"};
		if (has("flare") || has("glow") || has("lens") || has("spectrum"))
			return {"特效", "光晕/镜头效果"};
		if (has("baton") || has("penlight") || has("cheer"))
			return {"演出道具", "荧光棒/应援棒"};
		if (has("fence") || has("Geo_") || has("floor") || has("wall") || has("chair") || has("desk") || has("stage"))
			return {"场景几何", "场景几何体"};
		if (has("Hand") || has("Thumb") || has("Shoulder") || has("Arm") || has("Leg") || has("Foot") ||
			has("Head") || has("Neck") || has("Spine") || has("Chest") || has("Hip") || has("Toe") ||
			has("Index") || has("Middle") || has("Ring") || has("Pinky") || has("Jaw") || has("Eye") ||
			has("Brow") || has("Lip") || has("Root") || has("body") || has("Armature"))
			return {"骨骼", "骨骼/角色部件"};
		if (has("Light") || has("light") || has("Spot") || has("spot"))
			return {"灯光", "灯光"};
		if (has("Camera") || has("camera"))
			return {"相机", "相机"};
		if (has("Canvas") || has("Text") || has("Button") || has("Panel") || has("Image"))
			return {"UI", "界面元素"};
		return {"未识别", ""};
	}

	bool IsProtectedName(const std::string& name)
	{
		if (name.rfind("mdl_chr_", 0) == 0) return true;
		if (name.rfind("mdl_env_grp_", 0) == 0) return true;
		std::string lower = name;
		for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		static const char* tokens[] = {"actor", "idol", "camera", "light", "stage", "spot", "rim"};
		for (const char* token : tokens)
			if (lower.find(token) != std::string::npos) return true;
		return false;
	}

	// ---------------------------------------------------------------- state
	struct Item
	{
		int id = 0;
		std::string name;
		std::string category;
		std::string note;
		std::string parent;
		bool active = true;
		bool protectedNode = false;
	};

	std::mutex g_mutex;
	std::vector<Item> g_items;
	std::vector<std::pair<int, void*>> g_objects;
	std::vector<int> g_requestIds;
	bool g_requestActive = false;
	std::atomic<bool> g_forceScan{false};
	std::atomic<bool> g_ignoreProtection{false};
	std::atomic<bool> g_showAll{false};
	std::atomic<bool> g_chineseFirst{true};
	std::atomic<ULONGLONG> g_lastPaint{0};
	bool g_restoreAll = false;
	const GakuDesktopUiService* g_uiService = nullptr;
	std::chrono::steady_clock::time_point g_lastScan{};

	// -------------------------------------------------------------- callbacks
	GakuStatus Activate(const GakuHostApi* host, void** pluginState)
	{
		if (pluginState) *pluginState = nullptr;
		if (host && host->queryService)
		{
			static GakuDesktopUiService service{};
			if (host->queryService(host->hostContext, GAKU_SERVICE_DESKTOP_UI,
				GAKU_ABI_VERSION, sizeof(service), &service) == GAKU_OK)
				g_uiService = &service;
		}
		return GAKU_OK;
	}

	void RuntimeReady(void*)
	{
		Log("[SMM] runtime ready");
		LoadNameMap();
		InitializeApi();
	}

	void Tick(void*, float)
	{
		if (!InitializeApi()) return;
		static const auto started = std::chrono::steady_clock::now();
		const auto now = std::chrono::steady_clock::now();
		if (now - started < std::chrono::seconds(5)) return;

		const ULONGLONG paint = g_lastPaint.load(std::memory_order_relaxed);
		const bool panelVisible = paint != 0 && (GetTickCount64() - paint) < 800;
		const bool scanDue = g_forceScan.exchange(false) ||
			(panelVisible && now - g_lastScan >= std::chrono::milliseconds(1000));
		if (scanDue)
		{
			g_lastScan = now;
			typedef void* (*FindAllFn)(void*, bool);
			void* methodPointer = g_api.findAll ? *reinterpret_cast<void**>(g_api.findAll) : nullptr;
			if (!methodPointer) return;
			void* array = reinterpret_cast<FindAllFn>(methodPointer)(g_api.gameObjectType, true);
			if (!array) { Log("[SMM] FindObjectsOfType null"); return; }
			const uintptr_t count = g_api.arrayLength(array);
			void** raw = reinterpret_cast<void**>(static_cast<char*>(array) + 0x20);
			std::vector<Item> snapshot;
			std::vector<std::pair<int, void*>> objects;
			for (uintptr_t i = 0; i < count; ++i)
			{
				void* go = raw[i];
				if (!go) continue;
				const std::string name = GetName(go);
				if (name.empty()) continue;
				const bool isModel = name.rfind("mdl_", 0) == 0;
				const bool isLight = name.find("light") != std::string::npos || name.find("Light") != std::string::npos;
				const bool inOriginalList = isModel || isLight;
				if (!inOriginalList && !g_showAll.load(std::memory_order_relaxed)) continue;
				const int id = GetId(go);
				if (!id) continue;
				Item item;
				item.id = id;
				item.name = name;
				std::string baseName = name;
				const size_t variantSuffix = baseName.find(" Variant");
				if (variantSuffix != std::string::npos) baseName.resize(variantSuffix);
				auto mapped = g_nameMap.find(name);
				if (mapped == g_nameMap.end() && baseName != name) mapped = g_nameMap.find(baseName);
				if (inOriginalList)
				{
					item.category = CategoryOf(name);
					item.note = ModelLabel(name);
				}
				else if (mapped != g_nameMap.end())
				{
					item.category = std::string("列表外/") + mapped->second.first;
					item.note = mapped->second.second;
				}
				else
				{
					const Annotation annotation = LookupAnnotation(name);
					item.category = std::string("列表外/") + annotation.category;
					item.note = annotation.label;
				}
				item.active = GetActive(go);
				item.parent = GetParentName(go);
				int depth = 0;
				if (g_getTransform && g_parent)
				{
					typedef void* (*GetTransformFn)(void*);
					void* tr = reinterpret_cast<GetTransformFn>(*reinterpret_cast<void**>(g_getTransform))(go);
					depth = GetDepth(tr);
				}
				item.protectedNode = IsProtectedName(name) || depth <= 1 || !inOriginalList;
				snapshot.push_back(std::move(item));
				objects.emplace_back(id, go);
			}
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				g_items = std::move(snapshot);
				g_objects = std::move(objects);
			}
			static bool itemsLogged = false;
			if (!itemsLogged)
			{
				itemsLogged = true;
				size_t protectedCount = 0;
				for (const auto& item : g_items) if (item.protectedNode) ++protectedCount;
				Log("[SMM] items=%zu protected=%zu", g_items.size(), protectedCount);
			}
		}

		std::vector<int> requestIds;
		bool requestActive = false;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			requestIds = std::move(g_requestIds);
			g_requestIds.clear();
			requestActive = g_requestActive;
		}
		{
			// Right mouse sweep: select the object nearest the cursor by instance id.
			static POINT lastCursor{};
			static bool hasLast = false;
			POINT cursor{};
			GetCursorPos(&cursor);
			HWND foreground = GetForegroundWindow();
			RECT client{};
			POINT origin{};
			bool inside = false;
			if (foreground && GetClientRect(foreground, &client))
			{
				origin.x = 0; origin.y = 0;
				ClientToScreen(foreground, &origin);
				inside = cursor.x >= origin.x && cursor.x <= origin.x + client.right &&
					cursor.y >= origin.y && cursor.y <= origin.y + client.bottom;
			}
			const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
			if (inside && rightDown && g_cameraMain && g_worldToScreen && g_getTransform && g_getPosition)
			{
				const int moved = hasLast ? abs(cursor.x - lastCursor.x) + abs(cursor.y - lastCursor.y) : 100;
				if (moved > 2)
				{
					hasLast = true;
					lastCursor = cursor;
					std::vector<std::pair<int, void*>> sweepObjects;
					{
						std::lock_guard<std::mutex> lock(g_mutex);
						sweepObjects = g_objects;
					}
					typedef void* (*MainFn)();
					typedef Vec3 (*W2SFn)(void*, Vec3);
					typedef void* (*GetTransformFn)(void*);
					typedef Vec3 (*GetPositionFn)(void*);
					void* camera = reinterpret_cast<MainFn>(*reinterpret_cast<void**>(g_cameraMain))();
					if (camera)
					{
						double best = 1e9;
						int bestId = 0;
						for (const auto& entry : sweepObjects)
						{
							void* transform = reinterpret_cast<GetTransformFn>(*reinterpret_cast<void**>(g_getTransform))(entry.second);
							if (!transform) continue;
							const Vec3 world = reinterpret_cast<GetPositionFn>(*reinterpret_cast<void**>(g_getPosition))(transform);
							const Vec3 screen = reinterpret_cast<W2SFn>(*reinterpret_cast<void**>(g_worldToScreen))(camera, world);
							if (screen.z <= 0.0f) continue;
							const int sx = origin.x + static_cast<int>(screen.x);
							const int sy = origin.y + static_cast<int>(client.bottom - screen.y);
							const double dx = sx - cursor.x, dy = sy - cursor.y;
							const double distance = dx * dx + dy * dy;
							if (distance < best) { best = distance; bestId = entry.first; }
						}
						if (bestId && best < 60.0 * 60.0)
						{
							std::lock_guard<std::mutex> lock(g_selectedMutex);
							g_selected.insert(bestId);
						}
					}
				}
			}
			else
			{
				hasLast = false;
			}
		}
		if (requestIds.empty()) return;
		std::vector<std::pair<int, void*>> objects;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			objects = g_objects;
		}
		for (int id : requestIds)
		{
			bool protectedNode = false;
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				for (const auto& item : g_items) if (item.id == id) { protectedNode = item.protectedNode; break; }
			}
			if (protectedNode && !requestActive && !g_ignoreProtection.load(std::memory_order_relaxed))
			{
				Log("[SMM] protected skip id=%d", id);
				continue;
			}
			for (const auto& entry : objects)
			{
				if (entry.first != id) continue;
				SetActive(entry.second, requestActive);
				{
					std::lock_guard<std::mutex> lock(g_mutex);
					for (auto& item : g_items)
						if (item.id == id) item.active = requestActive;
				}
				break;
			}
		}
	}

	void DesktopUi(void*)
	{
		if (!g_uiService || !g_uiService->bind) return;
		g_lastPaint.store(GetTickCount64(), std::memory_order_relaxed);
		const GakuImGuiAbi abi{IMGUI_VERSION_NUM, sizeof(ImGuiIO), sizeof(ImGuiStyle),
			sizeof(ImDrawVert), sizeof(ImDrawIdx), sizeof(ImWchar)};
		GakuDesktopUiBinding binding{};
		if (g_uiService->bind(&abi, sizeof(binding), &binding) != GAKU_OK) return;
		if (!binding.imguiContext || !binding.alloc || !binding.free) return;
		ImGui::SetAllocatorFunctions(binding.alloc, binding.free, binding.allocatorUserData);
		ImGuiContext* previous = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(static_cast<ImGuiContext*>(binding.imguiContext));

		std::vector<Item> items;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			items = g_items;
		}
		std::set<int> selected;
		{
			std::lock_guard<std::mutex> lock(g_selectedMutex);
			selected = g_selected;
		}
		static char filter[128]{};
		ImGui::TextDisabled("场景模型管理  共 %zu 个对象", items.size());
		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputTextWithHint("##smm_filter", "搜索名称", filter, sizeof(filter));
		ImGui::SameLine();
		if (ImGui::Button("刷新")) g_forceScan.store(true);
		ImGui::SameLine();
		bool ignore = g_ignoreProtection.load(std::memory_order_relaxed);
		if (ImGui::Checkbox("无视风险", &ignore)) g_ignoreProtection.store(ignore, std::memory_order_relaxed);
		if (ignore)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "受保护节点可强制隐藏，可能导致游戏崩溃");
		}
		ImGui::SameLine();
		bool showAll = g_showAll.load(std::memory_order_relaxed);
		if (ImGui::Checkbox("显示全部对象", &showAll)) g_showAll.store(showAll, std::memory_order_relaxed);
		static bool onlyHidden = false;
		ImGui::SameLine();
		ImGui::Checkbox("只看已隐藏", &onlyHidden);
		ImGui::SameLine();
		bool chineseFirst = g_chineseFirst.load(std::memory_order_relaxed);
		if (ImGui::Checkbox("中文优先", &chineseFirst)) g_chineseFirst.store(chineseFirst, std::memory_order_relaxed);
		ImGui::SameLine();
		if (ImGui::Button("恢复全部"))
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			for (const auto& item : g_items) g_requestIds.push_back(item.id);
			g_requestActive = true;
		}
		ImGui::Separator();
		std::map<std::string, std::vector<const Item*>> groups;
		const std::string needle = filter;
		for (const auto& item : items)
		{
			if (!needle.empty() && item.name.find(needle) == std::string::npos) continue;
			if (onlyHidden && item.active) continue;
			groups[onlyHidden ? "已隐藏" : item.category].push_back(&item);
		}
		int shown = 0;
		for (const auto& group : groups) shown += static_cast<int>(group.second.size());
		ImGui::TextDisabled("当前筛选 %d 个，已选 %zu 个", shown, selected.size());
		if (ImGui::Button("全选当前"))
		{
			std::lock_guard<std::mutex> lock(g_selectedMutex);
			for (const auto& group : groups)
				for (const Item* item : group.second) g_selected.insert(item->id);
		}
		ImGui::SameLine();
		if (ImGui::Button("清空选择")) { std::lock_guard<std::mutex> lock(g_selectedMutex); g_selected.clear(); }
		ImGui::SameLine();
		if (ImGui::Button("隐藏选中"))
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			std::lock_guard<std::mutex> selLock(g_selectedMutex);
			for (int id : g_selected) g_requestIds.push_back(id);
			g_requestActive = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("恢复选中"))
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			std::lock_guard<std::mutex> selLock(g_selectedMutex);
			for (int id : g_selected) g_requestIds.push_back(id);
			g_requestActive = true;
		}
		ImGui::Separator();
		for (const auto& group : groups)
		{
			bool allSelected = !group.second.empty();
			for (const Item* item : group.second)
				if (!selected.contains(item->id)) { allSelected = false; break; }
			ImGui::PushID(group.first.c_str());
			if (ImGui::Checkbox("##group", &allSelected))
			{
				std::lock_guard<std::mutex> lock(g_selectedMutex);
				for (const Item* item : group.second)
				{
					if (allSelected) g_selected.insert(item->id);
					else g_selected.erase(item->id);
				}
			}
			ImGui::PopID();
			ImGui::SameLine();
			if (ImGui::CollapsingHeader(group.first.c_str()))
			{
				for (const Item* item : group.second)
				{
					ImGui::PushID(item->id);
					bool checked = selected.contains(item->id);
					if (ImGui::Checkbox("##sel", &checked))
					{
						std::lock_guard<std::mutex> lock(g_selectedMutex);
						if (checked) g_selected.insert(item->id);
						else g_selected.erase(item->id);
					}
					ImGui::SameLine();
					if (item->active) ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[显示]");
					else ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.5f, 1.0f), "[隐藏]");
					ImGui::SameLine();
					if (item->protectedNode) ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.3f, 1.0f), "[保护]");
					ImGui::SameLine();
					if (!item->note.empty() && g_chineseFirst)
					{
						ImGui::TextColored(ImVec4(0.65f, 0.85f, 1.0f, 1.0f), "%s", item->note.c_str());
						ImGui::SameLine();
						ImGui::TextDisabled("%s", item->name.c_str());
					}
					else
					{
						ImGui::TextUnformatted(item->name.c_str());
						if (!item->note.empty())
						{
							ImGui::SameLine();
							ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "（%s）", item->note.c_str());
						}
					}
					if (!item->parent.empty())
					{
						ImGui::SameLine();
						ImGui::TextDisabled("上级:%s", item->parent.c_str());
					}
					ImGui::SameLine();
					const bool blockHide = item->protectedNode && !g_ignoreProtection.load(std::memory_order_relaxed);
					if (blockHide) ImGui::BeginDisabled();
					if (ImGui::SmallButton(item->protectedNode ? "强制隐藏" : "隐藏"))
					{
						std::lock_guard<std::mutex> lock(g_mutex);
						g_requestIds.push_back(item->id);
						g_requestActive = false;
					}
					if (blockHide) ImGui::EndDisabled();
					ImGui::SameLine();
					if (ImGui::SmallButton("恢复"))
					{
						std::lock_guard<std::mutex> lock(g_mutex);
						g_requestIds.push_back(item->id);
						g_requestActive = true;
					}
					ImGui::PopID();
				}
			}
		}
		ImGui::SetCurrentContext(previous);
	}

	void Shutdown(void*) {}
}

extern "C" GAKU_PLUGIN_EXPORT GakuStatus GakuPlugin_Query(
	uint32_t requestedAbi, uint32_t outputSize, GakuPluginDescriptor* output)
{
	if (requestedAbi != GAKU_ABI_VERSION) return GAKU_E_VERSION;
	if (!output) return GAKU_E_INVALID_ARGUMENT;
	if (outputSize < sizeof(GakuPluginDescriptor)) return GAKU_E_SIZE;
	const GakuPluginDescriptor descriptor{GAKU_ABI_VERSION, sizeof(GakuPluginDescriptor),
		"gakusmm", "GakuSMM", "0.1.0",
		Activate, RuntimeReady, Tick, nullptr, DesktopUi, Shutdown};
	*output = descriptor;
	return GAKU_OK;
}
