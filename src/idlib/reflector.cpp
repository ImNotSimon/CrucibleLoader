#include "reflector.h"
#include "entityslayer/EntityParser.h"
#include "hash/HashLib.h"
#include <fstream>
#include <set>
#include <unordered_map>
#include <cassert>

const char* desHeaderStart =
R"(#include <string>
class BinaryReader;

namespace deserial {
)";

const char* desCppStart =
R"(#include "deserialgenerated.h"
#include "idlib/deserialcore.h"
#include "io/BinaryReader.h"
#include <cassert>

)";

class idlibReflector {
    public:
    // 
    const std::unordered_map<std::string, void(idlibReflector::*)(EntNode&)> SpecialTemplates = {
        {"idList", &idlibReflector::GenerateidList},
        {"idListBase", &idlibReflector::GenerateidListBase},
        {"idStaticList", &idlibReflector::GenerateidStaticList},
        {"idListMap", &idlibReflector::GenerateidListMap },
        {"idTypeInfoPtr", &idlibReflector::GenerateidTypeInfoPtr}
    };


    std::unordered_map<std::string, EntNode*> typelib;
    std::string desheader = desHeaderStart;
    std::string descpp = desCppStart;

    idlibReflector() {

        desheader.reserve(1000000);
        descpp.reserve(8000000);
    }

    void GenerateEnums(EntNode& enums) {

        EntNode** enumArray = enums.getChildBuffer();
        int enumCount = enums.getChildCount();

        for (EntNode** iter = enumArray, **iterMax = enumArray + enumCount; iter < iterMax; iter++) {
            EntNode& current = **iter;

            if(&current["INCLUDE"] == EntNode::SEARCH_404)
                continue;

            desheader.append("\tvoid ds_");
            desheader.append(current.getName());
            desheader.append("(BinaryReader& reader, std::string& writeTo);\n");

            descpp.append("void deserial::ds_");
            descpp.append(current.getName());
            descpp.append("(BinaryReader& reader, std::string& writeTo) {\n");
            descpp.append("\tconst std::unordered_map<uint64_t, const char*> valueMap = {\n");

            EntNode& values = current["values"];
            EntNode** valueArray = values.getChildBuffer();
            int valueCount = values.getChildCount();

            for (EntNode** valIter = valueArray, **valMax = valueArray + valueCount; valIter < valMax; valIter++) {
                EntNode& v = **valIter;
                std::string_view vName = v.getName();
                uint64_t hash = HashLib::FarmHash64(vName.data(), vName.length());

                descpp.append("\t\t{");
                descpp.append(std::to_string(hash)); 
                descpp.append("UL, \"");
                descpp.append(v.getName());
                descpp.append("\"},\n");
            }

            descpp.append("\t};\n\tds_enumbase(reader, writeTo, valueMap);\n");
            descpp.append("}\n");
        }
    }

    void WritePointerFunc(std::string_view typeName) {
        auto iter = typelib.find(std::string(typeName));
        assert(iter != typelib.end());

        EntNode& pointerfunc = (*iter->second)["pointerfunc"];
        if (&pointerfunc == EntNode::SEARCH_404) {
            descpp.append("pointerbase");
        }
        else {
            descpp.append(pointerfunc.getValue());
        }
    }

    void PopulateStructMap(EntNode& typeNode) {
        EntNode& values = typeNode["values"];
        EntNode** valueArray = values.getChildBuffer();
        int valueCount = values.getChildCount();

        for (EntNode** valIter = valueArray, **valMax = valueArray + valueCount; valIter < valMax; valIter++) {
            EntNode& v = **valIter;

            if (&v["INCLUDE"] == EntNode::SEARCH_404)
                continue;

            uint64_t hash = HashLib::FarmHash64(v.getValue().data(), v.getValue().length());

            descpp.append("\t\t{");
            descpp.append(std::to_string(hash));
            descpp.append(", {&ds_");

            /* If a pointer, map to the appropriate pointer function */
            EntNode& pointers = v["pointers"];
            if (&pointers == EntNode::SEARCH_404) {
                descpp.append(v.getName());
            } 
            else {
                // Double pointer variables in idLists technically aren't flagged for inclusion
                // and will be handled differently.
                std::string_view ptrCount = pointers.getValue();
                assert(ptrCount.length() == 1 && ptrCount[0] == '1');

                WritePointerFunc(v.getName());
            }

            descpp.append(", \"");
            descpp.append(v.getValue());
            descpp.append("\"");

            if (&v["array"] != EntNode::SEARCH_404) {
                descpp.append(", ");
                descpp.append(v["array"].getValue());
            }


            descpp.append("}},\n");
        }

        EntNode& parentName = typeNode["parentName"];
        if (&parentName != EntNode::SEARCH_404) {
            auto iter = typelib.find(std::string(parentName.getValue()));
            assert(iter != typelib.end());
            EntNode* parentType = iter->second;

            PopulateStructMap(*parentType);
        }
    }

    // bodyFunction is used for special template types that we need to generate non-standard
    // reflection code for (like idLists)
    void GenerateStruct(EntNode& structs, void(idlibReflector::*bodyFunction)(EntNode&) = nullptr) {

        EntNode** structArray = structs.getChildBuffer();
        int structCount = structs.getChildCount();

        for (EntNode** structIter = structArray, **structMax = structArray + structCount; structIter < structMax; structIter++)
        {
            EntNode& current = **structIter;

            if(&current["INCLUDE"] == EntNode::SEARCH_404)
                continue;

            desheader.append("\tvoid ds_");
            desheader.append(current.getName());
            desheader.append("(BinaryReader& reader, std::string& writeTo);\n");
            
            descpp.append("void deserial::ds_");
            descpp.append(current.getName());
            descpp.append("(BinaryReader& reader, std::string& writeTo) {\n");

            if (bodyFunction == nullptr) {
                EntNode& alias = current["alias"];

                if (&alias == EntNode::SEARCH_404) {
                    descpp.append("\tconst std::unordered_map<uint64_t, deserializer> propMap = {\n");
                    PopulateStructMap(current);
                    descpp.append("\t};\n\tds_structbase(reader, writeTo, propMap);\n");
                }
                else {
                    descpp.append("\tds_");
                    descpp.append(alias.getValue());
                    descpp.append("(reader, writeTo);\n");
                }

            }
            else {
                (this->*bodyFunction)(current);
            }

            descpp.append("}\n");
        }
        ///* Generate Reflection Code */

        //if (structData.exclude) {
        //    descpp.append("\t#ifdef _DEBUG\n\tassert(0);\n\t#endif\n");
        //}
    }
    
    void GenerateidList(EntNode& typenode) {
        // Get the parent type
        EntNode& parentName = typenode["parentName"];
        auto iter = typelib.find(std::string(parentName.getValue()));
        assert(iter != typelib.end());
        EntNode& parentnode = *iter->second;

        // The first value in idListBase is what the list stores
        EntNode& listType = *parentnode["values"].ChildAt(0);
        assert(&listType != EntNode::SEARCH_404);
        
        bool usePointerFunc;
        {
            std::string_view pointerCount = listType["pointers"].getValue();
            assert(pointerCount.length() == 1 && (pointerCount[0] == '1' || pointerCount[0] == '2'));
            
            usePointerFunc = pointerCount[0] == '2';
        }

        descpp.append("\tds_idList(reader, writeTo, &ds_");
        
        if (usePointerFunc) {
            WritePointerFunc(listType.getName());
        }
        else {
            descpp.append(listType.getName());
        }
        descpp.append(");\n");

        //printf("%.*s\n", (int)listType.getName().length(), listType.getName().data());   
    }

    void GenerateidListBase(EntNode& typenode) {
        // idListBase should never actually be included, only idList
        assert(0);
    }

    void GenerateidStaticList(EntNode& typenode) {
        // The first value in idListBase is what the list stores
        EntNode& listType = *typenode["values"].ChildAt(0);
        assert(listType.getValue() == "staticList");

        bool usePointerFunc;
        {
            // Since it's a static array, there may not be a pointer
            std::string_view pointerCount = listType["pointers"].getValue();
            assert(pointerCount.empty() || pointerCount[0] == '1');
            usePointerFunc = !pointerCount.empty();
        }

        descpp.append("\tds_idList(reader, writeTo, &ds_");

        if (usePointerFunc) {
            WritePointerFunc(listType.getName());
        }
        else {
            descpp.append(listType.getName());
        }
        descpp.append(");\n");
    }

    void GenerateidListMap(EntNode& typenode) {
        // We need to get the functions for the key and value types
        EntNode& keyval = *typenode["values"].ChildAt(0);
        EntNode& valueval = *typenode["values"].ChildAt(1);

        descpp.append("\tds_idListMap(reader, writeTo, &ds_");

        // Get key function
        {
            auto iter = typelib.find(std::string(keyval.getName()));
            assert(iter != typelib.end());

            // Get the idListBase
            iter = typelib.find(std::string((*iter->second)["parentName"].getValue()));
            assert(iter != typelib.end());

            EntNode& keylist = *(*iter->second)["values"].ChildAt(0);
            assert(keylist.getValue() == "list");
            assert(&keylist["pointers"] != EntNode::SEARCH_404);
            if (keylist["pointers"].getValue()[0] == '2')
                WritePointerFunc(keylist.getName());
            else descpp.append(keylist.getName());
        }

        

        // Get value function
        descpp.append(", &ds_");
        {
            auto iter = typelib.find(std::string(valueval.getName()));
            assert(iter != typelib.end());

            // Get the idListBase
            iter = typelib.find(std::string((*iter->second)["parentName"].getValue()));
            assert(iter != typelib.end());

            EntNode& valuelist = *(*iter->second)["values"].ChildAt(0);
            assert(valuelist.getValue() == "list");
            assert(&valuelist["pointers"] != EntNode::SEARCH_404);
            if (valuelist["pointers"].getValue()[0] == '2')
                WritePointerFunc(valuelist.getName());
            else descpp.append(valuelist.getName());
        }

        descpp.append(");\n");
    }

    void GenerateidTypeInfoPtr(EntNode& typenode) {
        descpp.append("\tds_idTypeInfoPtr(reader, writeTo);\n");
    }


    void AddTypeMap(EntNode& typelist) {
        for (int i = 0, max = typelist.getChildCount(); i < max; i++) {
            EntNode* n = typelist.ChildAt(i);
            typelib.emplace(n->getName(), n);
        }
    }

    void GenerateHashMaps() {
        descpp.append("const std::unordered_map<uint32_t, deserialTypeInfo> deserial::typeInfoPtrMap = {\n");

        for (const auto& pair : typelib) {
            EntNode& node = *pair.second;
            if(&node["INCLUDE"] == EntNode::SEARCH_404)
                continue;

            EntNode& hashNode = node["hash"];
            assert(&hashNode != EntNode::SEARCH_404);
            descpp.append("\t{");
            descpp.append(hashNode.getValueUQ());
            descpp.append("U, { &ds_");
            descpp.append(pair.first);
            descpp.append(", \"");
            descpp.append(pair.first);
            descpp.append("\"}},\n");
        }

        descpp.append("};\n");
    }

    void Generate(EntNode& root) {
        EntNode& enums = root["enums"];
        EntNode& structs = root["structs"];
        EntNode& templatesubs = root["templatesubs"];
        EntNode& templates = root["templates"];

        assert(&enums != EntNode::SEARCH_404);
        assert(&structs != EntNode::SEARCH_404);
        assert(&templatesubs != EntNode::SEARCH_404);
        assert(&templates != EntNode::SEARCH_404);

        /* Build Type Lib */
        typelib.reserve(25000);
        AddTypeMap(enums);
        AddTypeMap(structs);
        AddTypeMap(templatesubs);
        for (int i = 0, max = templates.getChildCount(); i < max; i++) {
            AddTypeMap(*templates.ChildAt(i));
        }

        GenerateHashMaps();
        GenerateEnums(enums);
        GenerateStruct(structs);
        GenerateStruct(templatesubs);
        for (int i = 0, max = templates.getChildCount(); i < max; i++) {

            EntNode* t = templates.ChildAt(i);
            auto iter = SpecialTemplates.find(std::string(t->getName()));
            if (iter == SpecialTemplates.end()) {
                GenerateStruct(*t);
            }
            else {
                GenerateStruct(*t, iter->second);
            }
            
        }
    }

    void OutputFiles() {
        desheader.push_back('}');

        std::ofstream writer = std::ofstream("src/idlib/generated/deserialgenerated.h", std::ios_base::binary);
        writer.write(desheader.data(), desheader.length());
        writer.close();

        writer.open("src/idlib/generated/deserialgenerated.cpp", std::ios_base::binary);
        writer.write(descpp.data(), descpp.length());
        writer.close();
    }
};

void idlibReflection::Generate() {

    printf("Engaging idlib Reflector shields\n");
    EntityParser parser = EntityParser("input/idlibcleaned.txt", ParsingMode::PERMISSIVE);
    EntNode* root = parser.getRoot();

    
    idlibReflector reflector;
    reflector.Generate(*root);
    reflector.OutputFiles();
}
