// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Precompiled.h"
#include "Script/LuaVM.h"
#include "Game/GameWorld.h"
#include "File/FileSystem.h"
#include "File/File.h"
#include "Core/CVars.h"
#include "Core/Cmds.h"

extern "C" {
#include "luasocket/luasocket.h"
}

BE_NAMESPACE_BEGIN

static CVar lua_debuggerAddr(L"lua_debuggerAddr", L"localhost", CVar::Archive, L"Lua debugger address for remote debugging");

void LuaVM::Init() {
    if (state) {
        Shutdown();
    }

    state = new LuaCpp::State(true);

    //BE_LOG(L"Lua version %.1f\n", state->Version());

    state->HandleExceptionsWith([](int status, std::string msg, std::exception_ptr exception) {
        const char *statusStr = "";
        switch (status) {
        case LUA_ERRRUN:
            statusStr = "LUA_ERRRUN";
            break;
        case LUA_ERRSYNTAX:
            statusStr = "LUA_ERRSYNTAX";
            break;
        case LUA_ERRMEM:
            statusStr = "LUA_ERRMEM";
            break;
#if LUA_VERSION_NUM >= 502
        case LUA_ERRGCMM:
            statusStr = "LUA_ERRGCMM";
            break;
#endif
        case LUA_ERRERR:
            statusStr = "LUA_ERRERR";
            break;
        }
        BE_ERRLOG(L"%hs - %hs\n", statusStr, msg.c_str());
    });

    state->RegisterSearcher([this](const std::string &name) {
        Str filename = name.c_str();
        filename.DefaultFileExtension(".lua");

        char *data;
        size_t size = fileSystem.LoadFile(filename, true, (void **)&data);
        if (!data) {
            size = fileSystem.LoadFile("Scripts/" + filename, true, (void **)&data);
            if (!data) {
                return false;
            }
        }

        state->RunBuffer(name, data, size, name.c_str());

        fileSystem.FreeFile(data);
        return true;
    });

    //state->Require("blueshift.io", luaopen_file);
}

void LuaVM::InitEngineModule(const GameWorld *gameWorld) {
    LuaVM::gameWorld = gameWorld;

    state->RegisterModule("blueshift", [this](LuaCpp::Module &module) {
        module["log"].SetFunc([](const std::string &msg) {
            BE_LOG(L"%hs\n", msg.c_str());
        });
        module["unit_to_centi"].SetFunc(&UnitToCenti);
        module["unit_to_meter"].SetFunc(&UnitToMeter);
        module["centi_to_unit"].SetFunc(&CentiToUnit);
        module["meter_to_unit"].SetFunc(&MeterToUnit);

        // Math
        RegisterMath(module);
        RegisterComplex(module);
        RegisterVec2(module);
        RegisterVec3(module);
        RegisterVec4(module);
        RegisterColor3(module);
        RegisterColor4(module);
        RegisterMat2(module);
        RegisterMat3(module);
        RegisterMat3x4(module);
        RegisterMat4(module);
        RegisterQuaternion(module);
        RegisterAngles(module);
        RegisterRotation(module);
        RegisterPlane(module);
        RegisterSphere(module);
        RegisterCylinder(module);
        RegisterAABB(module);
        RegisterOBB(module);
        RegisterFrustum(module);
        RegisterRay(module);
        RegisterPoint(module);
        RegisterRect(module);
        // Common
        RegisterCommon(module);
        // Input
        RegisterInput(module);
        // Screen
        RegisterScreen(module);
        // Physics
        RegisterPhysics(module);
        // Str
        RegisterStr(module);
        // File
        RegisterFile(module);
        RegisterFileSystem(module);
        // Object
        RegisterObject(module);
        // Asset
        RegisterAsset(module);
        RegisterTextureAsset(module);
        RegisterShaderAsset(module);
        RegisterMaterialAsset(module);
        RegisterSkeletonAsset(module);
        RegisterMeshAsset(module);
        RegisterAnimAsset(module);
        RegisterAnimControllerAsset(module);
        RegisterSoundAsset(module);
        RegisterMapAsset(module);
        RegisterPrefabAsset(module);
        // Component
        RegisterComponent(module);
        RegisterTransformComponent(module);
        RegisterColliderComponent(module);
        RegisterBoxColliderComponent(module);
        RegisterSphereColliderComponent(module);
        RegisterCylinderColliderComponent(module);
        RegisterCapsuleColliderComponent(module);
        RegisterMeshColliderComponent(module);
        RegisterRigidBodyComponent(module);
        RegisterSensorComponent(module);
        RegisterJointComponent(module);
        RegisterFixedJointComponent(module);
        RegisterHingeJointComponent(module);
        RegisterSocketJointComponent(module);
        RegisterSpringJointComponent(module);
        RegisterCharacterJointComponent(module);
        RegisterConstantForceComponent(module);
        RegisterCharacterControllerComponent(module);
        RegisterRenderableComponent(module);
        RegisterMeshRendererComponent(module);
        RegisterStaticMeshRendererComponent(module);
        RegisterSkinnedMeshRendererComponent(module);
        RegisterAnimatorComponent(module);
        RegisterTextRendererComponent(module);
        RegisterParticleSystemComponent(module);
        RegisterCameraComponent(module);
        RegisterLightComponent(module);
        RegisterAudioListenerComponent(module);
        RegisterAudioSourceComponent(module);
        RegisterSplineComponent(module);
        RegisterScriptComponent(module);
        // Game World
        RegisterEntity(module);
        RegisterGameWorld(module);

        for (int i = 0; i < engineModuleCallbacks.Count(); i++) {
            engineModuleCallbacks[i](module);
        }
    });

    //state->Require("blueshift");
}

void LuaVM::Shutdown() {
    engineModuleCallbacks.Clear();

    SAFE_DELETE(state);
}

void LuaVM::RegisterEngineModuleCallback(EngineModuleCallback callback) {
    engineModuleCallbacks.Append(callback);
}

const char *LuaVM::GetLuaVersion() const { 
    static char versionString[32] = "";
    int major, minor;
    state->Version(major, minor);
    Str::snPrintf(versionString, sizeof(versionString), "%i.%i", major, minor);
    return versionString;
}

const char *LuaVM::GetLuaJitVersion() const {
    static char versionString[32] = "";
    int major, minor, patch;
    state->JitVersion(major, minor, patch); 
    Str::snPrintf(versionString, sizeof(versionString), "%i.%i.%i", major, minor, patch);
    return versionString;
}

void LuaVM::EnableJIT(bool enabled) {
    state->EnableJIT(enabled);
}

void LuaVM::StartDebuggee() {
#if 1
    return;
    // Lua Debugger by devCAT
    // https://marketplace.visualstudio.com/items?itemName=devCAT.lua-debug
    Str addr = Str(lua_debuggerAddr.GetString());
    state->Require("socket.core", luaopen_socket_core);
    const char *text = va(R"(
local blueshift = require 'blueshift'
local json = require 'dkjson'
local debuggee = require 'vscode-debuggee'
local config = { redirectPrint = true, controllerHost = '%hs' }
local startResult, breakerType = debuggee.start(json, config)
if startResult then
    blueshift.log('Connected to debugger ('..breakerType..')')
else
    blueshift.log('Failed to connect to debugger')
end
    )", addr.c_str());
    (*state)(text);
#else
    char *addr = tombs(lua_debuggerAddr.GetString());

    File *fp = fileSystem.OpenFileRead("Scripts/debug/debug.lua", true);
    if (!fp) {
        return;
    }
    fileSystem.CloseFile(fp);

    state->Require("socket.core", luaopen_socket_core);
    char *cmd = va("assert(load(_G['blueshift.io'].open('Scripts/debug/debug.lua', 'rb'):read('*a'), '@Scripts/debug/debug.lua'))('%s')", addr);
    (*state)(cmd);
#endif
}

void LuaVM::PollDebuggee() {
#if 0
    (*state)(R"(
local debuggee = require 'vscode-debuggee'
debuggee.poll()
    )");
#endif
}

BE_NAMESPACE_END
