# API Reference

This document provides a complete reference for the XML-RPC API functions available in MultipackParser.

## Overview

MultipackParser exposes an XML-RPC server that allows Universal Robots to communicate with the application. The server runs on port 50000 by default and provides functions for data exchange and robot control.

## Server Information

- **Protocol**: XML-RPC
- **Port**: 50000
- **Host**: Localhost (127.0.0.1)
- **Threading**: Multi-threaded (handles concurrent requests)

## Common Functions

These functions are available for all robot types (UR10 and UR20).

### `UR_SetFileName(Artikelnummer)`

Sets the filename for the pallet data based on article number.

**Parameters**:
- `Artikelnummer` (string): Article number to set as filename

**Returns**:
- `string`: The filename that was set

**Example**:
```python
result = server.UR_SetFileName("12345")
# Returns: "12345" or the corresponding filename
```

### `UR_ReadDataFromUsbStick()`

Reads pallet data from the USB stick and updates the database.

**Parameters**: None

**Returns**:
- `0` (int): Success
- `1` (int): Error

**Example**:
```python
result = server.UR_ReadDataFromUsbStick()
# Returns: 0 on success, 1 on error
```

### `UR_GetPalletData()`

Retrieves the current pallet data from the application.

**Parameters**: None

**Returns**:
- `list`: Pallet data structure containing:
  - Palette dimensions
  - Package dimensions
  - Layer information
  - Package positions
  - Other pallet configuration data

**Example**:
```python
data = server.UR_GetPalletData()
# Returns: [palette_dim, package_dim, layers, positions, ...]
```

### `get_available_functions()`

Returns a list of all available XML-RPC functions for the current robot type.

**Parameters**: None

**Returns**:
- `list`: List of function names (strings)

**Example**:
```python
functions = server.get_available_functions()
# Returns: ["UR_SetFileName", "UR_ReadDataFromUsbStick", ...]
```

## UR10-Specific Functions

These functions are only available when the robot type is set to UR10.

### `UR_scanner1and2niobild()`

Gets the status of scanners 1 and 2 (no image).

**Parameters**: None

**Returns**:
- Status information for scanners 1 and 2

**Example**:
```python
status = server.UR_scanner1and2niobild()
```

### `UR_scanner1bild()`

Gets the status of scanner 1.

**Parameters**: None

**Returns**:
- Status information for scanner 1

**Example**:
```python
status = server.UR_scanner1bild()
```

### `UR_scanner2bild()`

Gets the status of scanner 2.

**Parameters**: None

**Returns**:
- Status information for scanner 2

**Example**:
```python
status = server.UR_scanner2bild()
```

### `UR_scanner1and2iobild()`

Gets the combined status of scanners 1 and 2 (with image).

**Parameters**: None

**Returns**:
- Combined status information for scanners 1 and 2

**Example**:
```python
status = server.UR_scanner1and2iobild()
```

## UR20-Specific Functions

These functions are only available when the robot type is set to UR20.

### `UR_scannerStatus()`

Gets the status of all three scanners (1, 2, and 3).

**Parameters**: None

**Returns**:
- Status information for all scanners

**Example**:
```python
status = server.UR_scannerStatus()
# Returns scanner status for scanners 1, 2, and 3
```

### `UR_SetActivePalette(palette_number)`

Sets the active palette number for dual palette systems.

**Parameters**:
- `palette_number` (int): Palette number to activate (1 or 2)

**Returns**:
- Success status

**Example**:
```python
result = server.UR_SetActivePalette(1)
# Sets palette 1 as active
```

### `UR_RequestPaletteChange()`

Requests a palette change operation.

**Parameters**: None

**Returns**:
- Request status

**Example**:
```python
result = server.UR_RequestPaletteChange()
```

### `UR_GetActivePaletteNumber()`

Gets the currently active palette number.

**Parameters**: None

**Returns**:
- `int`: Active palette number (1 or 2)

**Example**:
```python
palette = server.UR_GetActivePaletteNumber()
# Returns: 1 or 2
```

### `UR_GetPaletteStatus()`

Gets the status of both palettes.

**Parameters**: None

**Returns**:
- Status information for both palettes

**Example**:
```python
status = server.UR_GetPaletteStatus()
# Returns status for palette 1 and palette 2
```

### `UR_SetZwischenLageLegen(value)`

Sets whether to place an intermediate layer (Zwischenlage).

**Parameters**:
- `value` (bool): True to enable intermediate layer, False to disable

**Returns**:
- Success status

**Example**:
```python
result = server.UR_SetZwischenLageLegen(True)
# Enables intermediate layer placement
```

### `UR_GetKlemmungAktiv()`

Gets the clamping status.

**Parameters**: None

**Returns**:
- `bool`: True if clamping is active, False otherwise

**Example**:
```python
clamping = server.UR_GetKlemmungAktiv()
# Returns: True or False
```

### `UR_GetScannerOverwrite()`

Gets the scanner override status.

**Parameters**: None

**Returns**:
- Scanner override status information

**Example**:
```python
override = server.UR_GetScannerOverwrite()
```

## Client Implementation Examples

### Python Client

```python
import xmlrpc.client

# Connect to server
server = xmlrpc.client.ServerProxy("http://localhost:50000")

# Get available functions
functions = server.get_available_functions()
print("Available functions:", functions)

# Set filename
filename = server.UR_SetFileName("12345")
print("Filename set:", filename)

# Read data from USB
result = server.UR_ReadDataFromUsbStick()
if result == 0:
    print("Data read successfully")
else:
    print("Error reading data")

# Get pallet data
data = server.UR_GetPalletData()
print("Pallet data:", data)
```

### UR Robot Script (URScript)

```python
# Connect to XML-RPC server
socket_open("127.0.0.1", 50000)

# Call function to set filename
socket_send_string("UR_SetFileName", "12345")

# Read response
socket_read_string(response)

# Close socket
socket_close()
```

## Error Handling

### Connection Errors

If the server is not running or unreachable:
- XML-RPC client will raise a connection error
- Check that MultipackParser is running
- Verify port 50000 is not blocked

### Function Errors

If a function is not available for the robot type:
- Server will return an error
- Check robot type configuration
- Use `get_available_functions()` to verify available functions

### Data Errors

If data operations fail:
- Functions return error codes (typically 1 for error, 0 for success)
- Check log files for detailed error information
- Verify USB stick is connected and data is valid

## Logging

All XML-RPC function calls are logged:
- **Location**: `logs/server_YYYYMMDD_HHMMSS.log`
- **Format**: Function name, parameters, timestamp
- **Level**: DEBUG for successful calls, ERROR for failures

## Security Considerations

1. **Network Access**: Server listens on localhost by default
2. **No Authentication**: Currently no authentication required
3. **Local Network**: Intended for local network use only
4. **Input Validation**: All inputs are validated before processing

## Performance

- **Concurrent Requests**: Server handles multiple requests simultaneously
- **Response Time**: Typically < 100ms for most operations
- **Threading**: Each request handled in separate thread

## Troubleshooting

### Server Not Responding

1. Check if MultipackParser is running
2. Verify port 50000 is available
3. Check firewall settings
4. Review server logs

### Function Not Found

1. Verify robot type is correctly configured
2. Check available functions with `get_available_functions()`
3. Ensure correct robot-specific module is loaded

### Data Not Updating

1. Verify USB stick is connected
2. Check file format compatibility
3. Review application logs
4. Manually trigger data refresh

## Version Compatibility

- **API Version**: 1.7.6
- **Backward Compatibility**: Functions may change between versions
- **Robot Compatibility**: UR10 and UR20 supported

## Additional Resources

- [XML-RPC Specification](https://www.xmlrpc.com/spec)
- [Python xmlrpc.client Documentation](https://docs.python.org/3/library/xmlrpc.client.html)
- [Universal Robots Documentation](https://www.universal-robots.com/)

