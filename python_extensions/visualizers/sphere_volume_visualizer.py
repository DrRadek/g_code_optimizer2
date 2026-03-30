"""
Sphere visualizer - consumes (direction, value) points from shared memory.
C++ pushes vec3 direction + float value into a ring buffer.
Python reads at ~60fps, updates Open3D point cloud and camera.

Shared memory layout (matches ShmData in vis_test.cpp):
  header.write_idx    uint32  - C++ writes here (next slot to write)
  header.read_idx     uint32  - Python writes here (next slot to read)
  header.cam_front[3] float   - camera front vector
  header.cam_up[3]    float   - camera up vector
  entries[RING_SIZE]          - ring buffer of (x, y, z, value)

C++ waits (spins) when (write_idx - read_idx) >= RING_SIZE.
Python advances read_idx after consuming.

Shared memory is created by C++ - this script only attaches.

Platform notes:
  Windows : uses kernel32 OpenFileMappingA / MapViewOfFile
  Linux   : opens /dev/shm/<n>

Configuration is via constants at the top of the file:
  INVERT     - False: green=0 red=1 / True: green=1 red=0, uses (1-value) distance
  COLOR_LOW  - RGB for value=0
  COLOR_HIGH - RGB for value=1
"""

import argparse
import ctypes
import ctypes.wintypes
import time
import math
import sys
import numpy as np
import open3d as o3d


# ── configuration ────────────────────────────────────────────────────────────

RING_SIZE         = 32        # number of entries in the ring buffer - must match C++
MAX_DISPLAY       = 100_000     # maximum points kept for display
FRAME_TIME        = 1.0 / 60.0  # target ~60fps
CAMERA_ZOOM       = 0.7         # 0.7 is just enough to fit the unit sphere

COLOR_LOW  = [0.0, 1.0, 0.0]   # green  (value = 0)
COLOR_HIGH = [1.0, 0.0, 0.0]   # red    (value = 1)

# Invert mode:
#   False - use value as radial distance, green=0 red=1
#   True  - use (1 - value) as radial distance, green=1 red=0 (colors also swap)
INVERT = False

SHM_NAME         = "g_code_optimizer2_volume_forward"
POINT_SIZE       = 5.0
BACKGROUND_COLOR = [0.05, 0.05, 0.08]

# Center sphere: visible reference marker at the origin.
# Shrinks to match the closest data point distance, down to CENTER_SPHERE_MIN_RADIUS.
CENTER_SPHERE_RADIUS     = 0.08   # initial and maximum radius
CENTER_SPHERE_MIN_RADIUS = 0.01   # floor - never shrinks below this
CENTER_SPHERE_COLOR      = [0.9, 0.9, 0.9]


# ── shared memory structs (must match C++ ShmData layout) ────────────────────

class ShmHeader(ctypes.Structure):
    _fields_ = [
        ("write_idx",  ctypes.c_uint32),
        ("read_idx",   ctypes.c_uint32),
        ("cam_front",  ctypes.c_float * 3),
        ("cam_up",     ctypes.c_float * 3),
        ("point_size", ctypes.c_float),
        ("clear",      ctypes.c_uint32),
    ]

class ShmEntry(ctypes.Structure):
    _fields_ = [
        ("x",     ctypes.c_float),
        ("y",     ctypes.c_float),
        ("z",     ctypes.c_float),
        ("value", ctypes.c_float),
    ]

class ShmData(ctypes.Structure):
    _fields_ = [
        ("header",  ShmHeader),
        ("entries", ShmEntry * RING_SIZE),
    ]


# ── helpers ───────────────────────────────────────────────────────────────────

def normalize(v):
    n = np.linalg.norm(v)
    return v if n < 1e-9 else v / n

def value_to_color(t, color_low, color_high):
    """Linearly interpolate between color_low (t=0) and color_high (t=1)."""
    return [
        color_low[0] + (color_high[0] - color_low[0]) * t,
        color_low[1] + (color_high[1] - color_low[1]) * t,
        color_low[2] + (color_high[2] - color_low[2]) * t,
    ]


# ── main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Sphere point cloud visualizer")
    parser.add_argument("--shm-name",   default=SHM_NAME,
                        help="Shared memory block name")
    parser.add_argument("--max-points", type=int,   default=MAX_DISPLAY)
    parser.add_argument("--point-size", type=float, default=POINT_SIZE)
    args = parser.parse_args()

    if INVERT:
        color_low  = COLOR_HIGH
        color_high = COLOR_LOW
    else:
        color_low  = COLOR_LOW
        color_high = COLOR_HIGH

    # Attach to shared memory (C++ must have created it already)
    shm_size = ctypes.sizeof(ShmData)

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenFileMappingA.restype  = ctypes.wintypes.HANDLE
    kernel32.OpenFileMappingA.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.BOOL, ctypes.c_char_p]
    kernel32.MapViewOfFile.restype     = ctypes.c_void_p
    kernel32.MapViewOfFile.argtypes    = [ctypes.wintypes.HANDLE, ctypes.wintypes.DWORD,
                                          ctypes.wintypes.DWORD, ctypes.wintypes.DWORD,
                                          ctypes.c_size_t]
    kernel32.UnmapViewOfFile.argtypes  = [ctypes.c_void_p]
    kernel32.CloseHandle.argtypes      = [ctypes.wintypes.HANDLE]

    FILE_MAP_READ       = 0x0004
    FILE_MAP_WRITE      = 0x0002
    FILE_MAP_ALL_ACCESS = FILE_MAP_READ | FILE_MAP_WRITE

    hMap = kernel32.OpenFileMappingA(FILE_MAP_ALL_ACCESS, False, args.shm_name.encode())
    if not hMap:
        err = ctypes.get_last_error()
        print(f"[visualizer] OpenFileMappingA failed (error {err}) for '{args.shm_name}'.",
              file=sys.stderr)
        print("  Make sure C++ has created the mapping before starting this script.",
              file=sys.stderr)
        sys.exit(1)

    ptr = kernel32.MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, shm_size)
    if not ptr:
        err = ctypes.get_last_error()
        print(f"[visualizer] MapViewOfFile failed (error {err}), shm_size={shm_size}.",
              file=sys.stderr)
        kernel32.CloseHandle(hMap)
        sys.exit(1)

    print("[visualizer] Shared memory attached.")
    data = ShmData.from_address(ptr)

    # Pre-allocated ring buffers - avoids per-frame allocation
    max_pts   = args.max_points
    pts_buf   = np.zeros((max_pts, 3), dtype=np.float64)
    col_buf   = np.zeros((max_pts, 3), dtype=np.float64)
    n_pts     = 0
    write_pos = 0

    # Center sphere state
    sphere_radius    = CENTER_SPHERE_RADIUS
    sphere_dirty     = True  # rebuild mesh when radius changes

    # Open3D setup
    vis = o3d.visualization.Visualizer()
    vis.create_window(window_name="Volume Visualizer", width=768, height=768)

    # pcd: data points - seeded with invisible unit cube corners for stable bbox
    bg = BACKGROUND_COLOR
    corners = np.array([
        [ 1,  1,  1], [ 1,  1, -1], [ 1, -1,  1], [ 1, -1, -1],
        [-1,  1,  1], [-1,  1, -1], [-1, -1,  1], [-1, -1, -1],
    ], dtype=np.float64)
    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(corners)
    pcd.colors = o3d.utility.Vector3dVector(np.tile(bg, (8, 1)))
    vis.add_geometry(pcd)

    # Center sphere mesh
    sphere_mesh = o3d.geometry.TriangleMesh.create_sphere(radius=sphere_radius,
                                                           resolution=10)
    sphere_mesh.compute_vertex_normals()
    sphere_mesh.paint_uniform_color(CENTER_SPHERE_COLOR)
    vis.add_geometry(sphere_mesh)

    opt = vis.get_render_option()
    opt.point_size       = args.point_size
    opt.background_color = np.array(bg)

    view = vis.get_view_control()
    vis.reset_view_point(True)
    view.set_zoom(CAMERA_ZOOM)

    geometry_dirty  = False
    last_cam_front  = None
    last_cam_up     = None
    last_point_size = args.point_size

    invert_str = " (inverted)" if INVERT else ""
    print(f"[visualizer] Running{invert_str}. "
          f"max_points={args.max_points}, "
          f"point_size={args.point_size}")

    try:
        while True:
            frame_start = time.perf_counter()

            # ── consume shared memory ─────────────────────────────────────────

            write_idx = data.header.write_idx
            read_idx  = data.header.read_idx

            cam_front = np.array(data.header.cam_front, dtype=np.float64)
            cam_up    = np.array(data.header.cam_up,    dtype=np.float64)

            # ── clear flag ────────────────────────────────────────────────────

            if data.header.clear:
                n_pts     = 0
                write_pos = 0
                sphere_radius = CENTER_SPHERE_RADIUS
                sphere_dirty  = True
                geometry_dirty = True
                data.header.clear = 0

            # ── point size ────────────────────────────────────────────────────

            shm_point_size = data.header.point_size
            if shm_point_size > 0.0 and abs(shm_point_size - last_point_size) > 0.01:
                opt.point_size  = shm_point_size
                last_point_size = shm_point_size

            pending = (write_idx - read_idx) & 0xFFFFFFFF
            pending = min(pending, RING_SIZE)

            for _ in range(pending):
                e        = data.entries[read_idx % RING_SIZE]
                read_idx = (read_idx + 1) & 0xFFFFFFFF

                v            = float(np.clip(e.value, 0.0, 1.0))
                t            = (1.0 - v) if INVERT else v
                display_dist = (1.0 - v) if INVERT else v

                dir_norm = normalize(np.array([e.x, e.y, e.z], dtype=np.float64))
                pts_buf[write_pos] = dir_norm * display_dist
                col_buf[write_pos] = value_to_color(t, color_low, color_high)
                write_pos = (write_pos + 1) % max_pts
                if n_pts < max_pts:
                    n_pts += 1

                # Shrink center sphere to match closest point
                if display_dist > 1e-6:
                    new_radius = max(CENTER_SPHERE_MIN_RADIUS,
                                     min(sphere_radius, display_dist * 0.5))
                    if new_radius < sphere_radius - 1e-6:
                        sphere_radius = new_radius
                        sphere_dirty  = True

                geometry_dirty = True

            data.header.read_idx = read_idx

            # ── rebuild point cloud if changed ────────────────────────────────

            if geometry_dirty:
                if n_pts > 0:
                    pcd.points = o3d.utility.Vector3dVector(pts_buf[:n_pts])
                    pcd.colors = o3d.utility.Vector3dVector(col_buf[:n_pts])
                else:
                    # Restore invisible corners after clear
                    pcd.points = o3d.utility.Vector3dVector(corners)
                    pcd.colors = o3d.utility.Vector3dVector(np.tile(bg, (8, 1)))
                vis.update_geometry(pcd)
                geometry_dirty = False

            # ── rebuild center sphere if radius changed ───────────────────────

            if sphere_dirty:
                vis.remove_geometry(sphere_mesh, reset_bounding_box=False)
                sphere_mesh = o3d.geometry.TriangleMesh.create_sphere(
                    radius=sphere_radius, resolution=10)
                sphere_mesh.compute_vertex_normals()
                sphere_mesh.paint_uniform_color(CENTER_SPHERE_COLOR)
                vis.add_geometry(sphere_mesh, reset_bounding_box=False)
                sphere_dirty = False

            # ── update camera ─────────────────────────────────────────────────

            front_n = normalize(cam_front)
            up_n    = normalize(cam_up)
            if np.linalg.norm(cam_front) > 0.5 and np.linalg.norm(cam_up) > 0.5:
                cam_changed = (last_cam_up is None or
                               not np.allclose(front_n, last_cam_front, atol=1e-4) or
                               not np.allclose(up_n,    last_cam_up,    atol=1e-4))
                if cam_changed:
                    view.set_lookat([0.0, 0.0, 0.0])
                    view.set_front(front_n.tolist())
                    view.set_up(up_n.tolist())
                    last_cam_front = front_n
                    last_cam_up    = up_n

            # ── tick Open3D event loop ────────────────────────────────────────

            if not vis.poll_events():
                break
            vis.update_renderer()

            # ── frame timing ─────────────────────────────────────────────────

            elapsed   = time.perf_counter() - frame_start
            sleep_for = FRAME_TIME - elapsed
            if sleep_for > 0:
                time.sleep(sleep_for)

    except KeyboardInterrupt:
        print("[visualizer] Interrupted.")
    finally:
        vis.destroy_window()
        kernel32.UnmapViewOfFile(ptr)
        kernel32.CloseHandle(hMap)
        print("[visualizer] Clean exit.")


if __name__ == "__main__":
    main()