import os
import sys
from typing import Union, List
from enum import Enum
import matplotlib

from utils.database import load_from_database
matplotlib.use('qtagg', force=True)
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from PySide6.QtWidgets import QVBoxLayout, QProgressDialog, QHBoxLayout, QListWidget, QSplitter, QWidget
from PySide6.QtCore import Qt
from utils import global_vars
import time
from utils.pallet_data import *
from utils.robot_control import load_wordlist
logger = global_vars.logger

class MatplotlibCanvas(FigureCanvas):
    def __init__(self, parent=None, width=6, height=4, dpi=100):
        fig = Figure(figsize=(width, height), dpi=dpi)
        self.ax = fig.add_subplot(111, projection='3d')
        super().__init__(fig)
        self.setParent(parent)
        self.ax.mouse_init(rotate_btn=None, zoom_btn=None)

def initialize_3d_view(frame):
    """Initialize the 3D view in the given frame.

    Args:
        frame (QFrame): The frame to initialize the 3D view in.
    """
    # Create canvas for 3D visualization
    canvas = MatplotlibCanvas(frame, width=12, height=9, dpi=100)
    
    # Set up layout
    layout = QVBoxLayout(frame)
    layout.addWidget(canvas)
    
    return canvas

def clear_canvas(canvas):
    """Clear the matplotlib canvas.

    Args:
        canvas (MatplotlibCanvas): The canvas to clear.
    """
    if not isinstance(canvas, MatplotlibCanvas):
        return
        
    canvas.ax.clear()
    canvas.ax.set_axis_off()
    canvas.draw()

def update_palette_list():
    """Update the palette list with current wordlist in robFilesListWidget."""
    if global_vars.ui and hasattr(global_vars.ui, 'robFilesListWidget'):
        try:
            from utils.robot_control import load_rob_files
            load_rob_files()
            logger.debug("Updated robFilesListWidget with current .rob files")
        except Exception as e:
            logger.error(f"Failed to update robFilesListWidget: {e}")

def calculate_package_centers(center, width, length, rotation, num_packages):
    centers = []

    for i in range(num_packages):
        if rotation == 0:
            x = center[0] + (i - (num_packages - 1) / 2) * width
            y = center[1]
        elif rotation == 90:
            x = center[0]
            y = center[1] + (i - (num_packages - 1) / 2) * width
        elif rotation == 180:
            x = center[0] - (i - (num_packages - 1) / 2) * width
            y = center[1]
        elif rotation == 270:
            x = center[0]
            y = center[1] - (i - (num_packages - 1) / 2) * width
        else:
            raise ValueError("Invalid rotation angle. Must be one of [0, 90, 180, 270].")
        centers.append((x, y))

    return centers


def parse_rob_file(file_path) -> Pallet:
    
    start_time = time.time()
    lines, *_ = load_from_database(file_name=file_path)

    package_width, package_length, package_height = lines[1][0:3]

    num_unique_layers = lines[2][0]
    num_layers = lines[3][0]

    layer_order = []
    current_line = 5
    for _ in range(num_layers):
        unique_layer_id = lines[current_line][0]
        layer_order.append(unique_layer_id)
        current_line += 1
    unique_layers = []

    for unique_layer_id in range(num_unique_layers):
        num_coordinates = lines[current_line][0]
        current_line += 1
        layer_data = []
        BoxCount = 1
        for _ in range(num_coordinates):
            x, y, rotation, num_packages, dx, dy = lines[current_line][3:9]
            blue_line = None
            if dx != 0 or dy != 0:
                if dx == 0 and dy > 0:
                    blue_line = Side.bottom
                elif dx == 0 and dy < 0:
                    blue_line = Side.top
                elif dx > 0 and dy == 0:
                    blue_line = Side.left
                elif dx < 0 and dy == 0:
                    blue_line = Side.right
                elif dx > 0 and dy > 0:
                    blue_line = Corner.bottom_left
                elif dx > 0 and dy < 0:
                    blue_line = Corner.top_right
                elif dx < 0 and dy > 0:
                    blue_line = Corner.bottom_right
                elif dx < 0 and dy < 0:
                    blue_line = Corner.top_left

            if num_packages == 1:
                rect = Rectangle(width=package_length, length=package_width, x=x, y=y)
                layer_data.append(Box(blueNumber=BoxCount, blueLine=blue_line, rotation=rotation, rect=rect, height=package_height))
            else:
                boxes_centers = calculate_package_centers((x, y), package_width, package_length, rotation, num_packages)
                for box_center in boxes_centers:
                    rect = Rectangle(width=package_length, length=package_width, x=box_center[0], y=box_center[1])
                    layer_data.append(Box(blueNumber=BoxCount, blueLine=blue_line, rotation=rotation, rect=rect, height=package_height))
            BoxCount += 1
            current_line += 1
        unique_layers.append(Layer(unique_layer_id=unique_layer_id, boxes=layer_data))

    layers = []
    for num in layer_order:
        layers.append(Layer(unique_layer_id=num, boxes=unique_layers[num - 1].boxes))
    pallet = Pallet(layers=layers)
    parse_time = time.time() - start_time
    logger.info(f"Parse time: {parse_time:.3f} seconds")
    return pallet

def display_pallet_3d(canvas, pallet_name):
    """Display a 3D visualization of the pallet.

    Args:
        canvas (MatplotlibCanvas): The canvas to draw on
        pallet (Pallet): The pallet data to visualize
    """
    # Create and show progress dialog
    progress = QProgressDialog("Rendering 3D visualization...", None, 0, 100)
    progress.setWindowModality(Qt.WindowModal)
    progress.setWindowTitle("Loading")
    progress.setCancelButton(None)  # No cancel button
    progress.setMinimumDuration(0)  # Show immediately
    progress.setValue(0)
    
    # Parse file
    progress.setValue(10)
    progress.setLabelText("Parsing .rob file...")
    pallet = parse_rob_file(pallet_name + ".rob")
    
    start_time = time.time()
    canvas.ax.clear()

    progress.setValue(20)
    progress.setLabelText("Setting up view...")

    # Set camera angle to view from origin corner
    elev, azim = 30, 40  # These angles will give a good view from the origin corner
    canvas.ax.view_init(elev=elev, azim=azim)
    
    # Track min/max coordinates to set proper view limits
    min_x, max_x = 0, 1200
    min_y, max_y = 0, 800
    min_z, max_z = 0, 0

    progress.setValue(30)
    progress.setLabelText("Creating boxes...")
    box_creation_start = time.time()
    
    # Reverse layer order to draw from top to bottom
    allfaces = []
    allfacecolors = []
    total_boxes = sum(len(layer.boxes) for layer in pallet.layers)
    boxes_processed = 0
    
    for layer_idx, layer in enumerate(reversed(pallet.layers)):
        layer_num = len(pallet.layers) - layer_idx - 1  # Calculate actual layer number
        for box in layer.boxes:
            width, length = (box.rect.length, box.rect.width)

            if box.rotation == 90 or box.rotation == 270:
                width, length = (box.rect.width, box.rect.length)

            z = layer_num * box.height
            height = box.height

            # Update min/max coordinates
            max_z = max(max_z, z + height)

            verts = [
                (box.rect.x - width / 2, box.rect.y - length / 2, z),
                (box.rect.x + width / 2, box.rect.y - length / 2, z),
                (box.rect.x + width / 2, box.rect.y + length / 2, z),
                (box.rect.x - width / 2, box.rect.y + length / 2, z),
                (box.rect.x - width / 2, box.rect.y - length / 2, z + height),
                (box.rect.x + width / 2, box.rect.y - length / 2, z + height),
                (box.rect.x + width / 2, box.rect.y + length / 2, z + height),
                (box.rect.x - width / 2, box.rect.y + length / 2, z + height)
            ]

            faces = [
                [verts[0], verts[1], verts[2], verts[3]],  # Bottom face
                [verts[4], verts[5], verts[6], verts[7]],  # Top face
                [verts[0], verts[1], verts[5], verts[4]],  # Front face
                [verts[2], verts[3], verts[7], verts[6]],  # Back face
                [verts[0], verts[3], verts[7], verts[4]],  # Right face
                [verts[1], verts[2], verts[6], verts[5]]   # Left face
            ]

            box_top_color = 'green'
            box_bottom_color = box_top_color
            box_label_color = 'white'
            box_back_color = 'red'
            box_left_color = 'blue'
            box_right_color = box_left_color
            
            if box.rotation == 0:
                face_colors = [
                    box_bottom_color,
                    box_top_color,
                    box_label_color,
                    box_back_color,
                    box_right_color,
                    box_left_color,
                ]
            elif box.rotation == 90:
                face_colors = [
                    box_bottom_color,
                    box_top_color,
                    box_right_color,
                    box_left_color,
                    box_back_color,
                    box_label_color,
                ]
            elif box.rotation == 180:
                face_colors = [
                    box_bottom_color,
                    box_top_color,
                    box_back_color,
                    box_label_color,
                    box_left_color,
                    box_right_color,
                ]
            else:  # 270
                face_colors = [
                    box_bottom_color,
                    box_top_color,
                    box_left_color,
                    box_right_color,
                    box_label_color,
                    box_back_color,
                ]

            allfaces.extend(faces)
            allfacecolors.extend(face_colors)
            
            boxes_processed += 1
            progress.setValue(30 + int((boxes_processed / total_boxes) * 40))

    progress.setValue(70)
    progress.setLabelText("Creating 3D collection...")
    poly3d = Poly3DCollection(allfaces, facecolors=allfacecolors, edgecolors='black', alpha=1)
    canvas.ax.add_collection3d(poly3d)

    box_creation_time = time.time() - box_creation_start

    progress.setValue(80)
    progress.setLabelText("Setting view limits...")
    # Set view limits and labels
    canvas.ax.set_xlim(min_x, max_x)
    canvas.ax.set_ylim(min_y, max_y)
    canvas.ax.set_zlim(0, max_z)

    canvas.ax.set_xlabel('X')
    canvas.ax.set_ylabel('Y')
    canvas.ax.set_zlabel('Z')
    
    canvas.ax.set_title(f'3D View of Pallet ({total_boxes} boxes)')

    # Set aspect ratio based on actual dimensions
    x_range = max_x - min_x
    y_range = max_y - min_y
    z_range = max_z
    
    canvas.ax.set_box_aspect([x_range/100, y_range/100, z_range/100])
    
    progress.setValue(90)
    progress.setLabelText("Rendering final view...")
    render_start = time.time()
    canvas.draw()
    render_time = time.time() - render_start
    
    total_time = time.time() - start_time
    
    progress.setValue(100)
    progress.close()
    
    logger.info(f"\nPerformance Metrics:\n"
    f"Box creation time: {box_creation_time:.3f} seconds\n"
    f"Render time: {render_time:.3f} seconds\n"
    f"Total time: {total_time:.3f} seconds\n"
    f"Boxes drawn: {total_boxes}\n"
    f"Average time per box: {(total_time/total_boxes)*1000:.2f} ms")