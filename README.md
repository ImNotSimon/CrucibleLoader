
# Atlan Modding Tools
A collection of modding tools for id Software's DOOM: The Dark Ages. Official releases and explanations for every available tool can be found on the [Releases Page](https://github.com/FlavorfulGecko5/EntityAtlan/releases)

## Atlan Mod Loader
An entirely new mod loader with vast improvements over the DOOM Eternal mod loader. Available on the [https://github.com/FlavorfulGecko5/EntityAtlan/releases]()

## Atlan Resource Extractor

A limited resource extractor intended for files that control game logic. Available on the [Releases Page](https://github.com/FlavorfulGecko5/EntityAtlan/releases/tag/Extractor)

## EntityAtlan

### This tool is a work-in-progress. No release build is published yet.

EntityAtlan is a tool for deserializing and reserializing DOOM: The Dark Ages entity files. Unlike in DOOM Eternal, these files are pre-serialized into a binary format. For level modding to be achievable, it's critical that a tool exists to decode these files, and re-encode our modded ones.

To accomplish this goal, EntityAtlan is a 2-step program.
* Step 1: Parse the Runtime Type Information (RTTI) dumped from DOOM: The Dark Ages' executable file to generate reflection code.
* Step 2: Use this reflection code to deserialize and reserialize entity files.

As a modder and normal user of EntityAtlan, you won't need to worry about Step 1.

## Credits
* FlavorfulGecko5 - Author of the Atlan Modding Tools
* Proteh - Author of [DarkAgesPatcher](https://github.com/dcealopez/DarkAgesPatcher) and DarkAgesModManager, which both ship with Atlan Mod Loader
* jandk / tjoener - Extracting RTTI data from the game executable. Provided additional help with file formats, hashing algorithms, and other challenges. Author of idTech resource extractor [Valen](https://github.com/jandk/valen)





