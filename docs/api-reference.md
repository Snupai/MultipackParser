# API Reference

Complete reference documentation for the MultipackParser XML-RPC API.

---

## Table of Contents

1. [Overview](#overview)
2. [Server Configuration](#server-configuration)
3. [Common Functions](#common-functions)
4. [UR10-Specific Functions](#ur10-specific-functions)
5. [UR20-Specific Functions](#ur20-specific-functions)
6. [Client Examples](#client-examples)
7. [Error Handling](#error-handling)
8. [Troubleshooting](#troubleshooting)

---

## Overview

MultipackParser exposes an XML-RPC server that allows Universal Robots to communicate with the application. The server provides functions for palette data retrieval, robot status updates, and system control.

> [!IMPORTANT]
> ### Robot-Side Requirements
> 
> To use this API, you must have:
> - A **UR Program** installed and running on your Universal Robot controller
> - **URscript** code that calls these XML-RPC functions
> 
> These robot-side components are **not included** in this repository and must be obtained separately.

---

## Server Configuration

| Parameter | Value |
|-----------|-------|
| **Protocol** | XML-RPC |
| **Port** | 50000 |
| **Host** | 127.0.0.1 (localhost) |
| **Threading** | Multi-threaded (concurrent requests supported) |

### Connection String

```
http://127.0.0.1:50000
```

---

## Common Functions

These functions are available for **all robot types** (UR10 and UR20).

### `UR_SetFileName(Artikelnummer)`

Sets the filename for palette data lookup based on article number.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `Artikelnummer` | string | Article number (without .rob extension) |

**Returns:** `string` - The complete filename that was set (with .rob extension)

**Example:**
```python
filename = server.UR_SetFileName("12345")
# Returns: "12345.rob"
```

---

### `UR_ReadDataFromUsbStick()`

Reads and parses palette data from the configured USB path into global variables.

**Parameters:** None

**Returns:** `int`
- `0` - Success
- `1` - Error (file not found, parse error, etc.)

**Side Effects:**
- Populates global variables: `g_PalettenDim`, `g_PaketDim`, `g_PaketPos`, etc.
- Updates layer and package assignment data

**Example:**
```python
result = server.UR_ReadDataFromUsbStick()
if result == 0:
    print("Data loaded successfully")
else:
    print("Error loading data")
```

---

### `UR_Palette()`

Returns the palette dimensions.

**Parameters:** None

**Returns:** `List[int]` or `None`
- `[length, width, height]` - Palette dimensions in mm
- `None` if no data loaded

**Example:**
```python
dims = server.UR_Palette()
# Returns: [1200, 800, 144] (EUR palette)
```

---

### `UR_Karton()`

Returns the package/carton dimensions.

**Parameters:** None

**Returns:** `List[int]` or `None`
- `[length, width, height, gap]` - Package dimensions and gap in mm
- `None` if no data loaded

**Example:**
```python
dims = server.UR_Karton()
# Returns: [400, 300, 200, 5]
```

---

### `UR_Lagen()`

Returns the layer type assignments for each layer.

**Parameters:** None

**Returns:** `List[int]` or `None`
- List of layer type indices
- `None` if no data loaded

**Example:**
```python
layers = server.UR_Lagen()
# Returns: [1, 2, 1, 2, 1, 2] - alternating layer types
```

---

### `UR_Zwischenlagen()`

Returns the intermediate layer (Zwischenlage) configuration for each layer.

**Parameters:** None

**Returns:** `List[int]` or `None`
- List of intermediate layer flags
- `None` if no data loaded

**Example:**
```python
intermediate = server.UR_Zwischenlagen()
# Returns: [0, 1, 0, 1, 0, 0] - intermediate layer after layers 2 and 4
```

---

### `UR_PaketPos(Nummer)`

Returns the position data for a specific package, with coordinate transformation for UR20 palette 2.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `Nummer` | int | Package index (0-based) |

**Returns:** `List[int]` or `None`
- `[xp, yp, ap, xd, yd, ad, nop, dx, dy]` - Position data
- `None` if index invalid or no data

**Position Data Fields:**

| Index | Field | Description |
|-------|-------|-------------|
| 0 | `xp` | X position (pick) |
| 1 | `yp` | Y position (pick) |
| 2 | `ap` | Angle (pick) |
| 3 | `xd` | X position (drop) |
| 4 | `yd` | Y position (drop) |
| 5 | `ad` | Angle (drop) |
| 6 | `nop` | Number of packages |
| 7 | `dx` | X vector |
| 8 | `dy` | Y vector |

**Example:**
```python
pos = server.UR_PaketPos(0)
# Returns: [100, 200, 0, 150, 250, 90, 1, 0, 0]
```

---

### `UR_AnzLagen()`

Returns the total number of layers in the palette configuration.

**Parameters:** None

**Returns:** `int` or `None`
- Number of layers
- `None` if no data loaded

**Example:**
```python
count = server.UR_AnzLagen()
# Returns: 6
```

---

### `UR_AnzPakete()`

Returns the number of packages (picks) per layer type.

**Parameters:** None

**Returns:** `int` or `None`
- Number of packages
- `None` if no data loaded

**Example:**
```python
count = server.UR_AnzPakete()
# Returns: 8
```

---

### `UR_PaketeZuordnung()`

Returns the package count for each layer type.

**Parameters:** None

**Returns:** `List[int]` or `None`
- List of package counts per layer type
- `None` if no data loaded

**Example:**
```python
assignment = server.UR_PaketeZuordnung()
# Returns: [8, 7] - 8 packages in layer type 1, 7 in layer type 2
```

---

### `UR_CoG(Masse_Paket, Masse_Greifer, Anzahl_Pakete)`

Calculates the center of gravity for payload configuration.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `Masse_Paket` | float | Package mass (kg) |
| `Masse_Greifer` | float | Gripper mass (kg) |
| `Anzahl_Pakete` | int | Number of packages (default: 1) |

**Returns:** `List[float]` or `None`
- `[y, z, 0]` - Center of gravity coordinates
- `None` if calculation fails

**Example:**
```python
cog = server.UR_CoG(5.0, 2.5, 1)
# Returns: [-0.045, 0.147, 0]
```

---

### `UR_Paket_hoehe()`

Returns the package height from UI input.

**Parameters:** None

**Returns:** `int` - Package height in mm

**Example:**
```python
height = server.UR_Paket_hoehe()
# Returns: 200
```

---

### `UR_Startlage()`

Returns the starting layer number from UI input.

**Parameters:** None

**Returns:** `int` - Starting layer number

**Example:**
```python
start = server.UR_Startlage()
# Returns: 1
```

---

### `UR_MasseGeschaetzt()`

Returns the estimated package mass from UI input.

**Parameters:** None

**Returns:** `float` - Package mass in kg

**Example:**
```python
mass = server.UR_MasseGeschaetzt()
# Returns: 5.5
```

---

### `UR_PickOffsetX()` / `UR_PickOffsetY()`

Returns the pick offset in X or Y direction from UI input.

**Parameters:** None

**Returns:** `int` - Offset value in mm

**Example:**
```python
offset_x = server.UR_PickOffsetX()
offset_y = server.UR_PickOffsetY()
# Returns: 10, 5
```

---

### `UR_Quergreifen()`

Returns whether single package should be gripped crosswise.

**Parameters:** None

**Returns:** `bool` - True if crosswise grip enabled

**Example:**
```python
crosswise = server.UR_Quergreifen()
# Returns: True or False
```

---

## UR10-Specific Functions

These functions are **only available** when the robot type is set to UR10.

### `UR10_scanner1and2niobild()`

Gets the combined scanner status value for scanners 1 and 2 (no image variant).

**Returns:** `int` - Scanner status value

---

### `UR10_scanner1bild()`

Gets the status value for scanner 1.

**Returns:** `int` - Scanner 1 status value

---

### `UR10_scanner2bild()`

Gets the status value for scanner 2.

**Returns:** `int` - Scanner 2 status value

---

### `UR10_scanner1and2iobild()`

Gets the combined scanner status value for scanners 1 and 2 (with image variant).

**Returns:** `int` - Combined scanner status value

---

## UR20-Specific Functions

These functions are **only available** when the robot type is set to UR20.

### `UR20_scannerStatus(status)`

Updates the scanner status and triggers UI updates.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `status` | string | Comma-separated scanner status ("True,True,True") |

**Status Format:** `"Scanner1,Scanner2,Scanner3"`
- `"True"` = Scanner OK
- `"False"` = Scanner triggered/blocked

**Returns:** `int` - Always returns 0

**Status Combinations:**

| Status | Meaning |
|--------|---------|
| `"True,True,True"` | All scanners OK |
| `"False,False,False"` | All scanners triggered |
| `"True,False,False"` | Only scanner 1 OK |
| `"False,True,False"` | Only scanner 2 OK |
| `"False,False,True"` | Only scanner 3 OK |
| (other combinations) | Partial triggers |

**Side Effects:**
- Updates scanner image in UI
- Plays warning sound if status changes to unsafe

**Example:**
```python
server.UR20_scannerStatus("True,True,True")
# Sets all scanners to OK status
```

---

### `UR20_SetActivePalette(pallet_number)`

Sets the active palette for dual-palette operations.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `pallet_number` | int | Palette number (1 or 2) |

**Returns:** `int`
- Palette number if successful
- `503` if palette not empty
- `404` if invalid palette number

**Example:**
```python
result = server.UR20_SetActivePalette(1)
if result == 1:
    print("Palette 1 is now active")
elif result == 503:
    print("Palette 1 is not empty")
```

---

### `UR20_RequestPaletteChange(old_pallet_number, new_pallet_number)`

Requests a palette change from one palette to another.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `old_pallet_number` | int | Current palette number |
| `new_pallet_number` | int | Target palette number (1 or 2) |

**Returns:** `int`
- `1` if change request approved
- `0` if new palette not empty
- `404` if invalid palette number

**Example:**
```python
result = server.UR20_RequestPaletteChange(1, 2)
if result == 1:
    print("Palette change approved")
```

---

### `UR20_GetActivePaletteNumber()`

Gets the currently active palette number.

**Returns:** `int`
- Active palette number (1 or 2)
- `0` if active palette is not empty (cannot use)

**Example:**
```python
active = server.UR20_GetActivePaletteNumber()
# Returns: 1, 2, or 0
```

---

### `UR20_GetPaletteStatus(pallet_number)`

Gets the status (empty/full) of a specific palette.

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `pallet_number` | int | Palette number (1 or 2) |

**Returns:** `int`
- `1` if palette is empty
- `0` if palette is full/not empty
- `-1` if invalid palette number

**Example:**
```python
status = server.UR20_GetPaletteStatus(1)
if status == 1:
    print("Palette 1 is empty")
else:
    print("Palette 1 is not empty")
```

---

### `UR20_SetZwischenLageLegen(aktiv)`

Sets whether to place an intermediate layer (Zwischenlage).

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `aktiv` | bool | True to enable, False to disable |

**Returns:** `int` - Always returns 1

**Example:**
```python
server.UR20_SetZwischenLageLegen(True)
# Enables intermediate layer placement
```

---

### `UR20_GetKlemmungAktiv()`

Gets the clamping mechanism status.

**Returns:** `bool` - True if clamping is active

**Example:**
```python
clamping = server.UR20_GetKlemmungAktiv()
# Returns: True or False
```

---

### `UR20_GetScannerOverride()`

Gets the scanner override status for all three scanners.

**Returns:** `List[bool]` - `[scanner1, scanner2, scanner3]` override status

**Example:**
```python
override = server.UR20_GetScannerOverride()
# Returns: [False, False, True] - scanner 3 is overridden
```

---

## Client Examples

### Python Client

```python
import xmlrpc.client

# Connect to server
server = xmlrpc.client.ServerProxy("http://127.0.0.1:50000")

# Basic workflow
filename = server.UR_SetFileName("12345")
print(f"Loading: {filename}")

result = server.UR_ReadDataFromUsbStick()
if result == 0:
    # Get palette data
    palette = server.UR_Palette()
    package = server.UR_Karton()
    layers = server.UR_AnzLagen()
    
    print(f"Palette: {palette[0]}x{palette[1]}x{palette[2]} mm")
    print(f"Package: {package[0]}x{package[1]}x{package[2]} mm")
    print(f"Layers: {layers}")
    
    # Get package positions
    for i in range(server.UR_AnzPakete()):
        pos = server.UR_PaketPos(i)
        print(f"Package {i}: X={pos[3]}, Y={pos[4]}, R={pos[5]}")
else:
    print("Error loading data")
```

### UR20 Palette Management

```python
import xmlrpc.client

server = xmlrpc.client.ServerProxy("http://127.0.0.1:50000")

# Check palette status
status1 = server.UR20_GetPaletteStatus(1)
status2 = server.UR20_GetPaletteStatus(2)

print(f"Palette 1: {'empty' if status1 == 1 else 'not empty'}")
print(f"Palette 2: {'empty' if status2 == 1 else 'not empty'}")

# Set active palette
if status1 == 1:  # Palette 1 is empty
    result = server.UR20_SetActivePalette(1)
    print(f"Set active palette result: {result}")

# Request palette change
change_result = server.UR20_RequestPaletteChange(1, 2)
if change_result == 1:
    print("Palette change approved")
elif change_result == 0:
    print("New palette not empty")
```

### URScript Example (Simplified)

> [!NOTE]
> This is a simplified example. The actual URscript implementation must be obtained separately.

```python
# URScript pseudo-code
def call_xmlrpc(function_name, params):
    # Implementation depends on robot-side code
    pass

# Set filename
filename = call_xmlrpc("UR_SetFileName", ["12345"])

# Read data
result = call_xmlrpc("UR_ReadDataFromUsbStick", [])

# Get position for package 0
position = call_xmlrpc("UR_PaketPos", [0])
x = position[3]
y = position[4]
rotation = position[5]
```

---

## Error Handling

### Connection Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `ConnectionRefusedError` | Server not running | Start MultipackParser |
| `TimeoutError` | Network issue | Check connectivity |
| `socket.gaierror` | Invalid host | Check server address |

### Function Errors

| Return Value | Meaning |
|--------------|---------|
| `None` | Data not loaded or invalid request |
| `0` | Success (for status functions) |
| `1` | Error (for status functions) |
| `404` | Invalid parameter (e.g., invalid palette number) |
| `503` | Resource not available (e.g., palette not empty) |

### Best Practices

1. **Always check return values** before using data
2. **Handle None returns** gracefully
3. **Log errors** for debugging
4. **Implement timeouts** for robustness

---

## Troubleshooting

### Server Not Responding

1. Check MultipackParser is running
2. Verify port 50000 is not blocked
3. Check firewall settings
4. Review server logs

### Function Not Found

1. Verify robot type is correctly configured (UR10 vs UR20)
2. Function names are case-sensitive
3. Some functions are robot-type specific

### Data Not Updating

1. Verify USB stick is connected
2. Check file format (.rob files from Multipack)
3. Call `UR_ReadDataFromUsbStick()` to refresh
4. Review application logs

### Logging

Server logs are stored in: `logs/server_YYYYMMDD_HHMMSS.log`

---

## Version Information

| Item | Value |
|------|-------|
| API Version | 1.7.9 |
| Protocol | XML-RPC |
| Supported Robots | UR10, UR20 |

---

## Additional Resources

- [XML-RPC Specification](https://www.xmlrpc.com/spec)
- [Python xmlrpc.client](https://docs.python.org/3/library/xmlrpc.client.html)
- [Universal Robots Documentation](https://www.universal-robots.com/)

---

<div align="center">
  <sub>MultipackParser API Reference - Version 1.7.9</sub>
</div>
