#pragma once

/*
 * The pinned libultraship graphics interface only needs ImTextureID. Desktop
 * builds receive it from Dear ImGui; the native 3DS target has no ImGui
 * dependency, so keep this compatibility definition intentionally narrow.
 */
typedef void *ImTextureID;

