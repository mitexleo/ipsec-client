# IPsec Client

A Qt-based GUI application for managing strongSwan IPsec VPN connections via NetworkManager.

## Build Instructions

### Prerequisites (Native)

- Qt 5.15+ or Qt 6.x (Widgets module)
- CMake 3.16+
- NetworkManager with strongSwan VPN plugin (`network-manager-strongswan`)
- `nmcli` (NetworkManager command-line tool)
- strongSwan (`strongswan` or `strongswan-swanctl`)

### Build (Native)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Build (Flatpak)

#### Prerequisites (Host)

- flatpak
- flatpak-builder
- NetworkManager
- network-manager-strongswan
- strongswan

#### Build & Install

```bash
flatpak-builder build-dir flatpak/io.github.mitexleo.ipsec_client.yml --force-clean
flatpak-builder --user --install build-dir flatpak/io.github.mitexleo.ipsec_client.yml --force-clean
```

## Usage

Run the application:

```bash
./ipsec-client
```

Or under Flatpak:

```bash
flatpak run io.github.mitexleo.ipsec_client
```

Use the interface to add, edit, delete, and toggle strongSwan VPN connections.
