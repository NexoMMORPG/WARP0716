#pragma once
#include "protocol.hpp"
// Native Stylist v2 costume adapter. External resources remain client data.
namespace client {
bool CostumeInitialize();
void CostumeRecipe(const ns::Recipe&);
bool CostumeRecipeReady(unsigned recipeId);
bool CostumeRecipeFailed(unsigned recipeId);
// 0 not yet rendered, 1 validated colored render, 2 failed with original fallback.
unsigned CostumeRenderStatus(unsigned recipeId);
// Used, editable source groups. Cached after successful native sprite loading.
unsigned CostumeCapabilities(unsigned recipeId);
unsigned CostumeSourceRGB(unsigned recipeId,unsigned group);
// Scoped to the reserved UI-only preview ID. Never changes a saved recipe.
void CostumePreviewHighlight(unsigned recipeId,int group);
void LocalCostumePreview(bool);
void ReleaseLocalCostumes();
void CostumeResetSession();
unsigned CostumeOriginalView(unsigned domain,unsigned view);
bool ReadResourceAlloc(const char*,void** data,unsigned* bytes);
void FreeResource(void*,unsigned bytes);
}
