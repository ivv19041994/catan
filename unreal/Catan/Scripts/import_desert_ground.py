import os
import unreal

project_dir = unreal.Paths.project_dir()
source = os.path.join(project_dir, "ArtSource", "Environment", "Desert", "T_DesertSand_Albedo.png")
destination = "/Game/Environment/Desert"

task = unreal.AssetImportTask()
task.filename = source
task.destination_path = destination
task.destination_name = "T_DesertSand_Albedo"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.load_asset(destination + "/T_DesertSand_Albedo")
if not texture:
    raise RuntimeError("Desert sand texture import failed")
texture.set_editor_property("srgb", True)
texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
unreal.EditorAssetLibrary.save_loaded_asset(texture)

editing = unreal.MaterialEditingLibrary

def make_material(name, base_texture=None, color=None, roughness=0.9, metallic=0.0):
    path = destination + "/" + name
    material = unreal.load_asset(path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, destination, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Material creation failed: " + name)
    editing.delete_all_material_expressions(material)
    if base_texture:
        base = editing.create_material_expression(material, unreal.MaterialExpressionTextureSample, -320, 0)
        base.set_editor_property("texture", base_texture)
        editing.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        base = editing.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -320, 0)
        base.set_editor_property("constant", color)
        editing.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -120, 160)
    rough.set_editor_property("r", roughness)
    editing.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -120, 240)
    metal.set_editor_property("r", metallic)
    editing.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    editing.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

make_material("M_DesertSand", base_texture=texture, roughness=0.94)
make_material("M_OasisWater", color=unreal.LinearColor(0.006, 0.16, 0.19, 1.0), roughness=0.035, metallic=0.16)
print("CATAN_ASSET desert sand and oasis water materials imported")
