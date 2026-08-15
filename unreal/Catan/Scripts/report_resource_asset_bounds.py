import unreal

paths = [
    "/Game/Fab/Suffolk_Sheep_Thick_Wool_Fleece_Standing_Pose_3D_Model/3d_765/StaticMeshes/3d_765",
    "/Game/Fab/Megascans/Plants/Wild_Grass_vlkhcbxia/Medium/vlkhcbxia_tier_2/StaticMeshes/SM_vlkhcbxia_VarA",
    "/Game/Fab/Megascans/Plants/Wild_Grass_vlkhcbxia/Medium/vlkhcbxia_tier_2/StaticMeshes/SM_vlkhcbxia_VarD",
    "/Game/Fab/Old_Wooden_Barn__House_4_/ruined_house_4/StaticMeshes/ruined_house_4",
    "/Game/Fab/Megascans/3D/Round_Hay_Bale_rlCay/Medium/rlCay_tier_2/StaticMeshes/rlCay_tier_2",
]

for path in paths:
    mesh = unreal.load_asset(path)
    if not mesh:
        print("CATAN_BOUNDS MISSING " + path)
        continue
    bounds = mesh.get_bounds()
    print("CATAN_BOUNDS {} origin={} extent={}".format(path, bounds.origin, bounds.box_extent))
