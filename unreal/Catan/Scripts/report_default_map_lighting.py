import unreal


world = unreal.EditorLoadingAndSavingUtils.load_map(
    "/Engine/Maps/Templates/Template_Default"
)
if not world:
    raise RuntimeError("Could not load Template_Default")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
unreal.log("CATAN_LIGHT actor_count={}".format(len(actors)))
for actor in actors:
    if "light" in actor.get_class().get_name().lower():
        unreal.log(
            "CATAN_LIGHT actor={} class={}".format(
                actor.get_name(), actor.get_class().get_name()
            )
        )
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    if not component:
        continue
    fields = {
        "actor": actor.get_name(),
        "mobility": str(component.get_editor_property("mobility")),
        "cast_shadows": component.get_editor_property("cast_shadows"),
        "cast_dynamic_shadows": component.get_editor_property("cast_dynamic_shadows"),
        "dynamic_movable": component.get_editor_property(
            "dynamic_shadow_distance_movable_light"
        ),
        "dynamic_stationary": component.get_editor_property(
            "dynamic_shadow_distance_stationary_light"
        ),
        "cascades": component.get_editor_property("dynamic_shadow_cascades"),
        "intensity": component.get_editor_property("intensity"),
        "shadow_amount": component.get_editor_property("shadow_amount"),
        "actor_rotation": str(actor.get_actor_rotation()),
    }
    unreal.log("CATAN_LIGHT " + repr(fields))
