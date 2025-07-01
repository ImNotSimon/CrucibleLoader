#pragma once

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;


struct ResourceHeader {
    char      magic[4];                    
    uint32_t  version;					 
    uint32_t  flags;					 
    uint32_t  numSegments;				 
    uint64_t  segmentSize;				 
    uint64_t  metadataHash;				 
    uint32_t  numResources;				 
    uint32_t  numDependencies;			 
    uint32_t  numDepIndices;			 
    uint32_t  numStringIndices;			 
    uint32_t  numSpecialHashes;			 
    uint32_t  numMetaEntries;			 
    uint32_t  stringTableSize;			 
    uint32_t  metaEntriesSize;			 
    uint64_t  stringTableOffset;		 
    uint64_t  metaEntriesOffset;		 
    uint64_t  resourceEntriesOffset;	 
    uint64_t  resourceDepsOffset;		 
    uint64_t  resourceSpecialHashOffset; 
    uint64_t  dataOffset;
};

struct ResourceHeader_Eternal : public ResourceHeader {
	uint32_t unknown;
	uint64_t metaSize; // 4 bytes of unused space
};

struct ResourceEntry
{
	int64_t   resourceTypeString;
	int64_t   nameString;
	int64_t   descString;
	uint64_t  depIndices;
	uint64_t  strings;
	uint64_t  specialHashes;
	uint64_t  metaEntries;
	uint64_t  dataOffset; // Relative to beginning of archive
	uint64_t  dataSize; // Not null-terminated

	uint64_t  uncompressedSize;
	uint64_t  dataCheckSum;
	uint64_t  generationTimeStamp;
	uint64_t  defaultHash;
	uint32_t  version;
	uint32_t  flags;
	uint8_t   compMode;
	uint8_t   reserved0;
	uint16_t  variation;
	uint32_t  reserved2;
	uint64_t  reservedForVariations;

	uint16_t  numStrings;
	uint16_t  numSources;
	uint16_t  numDependencies;
	uint16_t  numSpecialHashes;
	uint16_t  numMetaEntries;
    uint8_t   padding[6];
};

struct StringChunk {
	uint64_t numStrings;
	uint64_t* offsets = nullptr;   // numStrings - Relative to byte after the offset list
	const char** values = nullptr; // numStrings

	~StringChunk() {
		delete[] offsets;
		delete[] values;
	}
};

struct ResourceDependency {
	uint64_t type;
	uint64_t name;
	uint32_t depType;
	uint32_t depSubType;
	uint64_t hashOrTimestamp;
};

struct ResourceArchive {
	ResourceHeader header;

	//FSeek(header.resourceEntriesOffset);
	ResourceEntry* entries = nullptr; // header.numResources

	//FSeek(header.stringTableOffset);
	StringChunk stringChunk;

	//FSeek(header.resourceDepsOffset);
	ResourceDependency* dependencies = nullptr; // header.numDependencies
	uint32_t* dependencyIndex = nullptr; // header.numDepIndices
	uint64_t* stringIndex = nullptr; // header.numStringIndices

	~ResourceArchive() {
		delete[] entries;
		delete[] dependencies;
		delete[] dependencyIndex;
		delete[] stringIndex;
	}
};