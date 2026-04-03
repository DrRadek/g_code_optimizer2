

from UM.Extension import Extension
from UM.Logger import Logger
from UM.Scene.SceneNode import SceneNode
from UM.Scene.Selection import Selection
from UM.Math.Quaternion import Quaternion

import subprocess
import tempfile
import os
import json

from functools import partial

class GCodeOptimizer2Extension(Extension):
    def __init__(self):
        super().__init__()
        Logger.log("d", "GCodeOptimizer2 Extension loaded")

        # Menu location
        self.setMenuName("g_code_optimizer2")

        # Available algorithms exposed as menu items
        algos = [
            ("stochastic", "stochastic (faster)"),
            ("deterministic", "deterministic (slower)"),
            ("", "UI mode")
        ]

        # Add the menu items
        for algo_name, algo_description in algos:
            self.addMenuItem(f"Optimize orientation -> {algo_description}", partial(self.optimize_orientation, algo_name))

    def optimize_orientation(self, algo_name : str):
        self.settings = self.get_plugin_settings()

        # Read config
        self.optimizer_exe_path = self.settings["g_code_optimizer2_exe_path"]
        self.algo_name = algo_name

        # Get selected objects
        selection = Selection.getAllSelectedObjects()

        processes = []
        # Start processes
        for node in selection:
            # Temporary files to communicate with C++
            # TODO: potential performance improvement: use shared memory instead
            v_fd, v_path = tempfile.mkstemp(suffix=".verts.bin")
            i_fd, i_path = tempfile.mkstemp(suffix=".inds.bin")
            q_fd, q_path = tempfile.mkstemp(suffix=".rotated.quat")

            os.close(v_fd)
            os.close(i_fd)
            os.close(q_fd)

            # Optimize each of selected object separately
            proc = self.start_process_for_node(node, v_path, i_path, q_path)
            processes.append((proc, node, v_path, i_path, q_path))

        # Wait for processes to finish
        for proc, node, v_path, i_path, q_path in processes:
            # finish
            self.finish_process_for_node(proc, node, q_path)

            # cleanup
            os.remove(v_path)
            os.remove(i_path)
            os.remove(q_path)

    def start_process_for_node(self, node: SceneNode, v_path :str, i_path : str, q_path : str):
        proc = None

        try:
            # Build arguments for C++ optimizer
            indices_were_saved = self.write_mesh_arrays(node, v_path, i_path)

            input_array = [self.optimizer_exe_path]
            input_array.append('--vertsFile')
            input_array.append(v_path)

            if indices_were_saved:
                input_array.append('--indsFile')
                input_array.append(i_path)

            input_array.append('--outputQuat')
            input_array.append(q_path)

            if self.algo_name != "":
                # Not UI
                input_array.append("--headless")
                input_array.append("--algorithm")
                input_array.append(self.algo_name)

            Logger.log("d", f"Inputs: {input_array}")

            # Call the C++ optimizer and wait
            proc = subprocess.Popen(input_array)
        finally:
            return proc

    def finish_process_for_node(self, proc: subprocess.Popen, node: SceneNode, q_path : str):
        try:
            if proc == None:
                return

            # Wait for the process
            if proc.wait() != 0:
                Logger.logException("d", f"g_code_optimizer2 failed")
                return

            # Apply the optimized orientation
            with open(q_path, "r") as f:
                quat_data = f.read().strip().split(' ')
                x = float(quat_data[0])
                y = float(quat_data[1])
                z = float(quat_data[2])
                w = float(quat_data[3])
                quat : Quaternion = Quaternion(x, z, y, w) # Remapping, C++ uses different up axis
                node.setOrientation(quat, transform_space=SceneNode.TransformSpace.World)
        finally:
            return

    # Returns: True if indices were written, False otherwise
    def write_mesh_arrays(self, node : SceneNode, verts_path : str, inds_path: str):
        mesh = node.getMeshData()
        if not mesh:
            raise ValueError("Node has no mesh data")

        verts_bytes = mesh.getVerticesAsByteArray()
        inds_bytes = mesh.getIndicesAsByteArray()

        with open(verts_path, "wb") as fv:
            fv.write(verts_bytes)

        if inds_bytes is not None:
            with open(inds_path, "wb") as fi:
                fi.write(inds_bytes)
            return True
        return False

    # Get plugin settings
    def get_plugin_settings(self) -> dict:
        plugin_dir = os.path.dirname(os.path.abspath(__file__))
        plugin_json_path = os.path.join(plugin_dir, "settings.json")

        with open(plugin_json_path, "r", encoding="utf-8") as f:
            return json.load(f)
