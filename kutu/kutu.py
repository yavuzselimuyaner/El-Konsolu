"""Parametric 3D-printed case for the T-Display S3 console. Writes STL files and a preview data file.

Run: python kutu.py          (needs: pip install manifold3d trimesh numpy)

Coordinates are millimetres in assembly space: origin at the perfboard's top-left corner seen from
the screen side, x to the right, y towards the player, z up, perfboard top surface at z=0.
"""
import base64
import json
from pathlib import Path

import numpy as np
import trimesh
from manifold3d import Manifold

OUT = Path(__file__).parent / "stl"
SEGMENTS = 64

# ---- Measure these on the real parts before printing ----
PERF_W, PERF_H, PERF_T = 70.0, 90.0, 1.6
PERF_HOLE_INSET = 2.5    # Corner mounting hole, from both edges
TACT_H = 5.0             # Top of the 6x6 switch stem, above the perfboard
SOCKET_H = 8.5           # Female header height: T-Display PCB rests on this
LCD_TOP = 12.9           # Glass of the display, above the perfboard
SCREEN_FROM_END = 27.7   # Centre of the visible screen, from the antenna end of the T-Display
SCREEN_W, SCREEN_H = 44.0, 24.0
USB_Z = 7.2              # Centre of the USB-C mouth, above the perfboard
BOOT_FROM_USB_END = 3.5  # Board buttons, from the USB end of the T-Display
BOOT_FROM_CENTRE = 8.3   # Board buttons, each side of the T-Display's long centre line
BOARD_BTN_TOP = 11.8     # Top of the board's own buttons, above the perfboard

# ---- Case ----
WALL = 2.0
GAP = 0.5           # Perfboard to inner wall
PLATE = 2.0         # Front and back plate
FRONT_INNER_Z = 13.5
BACK_DEPTH = 9.0    # Space under the perfboard
CORNER_R = 7.0
INNER_R = 0.4       # The perfboard has square corners
POST_D = 7.0        # Screw posts, must overlap the wall
HOLE_CLEARANCE = 0.35
SCREW_CLEAR, SCREW_PILOT, SCREW_HEAD = 2.4, 1.7, 4.4  # M2 screws
JACK_X, JACK_Z, JACK_D = 52.0, -6.6, 7.0
SD_X, SD_W, SD_H = 20.0, 27.0, 2.8
SWITCH_Y, SWITCH_W, SWITCH_H = 30.0, 9.0, 3.5

# ---- Perfboard grid: the 24x30 holes are centred on the board ----
GRID_X0 = (PERF_W - 23 * 2.54) / 2
GRID_Y0 = (PERF_H - 29 * 2.54) / 2


def hx(col):
    return GRID_X0 + (col - 1) * 2.54


def hy(row):
    return GRID_Y0 + (row - 1) * 2.54


HEADER_FIRST_COL, HEADER_ROWS = 5, (4, 13)
PIN1_FROM_END = 8.7  # First header pin, from the antenna end of the T-Display
BOARD_L, BOARD_W = 60.8, 25.5
BOARD_X = hx(HEADER_FIRST_COL) - PIN1_FROM_END
BOARD_Y = (hy(HEADER_ROWS[0]) + hy(HEADER_ROWS[1])) / 2
USB_END = BOARD_X + BOARD_L

DPAD = (hx(5), hy(20.5))
DPAD_TACT = 3 * 2.54
BTN_A = (hx(21), hy(19.5))
BTN_B = (hx(16), hy(21.5))
BTN_SELECT = (hx(10), hy(27.5))
BTN_START = (hx(15), hy(27.5))
BTN_IO14 = (USB_END - BOOT_FROM_USB_END, BOARD_Y - BOOT_FROM_CENTRE)
BTN_BOOT = (USB_END - BOOT_FROM_USB_END, BOARD_Y + BOOT_FROM_CENTRE)
SCREEN = (BOARD_X + SCREEN_FROM_END, BOARD_Y)

FRONT_TOP = FRONT_INNER_Z + PLATE
BACK_FLOOR = -PERF_T - BACK_DEPTH
BACK_BOTTOM = BACK_FLOOR - PLATE
OUT_X0, OUT_X1 = -GAP - WALL, PERF_W + GAP + WALL
OUT_Y0, OUT_Y1 = -GAP - WALL, PERF_H + GAP + WALL
IN_X0, IN_X1 = -GAP, PERF_W + GAP
IN_Y0, IN_Y1 = -GAP, PERF_H + GAP
SCREWS = [(x, y) for x in (PERF_HOLE_INSET, PERF_W - PERF_HOLE_INSET) for y in (PERF_HOLE_INSET, PERF_H - PERF_HOLE_INSET)]

# Key shapes
DPAD_ARM_W, DPAD_SPAN = 9.5, 25.0
KEY_PROUD = 2.2      # Height of the keys above the front plate
FLANGE = 1.4         # 45 degree retaining cone under the plate
ROUND_D = 10.0


def cyl(x, y, z0, z1, d, d1=None):
    m = Manifold.cylinder(z1 - z0, d / 2, (d1 if d1 else d) / 2, SEGMENTS)
    return m.translate([x, y, z0])


def box(x0, y0, x1, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def rprism(x0, y0, x1, y1, z0, z1, r):
    r = min(r, (x1 - x0) / 2 - 1e-3, (y1 - y0) / 2 - 1e-3)
    return Manifold.batch_hull([cyl(x, y, z0, z1, 2 * r) for x in (x0 + r, x1 - r) for y in (y0 + r, y1 - r)])


def centred(c, w, h, z0, z1, r):
    return rprism(c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2, z0, z1, r)


def sloped(c, w0, h0, w1, h1, z0, z1, r):
    """Prism going from size (w0, h0) at z0 to (w1, h1) at z1."""
    return Manifold.batch_hull([centred(c, w0, h0, z0, z0 + 0.01, r), centred(c, w1, h1, z1 - 0.01, z1, r)])


def cross(c, arm, span, z0, z1, r):
    return centred(c, span, arm, z0, z1, r) + centred(c, arm, span, z0, z1, r)


def cross_sloped(c, a0, s0, a1, s1, z0, z1, r):
    return sloped(c, s0, a0, s1, a1, z0, z1, r) + sloped(c, a0, s0, a1, s1, z0, z1, r)


def make_front():
    outer = rprism(OUT_X0, OUT_Y0, OUT_X1, OUT_Y1, -PERF_T, FRONT_TOP, CORNER_R)
    inner = rprism(IN_X0, IN_Y0, IN_X1, IN_Y1, -PERF_T - 1, FRONT_INNER_Z, INNER_R)
    shell = outer - inner

    for x, y in SCREWS:
        shell += cyl(x, y, 0, FRONT_INNER_Z + 0.5, POST_D)
    shell = shell ^ outer

    cuts = [sloped(SCREEN, SCREEN_W, SCREEN_H, SCREEN_W + 3.2, SCREEN_H + 3.2, FRONT_TOP - 1.6, FRONT_TOP + 0.01, 1.0),
            centred(SCREEN, SCREEN_W, SCREEN_H, FRONT_INNER_Z - 1, FRONT_TOP, 1.0)]

    c = HOLE_CLEARANCE
    cuts.append(cross(DPAD, DPAD_ARM_W + 2 * c, DPAD_SPAN + 2 * c, FRONT_INNER_Z - 1, FRONT_TOP + 1, 1.5))
    for pos in (BTN_A, BTN_B):
        cuts.append(cyl(*pos, FRONT_INNER_Z - 1, FRONT_TOP + 1, ROUND_D + 2 * c))
    for pos in (BTN_SELECT, BTN_START):
        cuts.append(centred(pos, 8.4 + 2 * c, 4.2 + 2 * c, FRONT_INNER_Z - 1, FRONT_TOP + 1, 2.4))
    for pos in (BTN_IO14, BTN_BOOT):
        cuts.append(cyl(*pos, FRONT_INNER_Z - 1, FRONT_TOP + 1, 4.0 + 2 * c))

    # USB-C: wide enough for the moulded plug body to enter the wall
    cuts.append(box(OUT_X1 - WALL - 1, BOARD_Y - 7, OUT_X1 + 1, BOARD_Y + 7, USB_Z - 4.2, USB_Z + 4.2))

    for x, y in SCREWS:
        cuts.append(cyl(x, y, -0.1, 9.0, SCREW_PILOT))

    return shell - sum(cuts[1:], cuts[0])


def make_back():
    outer = rprism(OUT_X0, OUT_Y0, OUT_X1, OUT_Y1, BACK_BOTTOM, -PERF_T, CORNER_R)
    inner = rprism(IN_X0, IN_Y0, IN_X1, IN_Y1, BACK_FLOOR, -PERF_T + 1, INNER_R)
    shell = outer - inner

    for x, y in SCREWS:
        shell += cyl(x, y, BACK_FLOOR - 0.5, -PERF_T, POST_D)
    shell = shell ^ outer

    cuts = []
    for x, y in SCREWS:
        cuts.append(cyl(x, y, BACK_BOTTOM - 1, -PERF_T + 1, SCREW_CLEAR))
        cuts.append(cyl(x, y, BACK_BOTTOM - 1, BACK_BOTTOM + 1.8, SCREW_HEAD))

    # Bottom wall: the SD adapter lies on the floor, the DAC's jack sits next to it
    cuts.append(box(SD_X - SD_W / 2, IN_Y1 - 1, SD_X + SD_W / 2, OUT_Y1 + 1, BACK_FLOOR, BACK_FLOOR + SD_H))
    jack = Manifold.cylinder(WALL + 2, JACK_D / 2, JACK_D / 2, SEGMENTS).rotate([-90, 0, 0])
    cuts.append(jack.translate([JACK_X, IN_Y1 - 1, JACK_Z]))

    # Left wall: power switch
    cuts.append(box(OUT_X0 - 1, SWITCH_Y - SWITCH_W / 2, IN_X0 + 1, SWITCH_Y + SWITCH_W / 2, -6.5 - SWITCH_H / 2, -6.5 + SWITCH_H / 2))

    return shell - sum(cuts[1:], cuts[0])


def key_top_z():
    return FRONT_TOP + KEY_PROUD


def flange_z():
    # The cone starts just under the plate, then a short flat rim
    top = FRONT_INNER_Z - 0.2
    return top, top - FLANGE, top - FLANGE - 0.8


def make_dpad():
    z1, z2, z3 = flange_z()
    a, s = DPAD_ARM_W, DPAD_SPAN
    top = key_top_z()
    body = cross(DPAD, a, s, z1, top - 0.6, 1.5) + cross_sloped(DPAD, a, s, a - 1.2, s - 1.2, top - 0.6, top, 1.2)
    cone = cross_sloped(DPAD, a + 2 * FLANGE, s + 2 * FLANGE, a, s, z2, z1, 1.5)
    rim = cross(DPAD, a + 2 * FLANGE, s + 2 * FLANGE, z3, z2, 1.5)

    # Rocks on the centre post, the other four posts land on the switches
    stems = [cyl(*DPAD, 0.1, z3 + 0.1, 5.0)]
    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        stems.append(cyl(DPAD[0] + dx * DPAD_TACT, DPAD[1] + dy * DPAD_TACT, TACT_H + 0.1, z3 + 0.1, 5.0))
    return body + cone + rim + sum(stems[1:], stems[0])


def make_round(pos):
    z1, z2, z3 = flange_z()
    d = ROUND_D
    top = key_top_z()
    return (cyl(*pos, z1, top - 0.8, d) + cyl(*pos, top - 0.8, top, d, d - 1.6) +
            cyl(*pos, z2, z1, d + 2 * FLANGE, d) + cyl(*pos, z3, z2, d + 2 * FLANGE) +
            cyl(*pos, TACT_H + 0.1, z3 + 0.1, 6.0))


def make_pill(pos):
    z1, z2, z3 = flange_z()
    w, h = 8.4, 4.2
    top = FRONT_TOP + 1.2
    return (centred(pos, w, h, z1, top, 2.0) +
            sloped(pos, w + 2 * FLANGE, h + 2 * FLANGE, w, h, z2, z1, 2.0) +
            centred(pos, w + 2 * FLANGE, h + 2 * FLANGE, z3, z2, 2.0) +
            cyl(*pos, TACT_H + 0.1, z3 + 0.1, 4.0))


def make_mini(pos):
    z1 = FRONT_INNER_Z - 0.2
    return (cyl(*pos, z1, FRONT_TOP + 1.0, 4.0) + cyl(*pos, z1 - 0.8, z1, 5.6, 4.0) +
            cyl(*pos, z1 - 1.4, z1 - 0.8, 5.6) + cyl(*pos, BOARD_BTN_TOP + 0.1, z1 - 1.3, 3.0))


def make_components():
    """Rough stand-ins for the electronics, only used for the fit check and the preview."""
    parts = {"pertinaks": box(0, 0, PERF_W, PERF_H, -PERF_T, 0)}
    parts["soketler"] = (box(hx(5) - 1.3, hy(4) - 1.3, hx(16) + 1.3, hy(4) + 1.3, 0, SOCKET_H) +
                         box(hx(5) - 1.3, hy(13) - 1.3, hx(16) + 1.3, hy(13) + 1.3, 0, SOCKET_H))
    pcb = box(BOARD_X, BOARD_Y - BOARD_W / 2, USB_END, BOARD_Y + BOARD_W / 2, SOCKET_H, SOCKET_H + 1.6)
    glass = box(BOARD_X + 4.8, BOARD_Y - 11.5, BOARD_X + 51.2, BOARD_Y + 11.5, SOCKET_H + 1.6, LCD_TOP)
    usb = box(USB_END - 7, BOARD_Y - 4.5, USB_END + 0.4, BOARD_Y + 4.5, USB_Z - 1.6, USB_Z + 1.6)
    parts["t_display"] = pcb + glass + usb

    tacts = [(DPAD[0] + dx * DPAD_TACT, DPAD[1] + dy * DPAD_TACT) for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))]
    tacts += [BTN_A, BTN_B, BTN_SELECT, BTN_START]
    bodies = [centred(p, 6.2, 6.2, 0, 3.5, 0.3) + cyl(*p, 3.5, TACT_H, 3.5) for p in tacts]
    parts["tuslar"] = sum(bodies[1:], bodies[0])

    parts["pil"] = box(10, 3, 60, 37, BACK_FLOOR, BACK_FLOOR + 6)
    parts["sd_adaptor"] = box(SD_X - 12, 58, SD_X + 12, PERF_H + 3.2, BACK_FLOOR, BACK_FLOOR + 2.1)
    parts["dac"] = box(JACK_X - 7, 58, JACK_X + 7, PERF_H, BACK_FLOOR, BACK_FLOOR + 1.6)
    return parts


def real_world(m):
    """The layout above has y growing towards the player, which is a mirrored axis. Undo it."""
    return m.mirror([0, 1, 0])


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)


def print_pose(m, flip):
    """Lay a part on the bed, flipped parts print with their visible face down."""
    if flip:
        m = m.rotate([180, 0, 0])
    bounds = m.bounding_box()
    return m.translate([-bounds[0], -bounds[1], -bounds[2]])


def save(name, m, flip=False):
    assert m.status().name == "NoError", name
    OUT.mkdir(exist_ok=True)
    to_trimesh(print_pose(real_world(m), flip)).export(OUT / f"{name}.stl")


def check_fit(moving, fixed):
    """Prints every overlap between parts, there should be none."""
    names = list(moving) + list(fixed)
    solids = {**moving, **fixed}
    overlaps = []
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            if a in fixed and b in fixed:
                continue
            volume = (solids[a] ^ solids[b]).volume()
            if volume > 0.05:
                overlaps.append((a, b, round(volume, 2)))
    return overlaps


def main():
    front, back, dpad = make_front(), make_back(), make_dpad()
    keys = {"tus_a": make_round(BTN_A), "tus_b": make_round(BTN_B),
            "tus_select": make_pill(BTN_SELECT), "tus_start": make_pill(BTN_START),
            "tus_io14": make_mini(BTN_IO14), "tus_boot": make_mini(BTN_BOOT)}
    components = make_components()
    shells = {"on_kapak": front, "arka_kapak": back, "dpad": dpad, **keys}

    overlaps = check_fit(shells, components)
    print("Overlaps:", overlaps or "none")
    print(f"Outer size: {OUT_X1 - OUT_X0:.1f} x {OUT_Y1 - OUT_Y0:.1f} x {FRONT_TOP - BACK_BOTTOM:.1f} mm")
    print(f"USB mouth to outer wall: {OUT_X1 - USB_END:.1f} mm")
    print("Screen window x:", round(SCREEN[0] - SCREEN_W / 2, 1), "to", round(SCREEN[0] + SCREEN_W / 2, 1))

    save("on_kapak", front, flip=True)
    save("arka_kapak", back)
    save("dpad", dpad, flip=True)
    save("tus_a_b", keys["tus_a"], flip=True)
    save("tus_start_select", keys["tus_select"], flip=True)
    save("tus_boot_io14", keys["tus_io14"], flip=True)
    test_plate = front ^ box(OUT_X0 - 1, OUT_Y0 - 1, OUT_X1 + 1, 50, FRONT_INNER_Z, FRONT_TOP + 1)
    save("deneme_plakasi", test_plate, flip=True)

    preview = {}
    groups = {**shells, **components}
    for name, solid in groups.items():
        mesh = to_trimesh(real_world(solid))
        positions = mesh.vertices[mesh.faces].astype(np.float32).reshape(-1)
        preview[name] = base64.b64encode(positions.tobytes()).decode()
    here = Path(__file__).parent
    page = (here / "sablon.html").read_text(encoding="utf-8").replace("/*DATA*/null", json.dumps(preview))
    (here / "kutu-onizleme.html").write_text(page, encoding="utf-8", newline="\n")

    for stl in sorted(OUT.glob("*.stl")):
        mesh = trimesh.load(stl)
        size = mesh.bounds[1] - mesh.bounds[0]
        print(f"{stl.name}: {size[0]:.1f} x {size[1]:.1f} x {size[2]:.1f} mm, watertight={mesh.is_watertight}, {mesh.volume / 1000:.1f} cm3")


if __name__ == "__main__":
    main()
