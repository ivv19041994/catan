import unreal


ASSETS = {
    "/Game/Viking_Village_Pack/Meshes/SM_Barn_1_2":
        "/Game/Catan/RuntimeAssets/Viking/SM_Barn_1_2",
    "/Game/Viking_Village_Pack/Meshes/SM_Barn_2":
        "/Game/Catan/RuntimeAssets/Viking/SM_Barn_2",
    "/Game/Viking_Village_Pack/Meshes/SM_Stone_1":
        "/Game/Catan/RuntimeAssets/Viking/SM_Stone_1",
}


for source, destination in ASSETS.items():
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        unreal.log(f"CATAN_ANDROID_ASSET already exists: {destination}")
        continue
    if not unreal.EditorAssetLibrary.duplicate_asset(source, destination):
        raise RuntimeError(f"Could not duplicate {source} to {destination}")
    unreal.log(f"CATAN_ANDROID_ASSET duplicated: {source} -> {destination}")

unreal.EditorAssetLibrary.save_directory(
    "/Game/Catan/RuntimeAssets", only_if_is_dirty=False, recursive=True)
