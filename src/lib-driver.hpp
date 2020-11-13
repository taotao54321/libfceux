#pragma once

#include "fceux.h"

int LoadGame(const char* path, bool silent);

extern FceuxHookBeforeExec hook_before_exec;
extern void* hook_before_exec_userdata;
