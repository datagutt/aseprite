// Aseprite
// Copyright (C) 2018-2023  Igara Studio S.A.
// Copyright (C) 2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/cmd/set_slice_key.h"
#include "app/cmd/set_slice_name.h"
#include "app/script/docobj.h"
#include "app/script/engine.h"
#include "app/script/luacpp.h"
#include "app/script/userdata.h"
#include "app/tx.h"
#include "doc/slice.h"
#include "doc/sprite.h"

namespace app { namespace script {

using namespace doc;

namespace {

int Slice_eq(lua_State* L)
{
  const auto a = may_get_docobj<Slice>(L, 1);
  const auto b = may_get_docobj<Slice>(L, 2);
  lua_pushboolean(L, (!a && !b) || (a && b && a->id() == b->id()));
  return 1;
}

int Slice_get_sprite(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  push_docobj(L, slice->owner()->sprite());
  return 1;
}

int Slice_get_name(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  lua_pushstring(L, slice->name().c_str());
  return 1;
}

int Slice_get_bounds(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  if (!slice->empty())
    push_new<gfx::Rect>(L, slice->begin()->value()->bounds());
  else
    lua_pushnil(L);
  return 1;
}

int Slice_get_center(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  if (!slice->empty() && slice->begin()->value()->hasCenter())
    push_new<gfx::Rect>(L, slice->begin()->value()->center());
  else
    lua_pushnil(L);
  return 1;
}

int Slice_get_pivot(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  if (!slice->empty() && slice->begin()->value()->hasPivot())
    push_new<gfx::Point>(L, slice->begin()->value()->pivot());
  else
    lua_pushnil(L);
  return 1;
}

int Slice_set_name(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  const char* name = lua_tostring(L, 2);
  if (name) {
    Tx tx(slice->sprite());
    tx(new cmd::SetSliceName(slice, name));
    tx.commit();
  }
  return 0;
}

int Slice_set_bounds(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  gfx::Rect bounds = convert_args_into_rect(L, 2);
  SliceKey key;
  if (const SliceKey* srcKey = slice->getByFrame(0))
    key = *srcKey;
  key.setBounds(bounds);
  Tx tx(slice->sprite());
  tx(new cmd::SetSliceKey(slice, 0, key));
  tx.commit();
  return 0;
}

int Slice_set_center(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  gfx::Rect center = convert_args_into_rect(L, 2);
  SliceKey key;
  if (const SliceKey* srcKey = slice->getByFrame(0))
    key = *srcKey;
  key.setCenter(center);
  Tx tx(slice->sprite());
  tx(new cmd::SetSliceKey(slice, 0, key));
  tx.commit();
  return 0;
}

int Slice_set_pivot(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  gfx::Point pivot = convert_args_into_point(L, 2);
  SliceKey key;
  if (const SliceKey* srcKey = slice->getByFrame(0))
    key = *srcKey;
  key.setPivot(pivot);
  Tx tx(slice->sprite());
  tx(new cmd::SetSliceKey(slice, 0, key));
  tx.commit();
  return 0;
}

// slice:setKey(frame, { bounds=Rectangle, center=Rectangle, pivot=Point })
//
// Sets the key of one frame, so a slice can change from frame to frame. The bounds, center and
// pivot properties only reach the key of the first frame. Fields left out keep the values of
// the key that was active on that frame.
int Slice_setKey(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  const doc::frame_t frame = get_frame_number_from_arg(L, 2);
  if (frame < 0 || frame >= slice->sprite()->totalFrames())
    return luaL_error(L, "frame %d is out of range", int(frame + 1));
  if (!lua_istable(L, 3))
    return luaL_error(L, "slice:setKey() expects a table with bounds, center or pivot");

  SliceKey key;
  if (const SliceKey* srcKey = slice->getByFrame(frame))
    key = *srcKey;

  if (lua_getfield(L, 3, "bounds") != LUA_TNIL)
    key.setBounds(convert_args_into_rect(L, -1));
  lua_pop(L, 1);
  if (lua_getfield(L, 3, "center") != LUA_TNIL)
    key.setCenter(convert_args_into_rect(L, -1));
  lua_pop(L, 1);
  if (lua_getfield(L, 3, "pivot") != LUA_TNIL)
    key.setPivot(convert_args_into_point(L, -1));
  lua_pop(L, 1);

  Tx tx(slice->sprite());
  tx(new cmd::SetSliceKey(slice, frame, key));
  tx.commit();
  return 0;
}

// slice:getKey(frame) returns { bounds=Rectangle, center=Rectangle, pivot=Point } for the key
// active on that frame, with center and pivot left out when the key has none, or nil when no
// key covers the frame.
int Slice_getKey(lua_State* L)
{
  auto slice = get_docobj<Slice>(L, 1);
  const doc::frame_t frame = get_frame_number_from_arg(L, 2);
  const SliceKey* key = slice->getByFrame(frame);
  if (!key || key->isEmpty()) {
    lua_pushnil(L);
    return 1;
  }
  lua_newtable(L);
  push_new<gfx::Rect>(L, key->bounds());
  lua_setfield(L, -2, "bounds");
  if (key->hasCenter()) {
    push_new<gfx::Rect>(L, key->center());
    lua_setfield(L, -2, "center");
  }
  if (key->hasPivot()) {
    push_new<gfx::Point>(L, key->pivot());
    lua_setfield(L, -2, "pivot");
  }
  return 1;
}

const luaL_Reg Slice_methods[] = {
  { "setKey", Slice_setKey },
  { "getKey", Slice_getKey },
  { "__eq",  Slice_eq },
  { nullptr, nullptr  }
};

const Property Slice_properties[] = {
  { "sprite",     Slice_get_sprite,               nullptr                        },
  { "name",       Slice_get_name,                 Slice_set_name                 },
  { "bounds",     Slice_get_bounds,               Slice_set_bounds               },
  { "center",     Slice_get_center,               Slice_set_center               },
  { "pivot",      Slice_get_pivot,                Slice_set_pivot                },
  { "color",      UserData_get_color<Slice>,      UserData_set_color<Slice>      },
  { "data",       UserData_get_text<Slice>,       UserData_set_text<Slice>       },
  { "properties", UserData_get_properties<Slice>, UserData_set_properties<Slice> },
  { nullptr,      nullptr,                        nullptr                        }
};

} // anonymous namespace

DEF_MTNAME(Slice);

void register_slice_class(lua_State* L)
{
  using doc::Slice;
  REG_CLASS(L, Slice);
  REG_CLASS_PROPERTIES(L, Slice);
}

}} // namespace app::script
