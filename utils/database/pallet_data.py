from enum import Enum
from typing import Union, List

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
    def __init__(self, width: int, length: int, x: int, y: int) -> None:
        self.width: int = width
        self.length: int = length
        self.x: int = x
        self.y: int = y

class Box:
    def __init__(self, blueNumber: int, blueLine: Union[Side, Corner, None], rotation: Rotation, rect: Rectangle, height: int) -> None:
        self.blueNumber: int = blueNumber
        self.blueLine: Union[Side, Corner, None] = blueLine
        self.rotation: Rotation = rotation
        self.rect: Rectangle = rect
        self.height: int = height

class Layer:
    def __init__(self, unique_layer_id: int, boxes: List[Box]) -> None:
        self.unique_layer_id: int = unique_layer_id
        self.boxes: List[Box] = boxes

class Pallet:
    def __init__(self, layers: List[Layer], width: int, length: int):
        self.layers: List[Layer] = layers
        self.layer_count: int = len(layers)
        self.total_boxes: int = sum(map(lambda layer: len(layer.boxes), layers))
        self.width: int = width
        self.length: int = length
