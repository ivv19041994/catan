import unreal


MESHES = [
    "/Game/Pirate/Mesh_UE5/Full/SKM_Pirate_Full_01",
    "/Game/Pirate/Mesh_UE5/Full/SKM_Pirate_Full_02",
    "/Game/Pirate/Mesh_UE5/Full/SKM_Pirate_Full_03",
]
ANIMATION = "/Game/Pirate/Demoscene_UE5/Animations/MM_Idle"


for path in MESHES:
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_error(f"CATAN_PIRATE missing mesh {path}")
        continue
    bounds = mesh.get_bounds()
    skeleton = mesh.get_editor_property("skeleton")
    materials = mesh.get_editor_property("materials")
    unreal.log(
        "CATAN_PIRATE mesh={} class={} origin={} extent={} sphere={} skeleton={} materials={}".format(
            path,
            mesh.get_class().get_name(),
            bounds.origin,
            bounds.box_extent,
            bounds.sphere_radius,
            skeleton.get_path_name() if skeleton else "None",
            len(materials),
        )
    )

animation = unreal.load_asset(ANIMATION)
if animation:
    skeleton = animation.get_editor_property("skeleton")
    unreal.log(
        "CATAN_PIRATE animation={} class={} skeleton={}".format(
            ANIMATION,
            animation.get_class().get_name(),
            skeleton.get_path_name() if skeleton else "None",
        )
    )
else:
    unreal.log_error(f"CATAN_PIRATE missing animation {ANIMATION}")
