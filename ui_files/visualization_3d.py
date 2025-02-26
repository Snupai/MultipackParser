import os
import sys
from typing import Union, List
from enum import Enum
import matplotlib
matplotlib.use('qtagg', force=True)
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from PySide6.QtWidgets import QVBoxLayout
from utils import global_vars
import time

logger = global_vars.logger

class Side(Enum):
    top = 1
    right = 2
    bottom = 3
    left = 4

class Corner(Enum):
    top_right = 1
    bottom_right = 2
    bottom_left = 3
    top_left = 4

class Rotation(Enum):
    zero = 0
    ninety = 90
    one_eighty = 180
    two_seventy = 270

class Rectangle:
    def __init__(self, width: int, length: int, x: int, y: int):
        self.width = width
        self.length = length
        self.x = x
        self.y = y

class Box:
    def __init__(self, blueNumber: int, blueLine: Union[Side, Corner, None], rotation: Rotation, rect: Rectangle, height: int):
        self.blueNumber = blueNumber
        self.blueLine = blueLine
        self.rotation = rotation
        self.rect = rect
        self.height = height

class Layer:
    def __init__(self, unique_layer_id: int, boxes: List[Box]):
        self.unique_layer_id = unique_layer_id
        self.boxes = boxes

class Pallet:
    def __init__(self, layers: List[Layer]):
        self.layers = layers
        self.layer_count = len(layers)
        self.total_boxes = sum(map(lambda layer: len(layer.boxes), layers))

class MatplotlibCanvas(FigureCanvas):
    def __init__(self, parent=None, width=6, height=4, dpi=100):
        fig = Figure(figsize=(width, height), dpi=dpi)
        self.ax = fig.add_subplot(111, projection='3d')
        super().__init__(fig)
        self.setParent(parent)


def initialize_3d_view(frame):
    """Initialize the 3D view in the given frame.

    Args:
        frame (QFrame): The frame to initialize the 3D view in.
    """
    canvas = MatplotlibCanvas(frame, width=12, height=9, dpi=100)
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
    with open(file_path, 'r') as file:
        lines = file.readlines()

    package_dimensions = list(map(int, lines[1].strip().split()))
    package_width, package_length, package_height = package_dimensions[0:3]

    num_unique_layers = int(lines[2].strip())
    num_layers = int(lines[3].strip())

    layer_order = []
    current_line = 5
    for _ in range(num_layers):
        unique_layer_id = int(lines[current_line].strip().split()[0])
        layer_order.append(unique_layer_id)
        current_line += 1

    num_coordinates = int(lines[current_line].strip())
    current_line += 1
    layer_data = []
    BoxCount = 1
    for _ in range(num_coordinates):
        coord_line = list(map(int, lines[current_line].strip().split()))
        x, y, rotation, num_packages, dx, dy = coord_line[3:9]
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

    unique_layers = []
    unique_layers.append(Layer(unique_layer_id=1, boxes=layer_data))
    layer_data_2 = []
    BoxCount = 1
    num_coordinates = int(lines[current_line].strip())
    current_line += 1
    for _ in range(num_coordinates):
        coord_line = list(map(int, lines[current_line].strip().split()))
        x, y, rotation, num_packages, dx, dy = coord_line[3:9]
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
            layer_data_2.append(Box(blueNumber=BoxCount, blueLine=blue_line, rotation=rotation, rect=rect, height=package_height))
        else:
            boxes_centers = calculate_package_centers((x, y), package_width, package_length, rotation, num_packages)
            for box_center in boxes_centers:
                rect = Rectangle(width=package_length, length=package_width, x=box_center[0], y=box_center[1])
                layer_data_2.append(Box(blueNumber=BoxCount, blueLine=blue_line, rotation=rotation, rect=rect, height=package_height))
        BoxCount += 1
        current_line += 1

    unique_layers.append(Layer(unique_layer_id=2, boxes=layer_data_2))
    layers = []
    for num in layer_order:
        layers.append(Layer(unique_layer_id=num, boxes=unique_layers[num - 1].boxes))
    pallet = Pallet(layers=layers)
    return pallet

def display_pallet_3d(canvas, pallet_name):
    """Display a 3D visualization of the pallet.

    Args:
        canvas (MatplotlibCanvas): The canvas to draw on
        pallet (Pallet): The pallet data to visualize
    """
    pallet = parse_rob_file(global_vars.PATH_USB_STICK + pallet_name + ".rob")
    start_time = time.time()
    canvas.ax.clear()

    # Set camera angle to view from origin corner
    elev, azim = 30, 40  # These angles will give a good view from the origin corner
    canvas.ax.view_init(elev=elev, azim=azim)
    
    # Track min/max coordinates to set proper view limits
    min_x, max_x = 0, 1200
    min_y, max_y = 0, 800
    min_z, max_z = 0, 0

    box_creation_start = time.time()
    # Reverse layer order to draw from top to bottom
    allfaces = []
    allfacecolors = []
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

    poly3d = Poly3DCollection(allfaces, facecolors=allfacecolors, edgecolors='black', alpha=1)
    canvas.ax.add_collection3d(poly3d)

    box_creation_time = time.time() - box_creation_start

    # Set view limits and labels
    canvas.ax.set_xlim(min_x, max_x)
    canvas.ax.set_ylim(min_y, max_y)
    canvas.ax.set_zlim(0, max_z)

    canvas.ax.set_xlabel('X')
    canvas.ax.set_ylabel('Y')
    canvas.ax.set_zlabel('Z')
    
    # Calculate total boxes
    total_boxes = sum(len(layer.boxes) for layer in pallet.layers)
    
    canvas.ax.set_title(f'3D View of Pallet ({total_boxes} boxes)')

    # Set aspect ratio based on actual dimensions
    x_range = max_x - min_x
    y_range = max_y - min_y
    z_range = max_z
    
    canvas.ax.set_box_aspect([x_range/100, y_range/100, z_range/100])
    
    render_start = time.time()
    canvas.draw()
    render_time = time.time() - render_start
    
    total_time = time.time() - start_time
    
    print(f"\nPerformance Metrics:")
    print(f"Box creation time: {box_creation_time:.3f} seconds")
    print(f"Render time: {render_time:.3f} seconds")
    print(f"Total time: {total_time:.3f} seconds")
    print(f"Boxes drawn: {total_boxes}")
    print(f"Average time per box: {(total_time/total_boxes)*1000:.2f} ms")