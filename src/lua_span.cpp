#include "lua_span.h"

#include "lua_span_context.h"
#include "utility.h"

#define METATABLE "lua_opentracing_bridge.span"

namespace lua_bridge_tracer {
//------------------------------------------------------------------------------
// check_lua_span
//------------------------------------------------------------------------------
static LuaSpan* check_lua_span(lua_State* L) noexcept {
  void* user_data = luaL_checkudata(L, 1, METATABLE);
  luaL_argcheck(L, user_data != NULL, 1, "`" METATABLE "' expected");
  return *static_cast<LuaSpan**>(user_data);
}

//------------------------------------------------------------------------------
// compute_finish_span_options
//------------------------------------------------------------------------------
static opentracing::FinishSpanOptions compute_finish_span_options(lua_State* L,
                                                                int index) {
  opentracing::FinishSpanOptions result;

  lua_getfield(L, index, "finish_time");
  result.finish_steady_timestamp =
      opentracing::convert_time_point<opentracing::SteadyClock>(
          convert_timestamp(L, -1));
  lua_pop(L, 1);

  return result;
}

//------------------------------------------------------------------------------
// free
//------------------------------------------------------------------------------
int LuaSpan::free(lua_State* L) noexcept {
  auto span = check_lua_span(L);
  delete span;
  return 0;
}

//------------------------------------------------------------------------------
// finish
//------------------------------------------------------------------------------
int LuaSpan::finish(lua_State* L) noexcept {
  auto span = check_lua_span(L);
  auto num_arguments = lua_gettop(L);
  if (num_arguments >= 2) {
    luaL_checktype(L, 2, LUA_TTABLE);
  }
  try {
    opentracing::FinishSpanOptions finish_span_options;
    if (num_arguments >= 2) {
      finish_span_options = compute_finish_span_options(L, 2);
    }
    span->span_->FinishWithOptions(finish_span_options);
    return 0;
  } catch (const std::exception& e) {
    lua_pushstring(L, e.what());
  }
  return lua_error(L);
}

//------------------------------------------------------------------------------
// context
//------------------------------------------------------------------------------
int LuaSpan::context(lua_State* L) noexcept {
  auto span = check_lua_span(L);
  auto userdata = static_cast<LuaSpanContext**>(
      lua_newuserdata(L, sizeof(LuaSpanContext*)));
  try {
    auto lua_span_context =
        std::unique_ptr<LuaSpanContext>{new LuaSpanContext{span->span_}};
    *userdata = lua_span_context.release();

    luaL_getmetatable(L, LuaSpanContext::description.metatable);
    lua_setmetatable(L, -2);

    return 1;
  } catch (const std::exception& e) {
    lua_pushstring(L, e.what());
  }
  return lua_error(L);
}

//------------------------------------------------------------------------------
// set_tag
//------------------------------------------------------------------------------
int LuaSpan::set_tag(lua_State* L) noexcept {
  auto span = check_lua_span(L);
  size_t key_len;
  auto key_data = luaL_checklstring(L, -2, &key_len);
  try {
    opentracing::string_view key{key_data, key_len};
    switch (lua_type(L, -1)) {
      case LUA_TNUMBER: {
        auto value = static_cast<double>(lua_tonumber(L, -1));
        span->span_->SetTag(key, value);
        break;
      }
      case LUA_TSTRING: {
        size_t value_len;
        auto value_data = lua_tolstring(L, -1, &value_len);
        std::string value{value_data, value_len};
        span->span_->SetTag(key, std::move(value));
        break;
      }
      case LUA_TNIL:
      case LUA_TNONE: {
        span->span_->SetTag(key, nullptr);
        break;
      }
      default:
        throw std::runtime_error{"invalid set_tag type"};
    }
    return 0;
  } catch (const std::exception& e) {
    lua_pushstring(L, e.what());
  }
  return lua_error(L);
}

//------------------------------------------------------------------------------
// description
//------------------------------------------------------------------------------
const LuaClassDescription LuaSpan::description = {
    nullptr,
    METATABLE,
    LuaSpan::free,
    {{nullptr, nullptr}},
    {{"context", LuaSpan::context},
     {"finish", LuaSpan::finish},
     {"set_tag", LuaSpan::set_tag},
     {nullptr, nullptr}}};
}  // namespace lua_bridge_tracer
