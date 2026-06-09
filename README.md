# IPsec Client

A Qt-based GUI application for managing strongSwan IPsec VPN connections via NetworkManager.

## Build Instructions

### Prerequisites

- Qt 5.15+ or Qt 6.x (Widgets module)
- CMake 3.16+
- NetworkManager with strongSwan VPN plugin (`network-manager-strongswan`)
- `nmcli` (NetworkManager command-line tool)

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

Run the application:

```bash
./ipsec-client
```

Use the interface to add, edit, delete, and toggle strongSwan VPN connections.
