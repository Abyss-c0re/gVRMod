#pragma once

#include <gmod/Interface.h>
#include <vector>
#include <string>
#include <variant>
#include <cstring>

namespace mock {

// Records what the code pushed/set on the Lua stack for verification
struct LuaCall {
    enum Kind {
        PUSH_NUMBER, PUSH_STRING, PUSH_BOOL, PUSH_NIL,
        PUSH_VECTOR, PUSH_ANGLE, PUSH_CFUNC,
        CREATE_TABLE, SET_FIELD, SET_TABLE,
        GET_FIELD, POP, CALL, PUSH_SPECIAL,
        REF_CREATE, REF_PUSH, REF_FREE,
        THROW_ERROR, CHECK_STRING, CHECK_NUMBER,
        GET_TYPE, IS_TYPE
    };
    Kind kind;
    std::string strVal;
    double numVal = 0;
    bool boolVal = false;
    int intVal = 0;
};

class MockLuaBase : public GarrysMod::Lua::ILuaBase {
public:
    std::vector<LuaCall> calls;
    int nextRef = 1;
    std::string lastError;
    bool throwCalled = false;

    // Configurable returns for CheckString/CheckNumber
    std::vector<std::string> checkStringReturns;
    std::vector<double> checkNumberReturns;
    int checkStringIdx = 0;
    int checkNumberIdx = 0;

    // Configurable type returns for GetType
    std::vector<int> getTypeReturns;
    int getTypeIdx = 0;

    void Reset() {
        calls.clear();
        nextRef = 1;
        lastError.clear();
        throwCalled = false;
        checkStringIdx = 0;
        checkNumberIdx = 0;
        getTypeIdx = 0;
    }

    int  Top() override { return 0; }
    void Push(int) override {}
    void Pop(int iAmt) override { calls.push_back({LuaCall::POP, "", (double)iAmt}); }
    void GetTable(int) override {}
    void GetField(int, const char* name) override { calls.push_back({LuaCall::GET_FIELD, name ? name : ""}); }
    void SetField(int, const char* name) override { calls.push_back({LuaCall::SET_FIELD, name ? name : ""}); }
    void CreateTable() override { calls.push_back({LuaCall::CREATE_TABLE}); }
    void SetTable(int) override { calls.push_back({LuaCall::SET_TABLE}); }
    void SetMetaTable(int) override {}
    bool GetMetaTable(int) override { return false; }
    void Call(int, int) override { calls.push_back({LuaCall::CALL}); }
    int  PCall(int, int, int) override { return 0; }
    int  Equal(int, int) override { return 0; }
    int  RawEqual(int, int) override { return 0; }
    void Insert(int) override {}
    void Remove(int) override {}
    int  Next(int) override { return 0; }
    void* NewUserdata(unsigned int) override { return nullptr; }
    void ThrowError(const char* err) override {
        lastError = err ? err : "";
        throwCalled = true;
        calls.push_back({LuaCall::THROW_ERROR, lastError});
    }
    void CheckType(int, int) override {}
    void ArgError(int, const char*) override {}
    void RawGet(int) override {}
    void RawSet(int) override {}
    const char* GetString(int, unsigned int*) override { return ""; }
    double GetNumber(int) override { return 0; }
    bool GetBool(int) override { return false; }
    GarrysMod::Lua::CFunc GetCFunction(int) override { return nullptr; }
    void* GetUserdata(int) override { return nullptr; }
    void PushNil() override { calls.push_back({LuaCall::PUSH_NIL}); }
    void PushString(const char* val, unsigned int) override { calls.push_back({LuaCall::PUSH_STRING, val ? val : ""}); }
    void PushNumber(double val) override { calls.push_back({LuaCall::PUSH_NUMBER, "", val}); }
    void PushBool(bool val) override { calls.push_back({LuaCall::PUSH_BOOL, "", 0, val}); }
    void PushCFunction(GarrysMod::Lua::CFunc) override { calls.push_back({LuaCall::PUSH_CFUNC}); }
    void PushCClosure(GarrysMod::Lua::CFunc, int) override {}
    void PushUserdata(void*) override {}
    int  ReferenceCreate() override {
        calls.push_back({LuaCall::REF_CREATE, "", 0, false, nextRef});
        return nextRef++;
    }
    void ReferenceFree(int i) override { calls.push_back({LuaCall::REF_FREE, "", 0, false, i}); }
    void ReferencePush(int i) override { calls.push_back({LuaCall::REF_PUSH, "", 0, false, i}); }
    void PushSpecial(int t) override { calls.push_back({LuaCall::PUSH_SPECIAL, "", 0, false, t}); }
    bool IsType(int, int iType) override { return iType == GarrysMod::Lua::Type::TABLE; }
    int  GetType(int) override {
        if (getTypeIdx < (int)getTypeReturns.size())
            return getTypeReturns[getTypeIdx++];
        return GarrysMod::Lua::Type::NONE;
    }
    const char* GetTypeName(int) override { return "mock"; }
    void CreateMetaTableType(const char*, int) override {}
    const char* CheckString(int) override {
        if (checkStringIdx < (int)checkStringReturns.size())
            return checkStringReturns[checkStringIdx++].c_str();
        return "";
    }
    double CheckNumber(int) override {
        if (checkNumberIdx < (int)checkNumberReturns.size())
            return checkNumberReturns[checkNumberIdx++];
        return 0;
    }
    int ObjLen(int) override { return 0; }
    const QAngle& GetAngle(int) override { static QAngle a; return a; }
    const Vector& GetVector(int) override { static Vector v; return v; }
    void PushAngle(const QAngle&) override { calls.push_back({LuaCall::PUSH_ANGLE}); }
    void PushVector(const Vector&) override { calls.push_back({LuaCall::PUSH_VECTOR}); }
    void SetState(lua_State*) override {}
    int  CreateMetaTable(const char*) override { return 0; }
    bool PushMetaTable(int) override { return false; }
    void PushUserType(void*, int) override {}
    void SetUserType(int, void*) override {}
};

// Count calls of a specific kind
inline int CountCalls(const MockLuaBase& lua, LuaCall::Kind kind) {
    int n = 0;
    for (auto& c : lua.calls) if (c.kind == kind) n++;
    return n;
}

// Find SetField calls with a specific field name
inline bool HasSetField(const MockLuaBase& lua, const char* name) {
    for (auto& c : lua.calls)
        if (c.kind == LuaCall::SET_FIELD && c.strVal == name) return true;
    return false;
}

} // namespace mock
