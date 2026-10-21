"""Bakes the Procedural Vegetation Editor's sample plants into static meshes for the island.

The plugin (Engine/Plugins/Experimental/ProceduralVegetationEditor, enabled in
Hearthvale.uproject) ships finished, textured plants as skeletal meshes, for
wind. The island draws trees as gathering-node parts and instanced scenery,
which need static meshes, so this copies each into /Game/Vegetation as a
Nanite static mesh with the same materials, and carries over its leaves and
twigs (a Nanite assembly of separate meshes, posed on the branches at rest). No collision: trees pair the mesh
with an invisible trunk cylinder (see create_world.py).

Run headless:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/bake_vegetation.py"
Existing meshes are left alone; delete one to bake it again.
"""

import unreal

SOURCE = "/ProceduralVegetationEditor/SampleAssets/StarterContent"
TARGET = "/Game/Vegetation"
# name: sample skeletal mesh
PLANTS = {
    "SM_PV_BroadleafTree": f"{SOURCE}/DeciduousTree_01/PVE_Deciduous_Tree_01",
    "SM_PV_Conifer": f"{SOURCE}/ConiferTree_01/PVE_Conifer_01",
    "SM_PV_Shrub": f"{SOURCE}/Deciduous_Shrub_01/PVE_Deciduous_Shrub_01",
    "SM_PV_Plant": f"{SOURCE}/Plant_01/PVE_Plant_01",
}

assets = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/ProceduralVegetationEditor"], True)


def nanite_settings():
    nanite = unreal.MeshNaniteSettings()
    nanite.set_editor_property("enabled", True)
    # Foliage keeps its shape at a distance by turning into voxels rather than thinning out (if the project allows it).
    try:
        nanite.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.VOXELIZE)
    except Exception as error:
        unreal.log_warning(f"bake_vegetation: no voxel shape preservation ({error})")
    return nanite


def bake(name, source_path):
    path = f"{TARGET}/{name}"
    if assets.does_asset_exist(path):
        unreal.log_warning(f"bake_vegetation: {name} already baked")
        return
    source = unreal.load_asset(source_path)
    mesh = unreal.DynamicMesh()
    read = unreal.GeometryScriptMeshReadLOD()
    mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(source, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), read)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_nanite", True)
    options.set_editor_property("nanite_settings", nanite_settings())
    options.set_editor_property("enable_collision", False)
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("enable_recompute_tangents", False)
    static, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, options)
    if not static:
        unreal.log_error(f"bake_vegetation: couldn't make {name} ({outcome})")
        return
    # The same materials, slot for slot.
    materials = [m.material_interface for m in source.materials]
    slots = static.static_materials
    for i, slot in enumerate(slots):
        if i < len(materials):
            slot.set_editor_property("material_interface", materials[i])
    static.set_editor_property("static_materials", slots)
    # The leaves and twigs aren't in the geometry: they're a Nanite assembly of separate meshes placed on the branches.
    pieces = unreal.VegetationBaker.copy_nanite_assembly(source, static)
    assets.save_loaded_asset(static, only_if_is_dirty=False)
    b = static.get_bounds()
    unreal.log_warning(f"bake_vegetation: {name} {len(slots)} slots, {static.get_num_nanite_triangles()} Nanite triangles, {pieces} leaf/twig pieces, size {b.box_extent * 2}")


for name, source_path in PLANTS.items():
    bake(name, source_path)


def evergreen(mesh_name, foliage_path):
    """The plugin's foliage material shades leaves by season (its defaults read as autumn: brown
    needles, orange maples). Give the baked mesh a copy with seasons off, so it stays green."""
    mesh = unreal.load_asset(f"{TARGET}/{mesh_name}")
    parent = unreal.load_asset(foliage_path)
    if not mesh or not parent:
        return
    name = parent.get_name() + "_Evergreen"
    path = f"{TARGET}/{name}"
    mi = unreal.load_asset(path) if assets.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, TARGET, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_static_switch_parameter_value(mi, "Seasons", False)
    mel.update_material_instance(mi)
    assets.save_loaded_asset(mi, only_if_is_dirty=False)
    slots = mesh.static_materials
    for slot in slots:
        if slot.material_interface == parent:
            slot.set_editor_property("material_interface", mi)
    mesh.set_editor_property("static_materials", slots)
    assets.save_loaded_asset(mesh, only_if_is_dirty=False)
    unreal.log_warning(f"bake_vegetation: {mesh_name} foliage is evergreen ({name})")


evergreen("SM_PV_Conifer", f"{SOURCE}/ConiferTree_01/Materials/MI_Conifer_Foliage_01")
evergreen("SM_PV_BroadleafTree", f"{SOURCE}/DeciduousTree_01/Materials/MI_LeafTree_01_Foliage")
evergreen("SM_PV_Shrub", f"{SOURCE}/DeciduousTree_01/Materials/MI_LeafTree_01_Foliage")
