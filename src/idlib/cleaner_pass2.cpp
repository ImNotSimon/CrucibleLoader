#include "cleaner.h"
#include "entityslayer/EntityParser.h"
#include <unordered_map>
#include <string>
#include <cassert>

struct TypeMap {
	EntNode* node = nullptr;
	int referenceCount = 0;
	bool IsEntity = false;

	TypeMap() {}
	TypeMap(EntNode* n) : node(n) {}
};

class idlibCleaner2 
{
	private:
	EntityParser parser = EntityParser("input/idlibcleaned_pass1.txt", ParsingMode::PERMISSIVE);
	std::unordered_map<std::string, TypeMap> typelib;

	public:

	void AddTypeMap(EntNode& typelist);
	void CountReferences(EntNode& typelist);
	bool IsEntity(std::string_view type);

	void Build();
	void Output();
};

void idlibCleaner2::AddTypeMap(EntNode& typelist) {
	for (int i = 0, max = typelist.getChildCount(); i < max; i++) {
		EntNode* n = typelist.ChildAt(i);
		typelib.emplace(n->getName(), n);
	}
}

bool idlibCleaner2::IsEntity(std::string_view type) {
	auto pair = typelib[std::string(type)];
	
	EntNode* parent = &(*pair.node)["parentName"];
	while (parent != EntNode::SEARCH_404) {
		std::string_view parentString = parent->getValue();
		if(parentString == "idEngineEntity")
			return true;
			
		
		auto parentPair = typelib.find(std::string(parentString));

		// TODO - Clean these?
		// Unknown Parent - A few still make it in
		if (parentPair == typelib.end()) {
			//printf("UNKNOWN PARENT: %s\n", std::string(parentString).c_str());
			return false;
		}
		else {
			parent = &(*parentPair->second.node)["parentName"];
		}
	}
	return false;
}

void idlibCleaner2::CountReferences(EntNode& typelist)
{
	for (int i = 0, max = typelist.getChildCount(); i < max; i++) {
		EntNode& type = *typelist.ChildAt(i);
		EntNode& valueList = type["values"];

		for (int k = 0; k < valueList.getChildCount(); k++) {
			EntNode& value = *valueList.ChildAt(k);

			if(&value["INCLUDE"] == EntNode::SEARCH_404)
				continue;

			// Need to check for types not defined in the idlib
			auto pair = typelib.find(std::string(value.getName()));
			if (pair != typelib.end()) {
				pair->second.referenceCount++;
			}
		}
	}
}

/*
* Plan B:
* - Build up an inheritance hierarchy of all idEngineEntity classes
* - Recursively scan through these classes and mark all non-pointer variable types for inclusion
*	- Don't need to scan through parents if we're inlining parent properties into child maps
* - May need to add special exemption for idList variables
* 
* TODO: Replace Def/design/edit with a single include tag
*/

void idlibCleaner2::Build() {

	EntNode& root = *parser.getRoot();
	EntNode& enums = root["enums"];
	EntNode& structs = root["structs"];
	EntNode& templatesubs = root["templatesubs"];
	EntNode& templates = root["templates"];

	assert(&enums != EntNode::SEARCH_404);
	assert(&structs != EntNode::SEARCH_404);
	assert(&templatesubs != EntNode::SEARCH_404);
	assert(&templates != EntNode::SEARCH_404);



	printf("Building type map\n");
	typelib.reserve(25000);
	AddTypeMap(enums);
	AddTypeMap(structs);
	AddTypeMap(templatesubs);
	for (int i = 0, max = templates.getChildCount(); i < max; i++) {
		AddTypeMap(*templates.ChildAt(i));
	}

	printf("Determining Entities");
	for (auto& pair : typelib) {
		pair.second.IsEntity = IsEntity(pair.first);
	}

	printf("Counting references \n"); // Do NOT count enums here
	CountReferences(structs);
	CountReferences(templatesubs);
	for (int i = 0, max = templates.getChildCount(); i < max; i++) {
		CountReferences(*templates.ChildAt(i));
	}

	// TODO FOR ENTITYSLAYER: This really exposes how horrible findPositionalId is - need to refactor
	// it and the history system that uses it

	// TODO: This is too overzealous - it's excluding entity classes
	// Seems better now after manually including entity classes - continue to monitor
	printf("Adding Include Tags\n");
	int includeCount = 0;
	for (auto& pair : typelib)
	{
		if(pair.second.referenceCount == 0 && !pair.second.IsEntity)
			continue;
		
		includeCount++;
		parser.EditTree("INCLUDE", pair.second.node, 0, 0, false, false);
		parser.PushGroupCommand();
	}
	printf("%d types out of %zu should be included \n", includeCount, typelib.size());
}

void idlibCleaner2::Output() {
	parser.WriteToFile("input/idlibcleaned.txt", false);
}

void idlibCleaning::Pass2()
{
	printf("Cleaning Phase 2\n");

	idlibCleaner2 cleaner;
	cleaner.Build();
	cleaner.Output();
}