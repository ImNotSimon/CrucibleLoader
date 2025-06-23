# EntityAtlan

### This tool is a work-in-progress. No release build is published yet.

EntityAtlan is a tool for deserializing and reserializing DOOM: The Dark Ages entity files. Unlike in DOOM Eternal, these files are pre-serialized into a binary format. For level modding to be achievable, it's critical that a tool exists to decode these files, and re-encode our modded ones.

To accomplish this goal, EntityAtlan is a 2-step program.
* Step 1: Parse the Runtime Type Information (RTTI) dumped from DOOM: The Dark Ages' executable file to generate reflection code.
* Step 2: Use this reflection code to deserialize and reserialize entity files.

As a modder and normal user of EntityAtlan, you won't need to worry about Step 1.

### Credits
* FlavorfulGecko5 - Author of EntityAtlan
* jandk / tjoener - Extracting RTTI data from the game executable. Provided additional help with file formats, hashing algorithms, and other challenges. Author of idTech resource extractor [Valen](https://github.com/jandk/valen)





