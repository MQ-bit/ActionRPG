"""Parkour motion regression test (headless editor, PIE on L_Parkour).

Runs a fixed list of vault/mantle cases once, measures what the numbers can judge and prints one PASS/FAIL line per case:
  - the move starts,
  - bone segments do not go into the obstacle deeper than the limits (mannequin bone radii, so feet show ~5-7 cm at rest),
  - hands stay on the ledge during the clip's hand IK window (windows are read from BP_GA_Parkour, ramps excluded),
  - a mantle ends standing on the top, without a height pop when the montage ends.
How natural the motion looks is NOT judged here; that stays a developer PIE check.

Run: Tools/MotionRegression/run_parkour_motion_regression.ps1 (editor must be closed).
Needs the gitignored parkour assets (Survival_Character, Content/Animations/Parkour); without them the cases do not start.
"""
import unreal, builtins

V = unreal.Vector

# Limits (cm). Set 2026-11-27 from the measured baseline after the parkour work, with a small margin. Change only with the
# developer's approval: a failing case means the motion got worse, not that the limit is wrong.
LIMITS = {
    "feet": 8.0,      # foot/ball segments (metric floor is ~5-7 cm with the bone radius)
    "legs": 9.0,      # thigh/calf
    "arms": 5.0,      # upper/lower arm
    "torso": 3.0,     # pelvis-spine, spine, neck-head
    "hand_gap_mean": 2.0,
    "hand_gap_max": 6.0,
    "mantle_end_height": 4.0,  # capsule bottom vs the top surface after the move
    "end_pop": 3.0,            # largest capsule z change in one tick during the 0.5 s after the montage ends
}
# label, block height, block depth, entry speed (0 = stand), distance from the front face when the move starts
CASES = [
    ("vault h80 stand", 80.0, 40.0, 0.0, 90.0),
    ("vault h80 walk150", 80.0, 40.0, 150.0, 120.0),
    ("mantle h110 stand", 110.0, 300.0, 0.0, 90.0),
    ("mantle h100 walk150", 100.0, 300.0, 150.0, 120.0),
    ("mantle h115 run400", 115.0, 300.0, 400.0, 120.0),
    ("mantle h150 stand", 150.0, 300.0, 0.0, 90.0),
    ("mantle h200 stand", 200.0, 300.0, 0.0, 90.0),
    ("mantle h240 stand", 240.0, 300.0, 0.0, 90.0),
    ("mantle h240 walk150", 240.0, 300.0, 150.0, 120.0),
    ("mantle h240 run400", 240.0, 300.0, 400.0, 90.0),
]
LANE_Y = -700.0
FRONT_X = 1000.0
RUNUP_X = 300.0
BP = "/Game/Blueprints/Abilities/BP_GA_Parkour"
SEGS = [("thigh_l", "calf_l", 9.0, "legs"), ("calf_l", "foot_l", 6.5, "legs"), ("foot_l", "ball_l", 5.0, "feet"),
        ("thigh_r", "calf_r", 9.0, "legs"), ("calf_r", "foot_r", 6.5, "legs"), ("foot_r", "ball_r", 5.0, "feet"),
        ("upperarm_l", "lowerarm_l", 5.0, "arms"), ("lowerarm_l", "hand_l", 4.0, "arms"),
        ("upperarm_r", "lowerarm_r", 5.0, "arms"), ("lowerarm_r", "hand_r", 4.0, "arms"),
        ("pelvis", "spine_02", 12.0, "torso"), ("spine_02", "spine_03", 12.0, "torso"), ("neck_01", "head", 9.0, "torso")]
SEG_BONES = sorted(set([x for sg in SEGS for x in sg[:2]]))
CASE_TIMEOUT = 90.0
builtins.MREG = {"t": 0.0, "phase": "boot", "i": 0, "pt": 0.0, "tries": 0, "pass": 0, "fail": 0}


def log(m):
    unreal.log("[MREG] " + m)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def signed_dist(p, box):
    x0, x1, y0, y1, z0, z1 = box
    dx = max(x0 - p.x, 0, p.x - x1)
    dy = max(y0 - p.y, 0, p.y - y1)
    dz = max(z0 - p.z, 0, p.z - z1)
    if dx or dy or dz:
        return (dx * dx + dy * dy + dz * dz) ** 0.5
    return -min(p.x - x0, x1 - p.x, p.y - y0, y1 - p.y, p.z - z0, z1 - p.z)


def hand_windows():
    """montage name -> ((left start, left end), (right start, right end)) without the ramps, from BP_GA_Parkour."""
    cdo = unreal.get_default_object(unreal.load_class(None, BP + ".BP_GA_Parkour_C"))
    out = {}
    for c in cdo.get_editor_property("clips"):
        m = c.get_editor_property("montage")
        if m is None:
            continue
        ramp = c.get_editor_property("hand_ramp")
        win = []
        for a, b in (("left_hand_start", "left_hand_end"), ("right_hand_start", "right_hand_end")):
            s_, e_ = c.get_editor_property(a), c.get_editor_property(b)
            win.append((s_ + ramp, e_ - ramp) if e_ - s_ > 2 * ramp else (0.0, 0.0))
        out.setdefault(m.get_name(), tuple(win))
    return out


def finish_case(s, label, ok, msg):
    s["pass" if ok else "fail"] += 1
    log("CASE %s | %s | %s" % ("PASS" if ok else "FAIL", label, msg))


def evaluate(s, label, top, is_mantle, pawn, mesh):
    fails = []
    worst = s["seg"]
    parts = []
    for group in ("feet", "legs", "arms", "torso"):
        e, at, bone = worst.get(group, (0.0, -1.0, "-"))
        parts.append("%s %.1f (%s@%.2f)" % (group, e, bone, at))
        if e > LIMITS[group]:
            fails.append("%s %.1f > %.1f" % (group, e, LIMITS[group]))
    win = s["windows"].get(s.get("mname", ""), None)
    hand_txt = "no hand IK window"
    if win:
        hand_parts = []
        for side, idx, (ws, we) in (("L", 1, win[0]), ("R", 2, win[1])):
            inw = [r for r in s["hrec"] if ws <= r[0] <= we]
            if we <= ws or not inw:
                continue
            gaps = [max(0.0, r[idx]) for r in inw]
            mean, mx = sum(gaps) / len(gaps), max(gaps)
            hand_parts.append("%s mean %.1f max %.1f" % (side, mean, mx))
            if mean > LIMITS["hand_gap_mean"] or mx > LIMITS["hand_gap_max"]:
                fails.append("hand %s gap mean %.1f max %.1f" % (side, mean, mx))
        hand_txt = ", ".join(hand_parts) or hand_txt
    end_txt = ""
    if is_mantle:
        half = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
        bottom = pawn.get_actor_location().z - half
        pop = s["pop"]
        end_txt = " | end: capsule bottom - top %.1f, pop %.1f" % (bottom - top, pop)
        if abs(bottom - top) > LIMITS["mantle_end_height"]:
            fails.append("ends %.1f cm off the top" % (bottom - top))
        if pop > LIMITS["end_pop"]:
            fails.append("end pop %.1f" % pop)
    msg = "%s | segments: %s | hands: %s%s" % (s.get("mname", "?"), ", ".join(parts), hand_txt, end_txt)
    if fails:
        msg += " | " + "; ".join(fails)
    finish_case(s, label, not fails, msg)


def tick(dt):
    import builtins as b
    import unreal as u
    s = b.MREG
    s["t"] += dt
    w = world()
    if s["phase"] == "boot":
        if not s.get("started"):
            s["started"] = True
            u.EditorLevelLibrary.load_level("/Game/Maps/L_Parkour")
            for _, top, depth, _, _ in CASES:
                lab = "MREG_%d_%d" % (top, depth)
                if any(a.get_actor_label() == lab for a in u.EditorLevelLibrary.get_all_level_actors()):
                    continue
                ca = u.EditorLevelLibrary.spawn_actor_from_class(u.StaticMeshActor, V(5000.0, LANE_Y, -1000.0))
                ca.set_actor_label(lab)
                cc = ca.get_component_by_class(u.StaticMeshComponent)
                cc.set_mobility(u.ComponentMobility.MOVABLE)
                cc.set_static_mesh(u.load_asset("/Engine/BasicShapes/Cube"))
                ca.set_actor_scale3d(V(depth / 100.0, 4.0, (top + 1.0) / 100.0))
            s["windows"] = hand_windows()
            u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
        pw = u.GameplayStatics.get_player_pawn(w, 0) if w is not None else None
        if pw is not None and pw.get_class().get_name().startswith("BP_PlayerCharacter") and s["t"] > 5.0:
            s["phase"] = "teleport"
        elif s["t"] > 120.0:
            log("SUMMARY could-not-run: PIE did not start with BP_PlayerCharacter")
            s["phase"] = "done"
            u.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
        return
    if s["phase"] == "done":
        return
    pawn = u.GameplayStatics.get_player_pawn(w, 0)
    asc = pawn.get_component_by_class(u.AbilitySystemComponent)
    cmc = pawn.get_editor_property("character_movement")
    mesh = pawn.get_editor_property("mesh")
    pc = u.GameplayStatics.get_player_controller(w, 0)
    s["pt"] += dt
    if s["i"] >= len(CASES):
        log("SUMMARY pass %d fail %d of %d" % (s["pass"], s["fail"], len(CASES)))
        s["phase"] = "done"
        u.SystemLibrary.execute_console_command(w, "QUIT_EDITOR")
        return
    label, top, depth, speed, dist = CASES[s["i"]]
    x1 = FRONT_X + depth
    box = (FRONT_X, x1, LANE_Y - 200.0, LANE_Y + 200.0, -1.0, top)
    is_mantle = label.startswith("mantle")
    if s["phase"] == "teleport":
        lab = "MREG_%d_%d" % (top, depth)
        for ac in u.GameplayStatics.get_all_actors_of_class(w, u.StaticMeshActor):
            al = ac.get_actor_label()
            if al.startswith("MREG_"):
                here = al == lab
                ac.set_actor_location(V(FRONT_X + depth / 2.0, LANE_Y, (top - 1.0) / 2.0) if here else V(5000.0, LANE_Y, -1000.0), False, True)
        pawn.set_actor_location(V(RUNUP_X if speed > 0 else FRONT_X - dist, LANE_Y, 100.0), False, True)
        pawn.set_actor_rotation(u.Rotator(0, 0, 0), False)
        pc.set_control_rotation(u.Rotator(0, 0, 0))
        cmc.set_editor_property("velocity", V(0, 0, 0))
        s["phase"], s["pt"] = "settle", 0.0
    elif s["phase"] == "settle" and s["pt"] > 2.0:
        if speed > 0:
            s["phase"], s["pt"] = "runup", 0.0
        else:
            pawn.set_actor_location(V(FRONT_X - dist, LANE_Y, 100.0), False, True)
            s["phase"], s["pt"] = "activate", 0.0
    elif s["phase"] == "runup":
        pawn.add_movement_input(V(1, 0, 0), min(1.0, speed / 600.0), False)
        if pawn.get_actor_location().x >= FRONT_X - dist or s["pt"] > 20.0:
            s["phase"] = "activate"
    elif s["phase"] == "activate" and (speed == 0 and s["pt"] > 1.5 or speed > 0):
        ok = asc.try_activate_ability_by_class(u.load_class(None, BP + ".BP_GA_Parkour_C"))
        if not ok:
            s["tries"] += 1
            if s["tries"] >= 2:
                finish_case(s, label, False, "the move did not start (2 tries)")
                s["i"], s["tries"] = s["i"] + 1, 0
            s["phase"] = "teleport"
            return
        s.update({"tries": 0, "phase": "watch", "flying": False, "after": 0.0, "seg": {}, "hrec": [], "pt": 0.0,
                  "ended": False, "endt": 0.0, "lastz": None, "pop": 0.0, "mname": "?"})
    elif s["phase"] == "watch":
        flying = "FLYING" in str(cmc.get_editor_property("movement_mode"))
        if flying:
            s["flying"] = True
        if s["flying"] and not flying:
            s["after"] += dt
        ai = mesh.get_anim_instance()
        mt = ai.get_current_active_montage() if ai else None
        mpos = ai.montage_get_position(mt) if mt is not None else -1.0
        if mt is not None and mpos >= 0:
            s["mname"] = mt.get_name()
            hl, hr = mesh.get_socket_location("hand_l"), mesh.get_socket_location("hand_r")
            s["hrec"].append((mpos, signed_dist(hl, box) - 4.0, signed_dist(hr, box) - 4.0))
        elif s["flying"] and not s["ended"]:
            s["ended"] = True
        if s["ended"] and s["endt"] < 0.5:
            s["endt"] += dt
            z = pawn.get_actor_location().z
            if s["lastz"] is not None:
                s["pop"] = max(s["pop"], abs(z - s["lastz"]))
            s["lastz"] = z
        if flying or (s["flying"] and s["after"] < 0.3):
            sp = {bn: mesh.get_socket_location(bn) for bn in SEG_BONES}
            for (ba, bb, rad, group) in SEGS:
                pa, pb = sp[ba], sp[bb]
                for fr in (0.0, 0.25, 0.5, 0.75, 1.0):
                    q = V(pa.x + (pb.x - pa.x) * fr, pa.y + (pb.y - pa.y) * fr, pa.z + (pb.z - pa.z) * fr)
                    e = rad - signed_dist(q, box)
                    cur = s["seg"].get(group)
                    if e > 0.0 and mpos >= 0 and (cur is None or e > cur[0]):
                        s["seg"][group] = (e, mpos, ba)
        if s["pt"] > CASE_TIMEOUT:
            finish_case(s, label, False, "timed out after %.0f s" % CASE_TIMEOUT)
            s["i"], s["phase"], s["pt"] = s["i"] + 1, "teleport", 0.0
        elif s["flying"] and s["after"] >= 1.5 and not (ai and ai.is_any_montage_playing()):
            evaluate(s, label, top, is_mantle, pawn, mesh)
            s["i"], s["phase"], s["pt"] = s["i"] + 1, "teleport", 0.0


builtins.MREG["handle"] = unreal.register_slate_post_tick_callback(tick)
