#pragma once
#include "material.hpp"
using ns::u8;using ns::u16;using ns::u32;
#define EXPORT extern "C" __declspec(dllexport)
template<class T>T N(u32 a){return reinterpret_cast<T>(a);}
inline u32& F(void* p,unsigned o){return *reinterpret_cast<u32*>(static_cast<u8*>(p)+o);}
static_assert(sizeof(void*)==4,"Ragexe x86 ABI");
namespace client {
void Receive(const ns::State&);void Close(bool);bool Owns(void*);bool Key(void*,unsigned,unsigned);void PaletteRecipe(const ns::Recipe&);
void ReceiveItems(const ns::ItemsPacket&);
bool ReadResource(const char*,void*,unsigned);
}
