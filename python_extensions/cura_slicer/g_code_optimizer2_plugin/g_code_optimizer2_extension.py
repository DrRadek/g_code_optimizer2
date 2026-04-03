

from UM.Extension import Extension
from UM.Logger import Logger
from UM.Scene.SceneNode import SceneNode
from UM.Scene.Selection import Selection
from UM.Math.Quaternion import Quaternion

import subprocess
import tempfile
import os
import json

class GCodeOptimizer2Extension(Extension):
    def __init__(self):
        super().__init__()
        Logger.log("d", "GCodeOptimizer2 Extension loaded")

        # Menu location
        self.setMenuName("g_code_optimizer2")
        self.addMenuItem("Optimize orientation", self.optimize_orientation)

    def optimize_orientation(self):
        # Read config
        self.optimizer_exe_path = self.get_plugin_settings()["g_code_optimizer2_exe_path"]

        # Get selected objects
        selection = Selection.getAllSelectedObjects()

        # TODO: potential performance improvement: optimize for all at once
        for node in selection:
            # Optimize each of selected object separately
            self.run_process_for_node(node)

    def run_process_for_node(self, node: SceneNode):
        # Temporary files to communicate with C++
        # TODO: potential performance improvement: use shared memory instead
        v_fd, v_path = tempfile.mkstemp(suffix=".verts.bin")
        i_fd, i_path = tempfile.mkstemp(suffix=".inds.bin")
        q_fd, q_path = tempfile.mkstemp(suffix=".rotated.quat")

        os.close(v_fd)
        os.close(i_fd)
        os.close(q_fd)

        try:
            # Build arguments for C++ optimizer
            indices_were_saved = self.write_mesh_arrays(node, v_path, i_path)

            # TODO: change to your actual exe path
            input_array = [self.optimizer_exe_path]
            input_array.append('--vertsfile')
            input_array.append(v_path)

            if indices_were_saved:
                input_array.append('--indsfile')
                input_array.append(i_path)

            input_array.append('--outputquatfile')
            input_array.append(q_path)

            # Call the C++ optimizer and wait
            proc = subprocess.Popen(input_array)
            proc.wait()

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
            os.remove(v_path)
            os.remove(i_path)

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