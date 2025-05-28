#include "reflector.h"
#include "entityslayer/EntityParser.h"
#include "io/BinaryWriter.h"
#include <set>
#include <unordered_map>

// Don't generate reflection functions for these struct names
// (Most likely reason is we're doing it manually for these types)
const std::set<std::string> HandcodedStructs = {
    "bool",
    "char",
    "unsigned_char",
    "wchar_t",
    "short",
    "unsigned_short",
    "int",
    "unsigned_int",
    "long",
    "long_long",
    "unsigned_long",
    "unsigned_long_long",
    "float",
    "double"
};

// Instead of generating unique reflection functions, key structs will use
// the value struct's reflection functions
const std::unordered_map<std::string, const char*> AliasStructs = {

};


struct ParsedEnumValue {
    std::string_view name;
    std::string_view value;
};

struct ParsedEnum {
    std::string_view comment;
    std::string_view name;
    std::string_view basetype;
    std::vector<ParsedEnumValue> values;
};

struct ParsedStructValue {
    std::string_view type;
    std::string_view name;
    bool exclude = false;
    bool isPointer = false;
};

struct ParsedStruct {
    std::string_view name = "";
    std::string_view parent = "";
    std::vector<ParsedStructValue> values;
    bool exclude = false;
};

struct ParsedIdlib {
    std::vector<ParsedEnum> enums;
};

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
    std::string desheader = desHeaderStart;
    std::string descpp = desCppStart;

    idlibReflector() {

        desheader.reserve(5000000);
        descpp.reserve(5000000);
    }

    void GenerateEnums(EntNode& enums) {

        EntNode** enumArray = enums.getChildBuffer();
        int enumCount = enums.getChildCount();

        for (EntNode** iter = enumArray, **iterMax = enumArray + enumCount; iter < iterMax; iter++) {
            EntNode& current = **iter;

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

            int temp = 0; // TEMPORARY - until we get the hash codes
            for (EntNode** valIter = valueArray, **valMax = valueArray + valueCount; valIter < valMax; valIter++) {
                EntNode& v = **valIter;

                descpp.append("\t\t{");
                descpp.append(std::to_string(temp++)); 
                descpp.append(", \"");
                descpp.append(v.getName());
                descpp.append("\"},\n");
            }

            descpp.append("\t};\n\tds_enumbase(reader, writeTo, valueMap);\n");
            descpp.append("}\n");
        }
    }

    void GenerateStruct(EntNode& structs) {

        EntNode** structArray = structs.getChildBuffer();
        int structCount = structs.getChildCount();

        for (EntNode** structIter = structArray, **structMax = structArray + structCount; structIter < structMax; structIter++)
        {
            EntNode& current = **structIter;
            if (HandcodedStructs.find(std::string(current.getName())) != HandcodedStructs.end()) {
                continue;
            }

            desheader.append("\tvoid ds_");
            desheader.append(current.getName());
            desheader.append("(BinaryReader& reader, std::string& writeTo);\n");
            
            descpp.append("void deserial::ds_");
            descpp.append(current.getName());
            descpp.append("(BinaryReader& reader, std::string& writeTo) {\n");
            descpp.append("\tconst std::unordered_map<uint64_t, deserializer> propMap = {\n");


            EntNode& values = current["values"];
            EntNode** valueArray = values.getChildBuffer();
            int valueCount = values.getChildCount();

            int temp = 0;
            for (EntNode** valIter = valueArray, **valMax = valueArray + valueCount; valIter < valMax; valIter++) {
                EntNode& v = **valIter;

                if(&v["EDIT"] == EntNode::SEARCH_404 && &v["DESIGN"] == EntNode::SEARCH_404 && &v["DEF"] == EntNode::SEARCH_404)
                    continue;

                descpp.append("\t\t{");
                descpp.append(std::to_string(temp++));
                descpp.append(", {&ds_");
                descpp.append(v.getName());
                descpp.append("}},\n");
            }

            descpp.append("\t};\n\tds_structbase(reader, writeTo, propMap);\n");
            descpp.append("}\n");
        }

        ///* Exclude hard-coded structs from reflection generation */
        //if (HandcodedStructs.find(std::string(structData.name)) != HandcodedStructs.end()) {
        //    return;
        //}            

        ///* Begin In-Depth Analysis of Types */
        //if (structData.name._Starts_with("idDecl")) {
        //    structData.exclude = true;
        //}



        ///* Generate Reflection Code */



        //if (structData.exclude) {
        //    descpp.append("\t#ifdef _DEBUG\n\tassert(0);\n\t#endif\n");
        //}
        
    }

    void Generate(EntNode& root) {
        GenerateEnums(root["enums"]);
        GenerateStruct(root["structs"]);
        GenerateStruct(root["templatesubs"]);

        EntNode& templates = root["templates"];
        for (int i = 0, max = templates.getChildCount(); i < max; i++) {
            GenerateStruct(*templates.ChildAt(i));
        }
    }

    void OutputFiles() {
        desheader.push_back('}');

        std::ofstream writer = std::ofstream("generated/deserialgenerated.h", std::ios_base::binary);
        writer.write(desheader.data(), desheader.length());
        writer.close();

        writer.open("generated/deserialgenerated.cpp", std::ios_base::binary);
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
