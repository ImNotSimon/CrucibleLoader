#pragma once

#define MOD_LOADER_VERSION 1

enum ArgFlags {
	argflag_resetvanilla = 1 << 0,
	argflag_gameupdated = 1 << 1,
	argflag_verbose = 1 << 2,
	argflag_nolaunch = 1 << 3,
	argflag_forceload = 1 << 4
};