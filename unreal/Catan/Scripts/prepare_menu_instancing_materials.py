import unreal


MESH_PATHS = (
    "/Game/Fab/Historical_house_with_exposed_bricks/historical_house_with_exposed_bricks/StaticMeshes/historical_house_with_exposed_bricks",
    "/Game/Fab/Megascans/3D/Stacked_Bricks_wjykcfnqx/Medium/wjykcfnqx_tier_2/StaticMeshes/wjykcfnqx_tier_2",
    "/Game/Fab/Old_Wooden_Barn__House_4_/ruined_house_4/StaticMeshes/ruined_house_4",
    "/Game/Fab/Megascans/3D/Round_Hay_Bale_rlCay/Medium/rlCay_tier_2/StaticMeshes/rlCay_tier_2",
    "/Game/Fab/Suffolk_Sheep_Thick_Wool_Fleece_Standing_Pose_3D_Model/3d_765/StaticMeshes/3d_765",
    "/Game/Fab/Mountain__1/mountain_1/StaticMeshes/mountain_1",
)


def base_material(material_interface):
    current = material_interface
    visited = set()
    while isinstance(current, unreal.MaterialInstance):
        path = current.get_path_name()
        if path in visited:
            return None
        visited.add(path)
        current = current.get_editor_property("parent")
    return current if isinstance(current, unreal.Material) else None


materials = {
    base_material(unreal.load_asset("/Game/Materials/M_CatanColor")),
    base_material(unreal.load_asset("/Game/Environment/Desert/M_OasisWater")),
}
for mesh_path in MESH_PATHS:
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"Missing static mesh: {mesh_path}")
    for slot in mesh.get_editor_property("static_materials"):
        material = base_material(slot.get_editor_property("material_interface"))
        if material:
            materials.add(material)

for material in materials:
    material.set_editor_property("used_with_instanced_static_meshes", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save material: {material.get_path_name()}")
    unreal.log(f"CATAN_MENU_INSTANCING enabled: {material.get_path_name()}")
