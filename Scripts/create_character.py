"""Builds the player's look: /Game/Characters/Settler/SKM_Settler.

A simple, rugged settler in the spirit of Valheim: wool tunic, leather belt
and bracers, loose trousers, wrapped shins, leather boots, a mop of hair and
a beard. It's made from
the UE mannequin (SKM_Manny_Simple), so it keeps the mannequin's skeleton and
every animation: each vertex is sorted into a part of the outfit by the bone
that moves it (and, on the head, where it sits), coloured with that part's
material, and pushed out a little from the body so clothes sit on top of it.
Hair and beard are the head's own vertices pushed outward, so they move with
the head.

The character picks the mesh up when it exists (AHearthvaleCharacter::Appearance).
Run headless; delete /Game/Characters/Settler to build it again:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_character.py"
"""

import math

import unreal

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
gs = unreal
SOURCE = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
FOLDER = "/Game/Characters/Settler"
MESH = f"{FOLDER}/SKM_Settler"

# The outfit: part -> (tint, roughness, how far it sits out from the body in cm, texture, tiling).
# Cloth is a white woven wool tinted per part, so it reads as fabric rather than painted skin.
PARTS = {
    "Skin":     ((0.55, 0.36, 0.26), 0.65, 0.0, None, 1.0),
    "Hair":     ((0.2, 0.1, 0.045), 0.9, 1.6, "Cloth", 14.0),
    "Beard":    ((0.23, 0.115, 0.05), 0.9, 2.2, "Cloth", 14.0),
    "Tunic":    ((0.13, 0.17, 0.24), 0.95, 2.4, "Cloth", 9.0),
    "Belt":     ((1.1, 0.9, 0.7), 0.6, 1.6, "Leather", 5.0),
    "Trousers": ((0.2, 0.15, 0.1), 0.97, 2.4, "Cloth", 9.0),
    "Wraps":    ((0.46, 0.4, 0.31), 0.97, 1.0, "Cloth", 12.0),
    "Bracers":  ((1.7, 1.3, 0.95), 0.65, 1.0, "Leather", 5.0),
    "Boots":    ((1.3, 1.05, 0.8), 0.7, 1.2, "Leather", 4.0),
    "Eyes":     ((0.025, 0.02, 0.018), 0.25, 0.0, None, 1.0),
}
ORDER = list(PARTS)


def part_for_bone(bone):
    """Which part of the outfit a bone's flesh is, before the head and belt are worked out."""
    b = bone.lower()
    if b.startswith(("head", "neck")):
        return "Skin"
    if any(b.startswith(f) for f in ("hand", "thumb", "index", "middle", "ring", "pinky", "wrist", "weapon")):
        return "Skin"
    if b.startswith(("foot", "ball")):
        return "Boots"
    if b.startswith("calf"):
        return "Wraps"
    if b.startswith(("thigh", "pelvis")):
        return "Trousers"
    if b.startswith("lowerarm"):
        return "Bracers"
    return "Tunic"  # spine, clavicles, upper arms


def import_textures():
    """The cloth and leather from SourceArt/Character (ambientCG, CC0)."""
    source = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + "SourceArt/Character"
    textures = {}
    for kind in ("Cloth", "Leather"):
        for name, srgb, compression in (("Color", True, unreal.TextureCompressionSettings.TC_DEFAULT),
                                        ("NormalDX", False, unreal.TextureCompressionSettings.TC_NORMALMAP)):
            path = f"{FOLDER}/T_Settler_{kind}_{name}"
            if not assets.does_asset_exist(path):
                task = unreal.AssetImportTask()
                task.set_editor_property("filename", f"{source}/T_Settler_{kind}_{name}.jpg")
                task.set_editor_property("destination_path", FOLDER)
                task.set_editor_property("automated", True)
                task.set_editor_property("replace_existing", True)
                unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.load_asset(path)
            texture.set_editor_property("srgb", srgb)
            texture.set_editor_property("compression_settings", compression)
            assets.save_loaded_asset(texture, only_if_is_dirty=False)
            textures[(kind, name)] = texture
    return textures


def create_materials():
    """One material: a texture (or plain white) times a tint, with its normal map, tiled over the
    mannequin's UVs; an instance per part."""
    textures = import_textures()
    path = f"{FOLDER}/M_Settler"
    if assets.does_asset_exist(path):
        base = unreal.load_asset(path)
    else:
        base = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Settler", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        coords = mel.create_material_expression(base, unreal.MaterialExpressionTextureCoordinate, -1100, 100)
        tiling = mel.create_material_expression(base, unreal.MaterialExpressionScalarParameter, -1100, 250)
        tiling.set_editor_property("parameter_name", "Tiling")
        tiling.set_editor_property("default_value", 1.0)
        uv = mel.create_material_expression(base, unreal.MaterialExpressionMultiply, -900, 150)
        mel.connect_material_expressions(coords, "", uv, "A")
        mel.connect_material_expressions(tiling, "", uv, "B")
        albedo = mel.create_material_expression(base, unreal.MaterialExpressionTextureSampleParameter2D, -650, 0)
        albedo.set_editor_property("parameter_name", "Albedo")
        albedo.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
        mel.connect_material_expressions(uv, "", albedo, "UVs")
        normal = mel.create_material_expression(base, unreal.MaterialExpressionTextureSampleParameter2D, -650, 300)
        normal.set_editor_property("parameter_name", "Normal")
        normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        normal.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
        mel.connect_material_expressions(uv, "", normal, "UVs")
        color = mel.create_material_expression(base, unreal.MaterialExpressionVectorParameter, -650, -200)
        color.set_editor_property("parameter_name", "Color")
        tint = mel.create_material_expression(base, unreal.MaterialExpressionMultiply, -350, 0)
        mel.connect_material_expressions(albedo, "RGB", tint, "A")
        mel.connect_material_expressions(color, "", tint, "B")
        mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
        rough = mel.create_material_expression(base, unreal.MaterialExpressionScalarParameter, -350, 250)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.85)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        base.set_editor_property("used_with_skeletal_mesh", True)
        mel.recompile_material(base)
        assets.save_loaded_asset(base, only_if_is_dirty=False)
    instances = {}
    for name, (rgb, roughness, _, texture, tiling) in PARTS.items():
        mi_path = f"{FOLDER}/MI_Settler_{name}"
        mi = unreal.load_asset(mi_path) if assets.does_asset_exist(mi_path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"MI_Settler_{name}", FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, base)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*rgb, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        mel.set_material_instance_scalar_parameter_value(mi, "Tiling", tiling)
        if texture:
            mel.set_material_instance_texture_parameter_value(mi, "Albedo", textures[(texture, "Color")])
            mel.set_material_instance_texture_parameter_value(mi, "Normal", textures[(texture, "NormalDX")])
        assets.save_loaded_asset(mi, only_if_is_dirty=False)
        instances[name] = mi
    return instances

def vsub(a, b):
    return unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z)


def vlen(v):
    return math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z)


def build():
    if assets.does_asset_exist(MESH):
        unreal.log_warning("create_character: SKM_Settler already exists; delete it to rebuild.")
        return
    materials = create_materials()
    target = assets.duplicate_asset(SOURCE, MESH)
    source = unreal.load_asset(SOURCE)

    mesh = unreal.DynamicMesh()
    mesh, _ = gs.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        source, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())

    # Bones: names and where each sits in the reference pose.
    mesh, bones = gs.GeometryScript_BoneWeights.get_all_bones_info(mesh)
    names = [str(b.get_editor_property("name")) for b in bones]
    where = {str(b.get_editor_property("name")): b.get_editor_property("world_transform").translation for b in bones}
    head = where.get("head", unreal.Vector(0, 0, 160))
    pelvis = where.get("pelvis", unreal.Vector(0, 0, 95))
    # Which way the character faces: from the heel (foot) to the toes (ball).
    toe = vsub(where.get("ball_l", unreal.Vector(0, 10, 0)), where.get("foot_l", unreal.Vector(0, 0, 0)))
    flat = math.hypot(toe.x, toe.y) or 1.0
    fwd = unreal.Vector(toe.x / flat, toe.y / flat, 0)
    up = unreal.Vector(0, 0, 1)
    right = unreal.Vector(fwd.y, -fwd.x, 0)
    unreal.log_warning(f"create_character: {len(names)} bones, head {head}, pelvis {pelvis}, facing {fwd}")

    mesh, positions, _ = gs.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)
    points = list(gs.GeometryScript_List.convert_vector_list_to_array(positions))
    count = len(points)

    # Head size, to place hair and beard relative to it.
    region = []
    bone_of = []
    for vid in range(count):
        mesh, weights, ok = gs.GeometryScript_BoneWeights.get_vertex_bone_weights(mesh, vid)
        best, best_w = 0, -1.0
        for w in weights:
            if w.get_editor_property("weight") > best_w:
                best_w = w.get_editor_property("weight")
                best = w.get_editor_property("bone_index")
        bone = names[best] if 0 <= best < len(names) else "root"
        bone_of.append(bone)
        region.append(part_for_bone(bone))
    # The head bone sits at the base of the skull: use the middle of the head's own vertices instead.
    head_ids = [i for i in range(count) if bone_of[i].lower().startswith("head")]
    if head_ids:
        head = unreal.Vector(sum(points[i].x for i in head_ids) / len(head_ids), sum(points[i].y for i in head_ids) / len(head_ids),
                             sum(points[i].z for i in head_ids) / len(head_ids))
    radius = max((vlen(vsub(points[i], head)) for i in head_ids), default=12.0)

    # Hair over the top and back of the head; beard on the lower front; a belt band at the waist.
    for i in range(count):
        p = points[i]
        if bone_of[i].lower().startswith(("head", "neck")):
            d = vsub(p, head)
            front = (d.x * fwd.x + d.y * fwd.y) / radius
            height = d.z / radius
            side = abs(d.x * right.x + d.y * right.y) / radius
            if region[i] == "Skin" and bone_of[i].lower().startswith("head"):
                if height > 0.5 or (front < -0.1 and height > -0.45) or (front < 0.25 and height > 0.3):
                    region[i] = "Hair"
                elif front > 0.05 and height < -0.38:
                    region[i] = "Beard"  # jaw and chin; the cheeks and nose stay bare
            if bone_of[i].lower().startswith("neck") and front > 0.25 and d.z > -radius * 1.2:
                region[i] = "Beard"
        elif region[i] in ("Tunic", "Trousers"):
            # Clean, level cuts round the body rather than the zigzag where bones meet:
            # tunic above, a belt, the tunic's hem hanging below it, then trousers.
            if pelvis.z + 3.0 < p.z < pelvis.z + 9.0:
                region[i] = "Belt"
            elif p.z >= pelvis.z + 9.0 or p.z > pelvis.z - 16.0:
                region[i] = "Tunic"
            else:
                region[i] = "Trousers"

    # Cloth drapes over the body rather than hugging every muscle: smooth the clothed parts
    # (each vertex drifts towards the middle of its neighbours) before pushing them out.
    mesh, triangles, _ = gs.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False)
    tris = list(gs.GeometryScript_List.convert_triangle_list_to_array(triangles))
    neighbours = [set() for _ in range(count)]
    for t in tris:
        if min(t.x, t.y, t.z) < 0:
            continue
        neighbours[t.x].update((t.y, t.z))
        neighbours[t.y].update((t.x, t.z))
        neighbours[t.z].update((t.x, t.y))
    draped = {"Tunic": 0.75, "Trousers": 0.75, "Belt": 0.5, "Bracers": 0.5}
    for _ in range(24):
        smoothed = list(points)
        for i in range(count):
            amount = draped.get(region[i])
            if not amount or not neighbours[i]:
                continue
            n = neighbours[i]
            ax = sum(points[j].x for j in n) / len(n)
            ay = sum(points[j].y for j in n) / len(n)
            az = sum(points[j].z for j in n) / len(n)
            p = points[i]
            smoothed[i] = unreal.Vector(p.x + (ax - p.x) * amount, p.y + (ay - p.y) * amount, p.z + (az - p.z) * amount)
        points = smoothed

    # Push each part out from its bone so clothes sit on the body, hair and beard most of all.
    out = []
    for i in range(count):
        p = points[i]
        part = region[i]
        lift = PARTS[part][2]
        if lift == 0.0:
            out.append(p)
            continue
        centre = head if part in ("Hair", "Beard") else where.get(bone_of[i], p)
        d = vsub(p, centre)
        length = vlen(d) or 1.0
        n = unreal.Vector(d.x / length, d.y / length, d.z / length)
        if part == "Beard":
            # A full beard hangs: out and down, more towards the chin.
            drop = max(0.0, -(p.z - head.z) / radius)
            out.append(unreal.Vector(p.x + n.x * lift + fwd.x * drop * 1.5, p.y + n.y * lift + fwd.y * drop * 1.5, p.z + n.z * lift - drop * 2.5))
        else:
            out.append(unreal.Vector(p.x + n.x * lift, p.y + n.y * lift, p.z + n.z * lift))
    moved = gs.GeometryScript_List.convert_array_to_vector_list(out)
    mesh = gs.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh, moved)

    # Each triangle takes the part most of its corners belong to.
    mesh, triangles, _ = gs.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False)
    ids = []
    for tri in gs.GeometryScript_List.convert_triangle_list_to_array(triangles):
        if min(tri.x, tri.y, tri.z) < 0:  # a gap in the triangle IDs
            ids.append(0)
            continue
        corners = [region[tri.x], region[tri.y], region[tri.z]]
        best = max(set(corners), key=lambda r: (corners.count(r), -ORDER.index(r)))
        ids.append(ORDER.index(best))
    id_list = gs.GeometryScript_List.convert_array_to_index_list(ids)
    mesh = gs.GeometryScript_Materials.set_all_triangle_material_i_ds(mesh, id_list)

    # Eyes: two small dark balls set into the face, skinned to the head (the mannequin's face is too
    # plain to paint them on). Each sits just behind the front-most point of the face at eye height.
    head_bone = names.index("head") if "head" in names else 0
    for sign in (-1.0, 1.0):
        best_p, best_front = None, -1e9
        for i in head_ids:
            d = vsub(points[i], head)
            side = (d.x * right.x + d.y * right.y) / radius
            if abs(side - sign * 0.27) < 0.12 and -0.02 < d.z / radius < 0.14:
                front = d.x * fwd.x + d.y * fwd.y
                if front > best_front:
                    best_front, best_p = front, points[i]
        if best_p is None:
            continue
        centre = unreal.Vector(best_p.x - fwd.x * 0.5, best_p.y - fwd.y * 0.5, best_p.z)
        before = gs.GeometryScript_MeshQueries.get_vertex_count(mesh)
        primitive = unreal.GeometryScriptPrimitiveOptions()
        primitive.set_editor_property("material_id", ORDER.index("Eyes"))
        mesh = gs.GeometryScript_Primitives.append_sphere_lat_long(mesh, primitive, unreal.Transform(centre, unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1)), 1.15, 8, 10)
        after = gs.GeometryScript_MeshQueries.get_vertex_count(mesh)
        weight = unreal.GeometryScriptBoneWeight()
        weight.set_editor_property("bone_index", head_bone)
        weight.set_editor_property("weight", 1.0)
        for vid in range(before, after):
            mesh, _ = gs.GeometryScript_BoneWeights.set_vertex_bone_weights(mesh, vid, [weight])
        unreal.log_warning(f"create_character: eye at {centre}")

    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", True)
    options.set_editor_property("enable_recompute_tangents", True)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [materials[name] for name in ORDER])
    options.set_editor_property("new_material_slot_names", ORDER)
    mesh, outcome = gs.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(mesh, target, options, unreal.GeometryScriptMeshWriteLOD())
    assets.save_loaded_asset(target, only_if_is_dirty=False)
    counts = {name: region.count(name) for name in ORDER}
    unreal.log_warning(f"create_character: built SKM_Settler ({outcome}) - vertices per part {counts}")


build()
