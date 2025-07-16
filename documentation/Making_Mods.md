
# Making Mods
A breakdown of what Atlan Mod Loader supports

### What is possible?
Currently, .decl modding is possible and nothing else. You can replace existing .decl files, or add your own original .decl files. Thanks to the revolutionary advancements of Atlan Mod Loader, all stability issues with adding new assets are eliminated. AssetsInfo JSONs are not needed, and you don't need to care about resource patch priority anymore!

The paths of all decl files must begin with `rs_streamfile` (i.e. `rs_streamfile/generated/decls/my_decl.decl`)

To get started with making mods, use [Atlan Resource Extractor](https://github.com/FlavorfulGecko5/EntityAtlan/releases/tag/Extractor) to extract the game's decl files!

### What about entitydefs and entities level modding?
idTech8 entity files have been pre-serialized into a binary format. To support level modding, a tool must first be programmed to deserialize, and reserialize, these files. This is a monumental task that requires generating 200,000+ lines of reflection code. Big progress has been made, but we're still far from being finished. If you want to contribute, check out the [relevant projects](https://github.com/FlavorfulGecko5/EntityAtlan)

### Mod Config File
Atlan Mod Loader uses `darkagesmod.txt` as a mod configuration file. Including one in your mod zip is optional, but highly recommended! 

Here is an example:
```
modinfo = {
    name = "Your Awesome Mod"
    author = "Your Name Here"
    description = "Your Mod Description"
    version = "3.0"
    loadPriority = 1
    requiredVersion = 1
}
aliasing = {
    "my_first_decl.decl" = "rs_streamfile/generated/decls/ability_dash/my_ability_dash_decl.decl"
    "my_second_decl.decl" = "rs_streamfile/generated/decls/ability_dash/default.decl"
}
```

If you've created mods for DOOM Eternal, most of this will be familiar. But there are a few key differences:
* We've moved away from JSON in favor of a simpler, decl-like syntax.
* The Mod Manager properties are now encased in a `modinfo` block. For those new to modding:
	* `name` - Your mod's name - displayed in the Mod Manager
	* `author` - A list of authors - displayed in the Mod Manager
	* `description` - Your mod's description - displayed in the Mod Manager
	* `version` - Your mod's version - displayed in the Mod Manager
	* `loadPriority`- If multiple mods edit the same file, their load priority is used to help resolve conflicts. Mods with a lower load priority will override mods with a higher load priority
	* `requiredVersion` - The version of *Atlan Mod Loader* you must be running to load this mod.   
* The `aliasing` block is an *optional* new feature. Is the name of your mod file extremely long and convoluted? Name it something simpler, then use your config. file to declare what's it name should be when mods are loaded!

### Zip Format
A major goal of Atlan Mod Loader is improving the developer experience when making mods. The config file's `aliasing` system is one way of achieving this. Another massive improvement is making directories optional!

Is your mod file nested inside of 5 or 6 different folders? Use `@` symbols in the filename, instead of creating *literal* directories.
For example: `rs_streamfile/generated/decls/strings/ui/mainmenu/a/b/c/somestring.decl` is encased in 9 folders! Eliminate them all by naming your file `rs_streamfile@generated@decls@strings@ui@mainmenu@a@b@c@somestring.decl`

You can also mix and match! Have a folder named `rs_streamfile@generated@decls@strings` and name your file `ui@mainmenu@a@b@c@somestring.decl`. Find a setup and style most convenient for you!

### Example Zip
To illustrate these new convenience features, I've uploaded a [sample mod zip.](https://github.com/FlavorfulGecko5/EntityAtlan/blob/master/documentation/Example_Mod.zip) Download it and check out the power at your fingertips!







