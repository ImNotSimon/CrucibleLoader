#include "reflector.h"
#include "io/BinaryWriter.h"
#include <iostream>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <exception>
#include <set>

// Don't generate reflection functions for these struct names
// (Most likely reason is we're doing it manually for these types)
const std::set<std::string> HandcodedStructs = {
    "bool",
    "char",
    "unsigned char",
    "wchar_t",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "long long",
    "unsigned long",
    "unsigned long long",
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

enum TokenType {
    TT_End,
    TT_Colon,
    TT_Equals,
    TT_Comma,
    TT_BraceOpen,
    TT_BraceClose,
    TT_Semicolon,
    TT_Asterisk,

    TT_Comment,
    TT_ParenthBlock,
    TT_TemplateBlock,
    TT_BracketBlock,
    TT_Number,
    TT_Keyword,
};

struct ParsedToken {
    TokenType type;
    std::string_view data;
};

void GetFixedName(const std::string_view from, std::string& to) {
    to = std::string(from);

    for (char* c = to.data(), *max = to.data() + to.size(); c < max; c++) {
        switch (*c) {
            case ':': case ' ': case ',': case '*': case '-':
            *c = '_';
            break;

            case '<': case '>':
            *c = 'T';
            break;
        }
    }
}

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

class idlibTokenizer {
    public:
    char* text = nullptr;
    char* ch = nullptr;
    size_t length = 0;
    size_t currentLine = 1;

    std::string desheader = desHeaderStart;
    std::string descpp = desCppStart;

    ~idlibTokenizer() {
        delete[] text;
    }

    std::exception Error(const char* message) {
        std::cout << currentLine << ' ' << message;
        throw std::exception(message);
    }

    idlibTokenizer() {
        std::ifstream fstream = std::ifstream("idlib.h", std::ios_base::binary);
        if (!fstream.is_open()) {
            std::cout << "Could not open file";
            return;
        }

        fstream.seekg(0, std::ios_base::end);
        length = static_cast<size_t>(fstream.tellg());
        text = new char[length + 1]; // Room for a null char
        ch = text;
        fstream.seekg(0, std::ios_base::beg);
        fstream.read(text, length);
        fstream.close();
        text[length] = '\0';

        desheader.reserve(5000000);
        descpp.reserve(5000000);
    }

    void GenerateEnum(std::string_view comment, bool write) {
        // Tokenization
        ParsedEnum e;
        e.comment = comment;

        ParsedToken t = Tokenize();
        if(t.type != TT_Keyword)
            throw Error("Enum Name Expected");

        e.name = t.data;
        TokenAssert(TT_Colon);
        t = Tokenize();
        e.basetype = t.data;
        TokenAssert(TT_BraceOpen);
        
        while (true) {
            t = Tokenize();
            if(t.type == TT_BraceClose)
                break;

            ParsedEnumValue val;
            val.name = t.data;
            TokenAssert(TT_Equals);

            t = Tokenize();
            val.value = t.data;
            TokenAssert(TT_Comma);

            e.values.push_back(val);
        }
        TokenAssert(TT_Semicolon);

        if (!write)
            return;

        //if(!e.comment.empty())
        //    std::cout << e.comment << ' ' << e.basetype << ' ' << e.name << ' ' << e.values.size() << '\n';

        std::string fixedName = std::string(e.name);
        for (char* c = fixedName.data(), *max = fixedName.data() + fixedName.size(); c < max; c++) {
            if(*c == ':')
                *c = '_';
        }

        //std::cout << fixedName << '\n';

        desheader.append("\tvoid ds_");
        desheader.append(fixedName);
        desheader.append("(BinaryReader& reader, std::string& writeTo);\n");

        descpp.append("void deserial::ds_");
        descpp.append(fixedName);
        descpp.append("(BinaryReader& reader, std::string& writeTo) {\n");
        descpp.append("\tconst std::unordered_map<uint64_t, const char*> valueMap = {\n");

        int i = 0;
        for (ParsedEnumValue& val : e.values) {
            descpp.append("\t\t{");
            descpp.append(std::to_string(i++)); // TEMPORARY - until we get the hash codes
            descpp.append(", \"");
            descpp.append(val.name);
            descpp.append("\"},\n");
        }
        descpp.append("\t};\n\tds_enumbase(reader, writeTo, valueMap);\n");
        descpp.append("}\n");

    }

    int structCount = 0;
    int propCount = 0;
    int excludedStructs = 0;
    int excludedProps = 0;
    std::unordered_map<std::string, int> templateTypes;

    void ParseStruct(ParsedStruct& structData) {

    }

    void GenerateStruct(bool write) {
        ParsedStruct structData;
        std::vector<ParsedToken> tokens;

        /* Read Struct Name */
        do {
            tokens.push_back(Tokenize());
        }
        while (tokens.back().type != TT_BraceOpen && tokens.back().type != TT_Colon);

        {
            const char* start = tokens[0].data.data();
            ParsedToken& endToken = tokens[tokens.size() - 2];
            const char* end = endToken.data.data() + endToken.data.size();
            structData.name = std::string_view(start, end - start);
        }

        /* Read Struct Parent Type */
        if (tokens.back().type == TT_Colon) {
            tokens.clear();

            do {
                tokens.push_back(Tokenize());
            } while (tokens.back().type != TT_BraceOpen);

            {
                const char* start = tokens[0].data.data();
                ParsedToken& endToken = tokens[tokens.size() - 2];
                const char* end = endToken.data.data() + endToken.data.size();
                structData.parent = std::string_view(start, end - start);

                // For Counting Template Types
                //for (int i = 0; i < tokens.size() - 1; i++) {
                //    if (tokens[i].type == TT_TemplateBlock) {
                //        auto pair = templateTypes.find(std::string(tokens[0].data));
                //        if (pair != templateTypes.end()) {
                //            pair->second++;
                //        }
                //        else {
                //            templateTypes.emplace(tokens[0].data, 0);
                //        }
                //    }
                //}
            }
        }

        //std::cout << currentLine << " \"" << structData.name << "\" \"" << structData.parent << "\"\n";

        /* Read Struct Variables */
        while (true) {
            tokens.clear();

            tokens.push_back(Tokenize());
            if(tokens.back().type == TT_BraceClose)
                break;
            if(tokens.back().type == TT_Comment)
                continue;

            do {
                tokens.push_back(Tokenize());
            } while(tokens.back().type != TT_Semicolon);

            {
                ParsedStructValue val;
                val.name = tokens[tokens.size() - 2].data;

                const char* start = tokens[0].data.data();
                ParsedToken& endToken = tokens[tokens.size() - 3];
                const char* end = endToken.data.data() + endToken.data.size();
                val.type = std::string_view(start, end - start);

                structData.values.push_back(val);
                //std::cout << val.type.length() << " " << val.name.length() << "\n";
                //std::cout << "\t\"" << val.type << "\" \"" << val.name << "\"\n";
                propCount++;
            }
        }
        structCount++;
        TokenAssert(TT_Semicolon);

        /* Exclude hard-coded structs from reflection generation */
        if (HandcodedStructs.find(std::string(structData.name)) != HandcodedStructs.end()) {
            return;
        }            

        /* Begin In-Depth Analysis of Types */
        if (structData.name._Starts_with("idDecl")) {
            structData.exclude = true;
            excludedStructs++;
            excludedProps += structData.values.size();
        }

        
        if (!structData.exclude)
        {
            for (ParsedStructValue& val : structData.values) {

                // Function pointers - and some other bizarre stuff
                if (val.type.find('(') != std::string_view::npos) {
                    val.exclude = true;
                }


                if (val.exclude) {
                    excludedProps++;
                }
            }
        }



        /* Generate Reflection Code */
        if (!write)
            return;

        std::string fixedname;
        GetFixedName(structData.name, fixedname);
        desheader.append("\tvoid ds_");
        desheader.append(fixedname);
        desheader.append("(BinaryReader& reader, std::string& writeTo);\n");

        descpp.append("void deserial::ds_");
        descpp.append(fixedname);
        descpp.append("(BinaryReader& reader, std::string& writeTo) {\n");
        if (structData.exclude) {
            descpp.append("\t#ifdef _DEBUG\n\tassert(0);\n\t#endif\n");
        }
        descpp.append("}\n");
    }

    void Generate() {
        std::string_view lastComment = "";

        bool loop = true;
        while (loop) {
            ParsedToken token = Tokenize();
            switch (token.type) {
                case TT_Comment:
                lastComment = token.data;
                break;

                case TT_Keyword:
                if (token.data == "enum") {
                    GenerateEnum(lastComment, false);
                    lastComment = "";
                }

                else if (token.data == "struct") {
                    token = Tokenize();
                    if (token.data != "__cppobj") {
                        throw Error("__cppobj expected");
                    }
                    GenerateStruct(true);
                }

                else if (token.data == "typedef") {
                    loop = false; // For now
                    printf("Structs: %d / %d  Props: %d / %d", structCount, excludedStructs, propCount, excludedProps);
                    for (const auto& pair : templateTypes)
                        std::cout << pair.first << " " << pair.second << "\n";
                }
                break;

                default:
                return;
            }
        }

        desheader.push_back('}');
    }

    void TokenAssert(TokenType type) {
        ParsedToken t = Tokenize();
        if(t.type != type)
            throw Error("Token Assert Failed");
    }

    ParsedToken Tokenize() {
        // Faster than STL isalpha(char) and isdigit(char) functions
        #define isLetter (((unsigned int)(*ch | 32) - 97) < 26U)
        #define isNum (((unsigned)*ch - '0') < 10u)
        const char* first; // Ptr to start of current identifier/value token

        LABEL_TOKENIZE_START:
        switch (*ch) {

            case '\0':
            return {TT_End};

            case '=':
            ch++;
            return {TT_Equals};

            case ':':
            if(*(ch+1) == ':')
                goto LABEL_FORCE_DEFAULT;
            ch++;
            return {TT_Colon}; // Need this to properly form a type token

            case '*':
            ch++;
            return {TT_Asterisk, std::string_view(ch - 1, 1) };

            case ',':
            ch++;
            return {TT_Comma};

            case ';':
            ch++;
            return {TT_Semicolon};

            case '{':
            ch++;
            return {TT_BraceOpen};

            case '}':
            ch++;
            return {TT_BraceClose};

            case ' ': case '\t':
            ch++;
            goto LABEL_TOKENIZE_START;

            case '\r':
            if (*++ch != '\n')
                throw Error("Expected line feed after carriage return");
            case '\n':
            ch++;
            currentLine++;
            goto LABEL_TOKENIZE_START;

		    case '/':
		    first = ch;
		    if (*++ch == '/') {
			    LABEL_COMMENT_START:
			    switch (*++ch)
			    {
                    // Todo: Optimize to skip newline?
                    case '\r': case '\n': case '\0':
                    return {TT_Comment, std::string_view(first, (size_t)(ch - first))};

				    default:
				    goto LABEL_COMMENT_START;
			    }
		    }
		    else throw Error("Invalid Comment Syntax");

            case '(':
            {
                first = ch;
                int parenStack = 1;
                LABEL_PARENBLOCK_START:
                switch (*++ch)
                {
                    case '(':
                    parenStack++;
                    goto LABEL_PARENBLOCK_START;

                    case ')':
                    parenStack--;
                    if (parenStack == 0) {
                        return {TT_ParenthBlock, std::string_view(first, ++ch - first)};
                    }
                    goto LABEL_PARENBLOCK_START;

                    case '\r': case '\n': case '\0':
                    throw Error("No end to parenth block");

                    default:
                    goto LABEL_PARENBLOCK_START;
                }
            }

            case '[':
            {
                first = ch;
                int brackStack = 1;
                LABEL_BRACKBLOCK_START:
                switch (*++ch)
                {
                    case '[':
                    brackStack++;
                    goto LABEL_BRACKBLOCK_START;

                    case ']':
                    brackStack--;
                    if (brackStack == 0) {
                        return {TT_BracketBlock, std::string_view(first, ++ch - first)};
                    }
                    goto LABEL_BRACKBLOCK_START;

                    case '\r': case '\n': case '\0':
                    throw Error("No end to bracket block");

                    default:
                    goto LABEL_BRACKBLOCK_START;
                }
            }

            case '<':
            {
                first = ch;
                int tempStack = 1;
                LABEL_TEMPBLOCK_START:
                switch (*++ch)
                {
                    case '<':
                    if (*(ch + 1) == '<') { // Indiana Jones and the Template of Bitwise Bullshit
                        ch++;
                    }
                    else {
                        tempStack++;
                    }
                    goto LABEL_TEMPBLOCK_START;

                    case '>':
                    tempStack--;
                    if (tempStack == 0) {
                        return {TT_TemplateBlock, std::string_view(first, ++ch - first)};
                    }
                    goto LABEL_TEMPBLOCK_START;

                    case '\r': case '\n': case '\0':
                    throw Error("No end to template block");

                    default:
                    goto LABEL_TEMPBLOCK_START;
                }
            }

            break;


            case '-': //case '.':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            {
                bool hasDot = false;
                first = ch;
                LABEL_NUMBER_START:
                switch (*++ch)
                {
                    case '0': case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                    goto LABEL_NUMBER_START;

                    case '.':
                    if (hasDot)
                        throw Error("Decimal numbers can't have multiple periods.");
                    hasDot = true;
                    goto LABEL_NUMBER_START;

                    case 'e': case 'E': case'+': case '-':
                    goto LABEL_NUMBER_START;

                    default:
                    return {TT_Number, std::string_view(first, (size_t)(ch - first)) };
                }
            }

		    default:
		    {
		    	if (!isLetter && *ch != '_' && *ch != ':') {
		    		throw Error("Unrecognized character");
		    	}

                LABEL_FORCE_DEFAULT:
		    		
		    	first = ch;

		    	LABEL_ID_START:
		    	switch (*++ch)
		    	{
		    		case '[':
                    {
                        int brackStack = 1;
                        LABEL_ID_BRACKET_START:
                        switch (*++ch)
                        {
                            case '[':
                            brackStack++;
                            goto LABEL_ID_BRACKET_START;

                            case ']':
                            brackStack--;
                            if(brackStack == 0)
                                goto LABEL_ID_START;
                            else goto LABEL_ID_BRACKET_START;

                            default:
                            goto LABEL_ID_BRACKET_START;
                        }
                    }
		    		break;

		    		default:
		    		if (isLetter || isNum || *ch == '_' || *ch == ':')
		    			goto LABEL_ID_START;
		    		break;
		    	}
                return {TT_Keyword, std::string_view(first, (size_t)(ch - first))};
		    }
        }
    }

    void OutputFiles() {
        std::ofstream writer = std::ofstream("generated/deserialgenerated.h", std::ios_base::binary);
        writer.write(desheader.data(), desheader.length());
        writer.close();

        writer.open("generated/deserialgenerated.cpp", std::ios_base::binary);
        writer.write(descpp.data(), descpp.length());
        writer.close();
    }
};

void idlibReflector::Generate() {
    idlibTokenizer tokenizer;
    if(!tokenizer.text)
        return;

    tokenizer.Generate();
    tokenizer.OutputFiles();
}
