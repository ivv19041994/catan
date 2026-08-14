import os
import unreal

project_dir = unreal.Paths.project_dir()
source = os.path.join(project_dir, "ArtSource", "Environment", "Forest", "T_ForestGrass_Albedo.png")
destination = "/Game/Environment/Forest"

task = unreal.AssetImportTask()
task.filename = source
task.destination_path = destination
task.destination_name = "T_ForestGrass_Albedo"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.load_asset(destination + "/T_ForestGrass_Albedo")
if not texture:
    raise RuntimeError("Forest grass texture import failed")
texture.set_editor_property("srgb", True)
texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
unreal.EditorAssetLibrary.save_loaded_asset(texture)

material_path = destination + "/M_ForestGround"
material = unreal.load_asset(material_path)
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_ForestGround", destination, unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError("Forest ground material creation failed")

editing = unreal.MaterialEditingLibrary
editing.delete_all_material_expressions(material)
sample = editing.create_material_expression(material, unreal.MaterialExpressionTextureSample, -320, 0)
sample.set_editor_property("texture", texture)
editing.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
roughness = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -120, 180)
roughness.set_editor_property("r", 0.9)
editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
editing.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
print("CATAN_ASSET forest grass texture imported")
