import unreal


DESTINATION = "/Game/Environment/Ocean"
editing = unreal.MaterialEditingLibrary
normal_texture = unreal.load_asset(
    "/Water/Textures/Normals/T_Water_TilingNormal_Waves_02.T_Water_TilingNormal_Waves_02"
)
if not normal_texture:
    raise RuntimeError("UE Water normal texture is unavailable")


def constant(material, value, x, y):
    expression = editing.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    expression.set_editor_property("r", value)
    return expression


def color(material, value, x, y):
    expression = editing.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, x, y
    )
    expression.set_editor_property("constant", unreal.LinearColor(*value, 1.0))
    return expression


def create_material(name, translucent):
    path = f"{DESTINATION}/{name}"
    material = unreal.load_asset(path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DESTINATION, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Could not create {name}")

    material.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_OPAQUE,
    )
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)
    editing.delete_all_material_expressions(material)

    if translucent:
        depth_fade = editing.create_material_expression(
            material, unreal.MaterialExpressionDepthFade, -760, -260
        )
        depth_fade.set_editor_property("opacity_default", 1.0)
        depth_fade.set_editor_property("fade_distance_default", 155.0)

        shallow_color = color(material, (0.015, 0.20, 0.24), -540, -300)
        deep_color = color(material, (0.004, 0.045, 0.095), -540, -220)
        color_lerp = editing.create_material_expression(
            material, unreal.MaterialExpressionLinearInterpolate, -300, -260
        )
        editing.connect_material_expressions(shallow_color, "", color_lerp, "A")
        editing.connect_material_expressions(deep_color, "", color_lerp, "B")
        editing.connect_material_expressions(depth_fade, "", color_lerp, "Alpha")
        editing.connect_material_property(color_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

        shallow_opacity = constant(material, 0.16, -540, -100)
        deep_opacity = constant(material, 0.96, -540, -20)
        opacity_lerp = editing.create_material_expression(
            material, unreal.MaterialExpressionLinearInterpolate, -300, -70
        )
        editing.connect_material_expressions(shallow_opacity, "", opacity_lerp, "A")
        editing.connect_material_expressions(deep_opacity, "", opacity_lerp, "B")
        editing.connect_material_expressions(depth_fade, "", opacity_lerp, "Alpha")
        editing.connect_material_property(opacity_lerp, "", unreal.MaterialProperty.MP_OPACITY)

        refraction = constant(material, 1.015, -280, 20)
        editing.connect_material_property(refraction, "", unreal.MaterialProperty.MP_REFRACTION)
    else:
        deep_color = color(material, (0.003, 0.028, 0.065), -420, -180)
        editing.connect_material_property(deep_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    metallic = constant(material, 0.04 if translucent else 0.08, -260, 100)
    roughness = constant(material, 0.10 if translucent else 0.18, -260, 180)
    specular = constant(material, 0.95 if translucent else 0.75, -260, 260)
    editing.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    editing.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    texcoord = editing.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -900, 420
    )
    texcoord.set_editor_property("u_tiling", 9.5)
    texcoord.set_editor_property("v_tiling", 9.5)
    panner = editing.create_material_expression(material, unreal.MaterialExpressionPanner, -700, 420)
    panner.set_editor_property("speed_x", 0.018)
    panner.set_editor_property("speed_y", 0.011)
    editing.connect_material_expressions(texcoord, "", panner, "Coordinate")
    normal = editing.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -480, 420
    )
    normal.set_editor_property("texture", normal_texture)
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    editing.connect_material_expressions(panner, "", normal, "UVs")
    editing.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    editing.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


create_material("M_CatanOcean", True)
create_material("M_CatanOceanDepth", False)
print("CATAN_ASSET depth-aware translucent ocean and deep floor created")
