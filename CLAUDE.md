# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Path Loss Simulator v1.1 is an ns-3-based testbed for 5G NR V2X (Vehicle-to-Vehicle/Vehicle-to-Everything) research. It integrates ns-3 (network simulator) with SUMO (Simulation of Urban Mobility) and extends ns-3 with two custom modules: `mmwave` (mmWave cellular) and `millicar` (mmWave vehicular V2X). The simulator focuses on millimeter-wave propagation, including weather effects (rain/snow attenuation), beamforming, and adaptive PHY/MAC for 5G NR.

## Build Commands

The project uses WAF (Python-based build system). All commands run from the repository root.

```bash
# Initial configure (required before first build)
./waf configure --disable-python --enable-examples

# Build
./waf build

# Run a specific example
./waf --run "vehicular-simple-two"

# Run with command-line arguments
./waf --run "vehicular-simple-two --RngRun=1"

# Run tests
./test.py                              # All tests
./test.py -s millicar                  # Single test suite
./test.py -s mmwave-channel-model      # Single test suite

# Clean
./waf clean        # Clean build artifacts
./waf distclean    # Full clean (forces reconfigure)

# Generate Doxygen docs
./waf --doxygen
```

Build profiles: `--build-profile=debug` (default), `--build-profile=optimized`, `--build-profile=release`. The `optimized` profile is recommended for long simulation runs.

## Module Architecture

### Two Custom ns-3 Modules

**`src/mmwave/`** — mmWave cellular (eNB ↔ UE) stack:
- `model/` — 84 source files implementing PHY, MAC, schedulers, beamforming, HARQ, AMC, propagation loss
- `helper/` — `MmWaveHelper` (main entry point for cellular sim setup), bearer stats, PHY traces
- Key classes: `MmWaveSpectrumPhy`, `MmWaveEnbPhy`/`MmWaveUePhy`, `MmWaveEnbMac`/`MmWaveUeMac`, `MmWaveFlexTtiMacScheduler`

**`src/millicar/`** — mmWave V2V sidelink (vehicle ↔ vehicle) stack, derived from mmwave:
- `model/` — `MmWaveSidelinkSpectrumPhy`, `MmWaveSidelinkMac`, `MmWaveVehicularNetDevice`, propagation loss models, `RainAttenuation`, `RainSnowAttenuation`, antenna array model
- `helper/` — `MmWaveVehicularHelper` (main entry point for V2X sim setup), traces helper
- `examples/` — `vehicular-simple-one.cc`, `vehicular-simple-two.cc`, `mmwave-vehicular-link-adaptation-example.cc`

### Module Dependencies

```
millicar → mmwave → lte, spectrum, propagation, internet, core
```

### Data Flow (Millicar V2X)

```
Application (UDP/TCP)
    ↓
PDCP / RLC  (from ns-3 LTE module)
    ↓
MmWaveSidelinkMac
    ↓
MmWaveSidelinkSpectrumPhy
    ↓
Channel: MmWaveVehicularPropagationLossModel
         MmWaveVehicularSpectrumPropagationLossModel
         RainAttenuation / RainSnowAttenuation
    ↓
MmWaveVehicularAntennaArrayModel (beamforming)
```

### Namespaces

- `ns3::` — base ns-3 classes
- `ns3::mmwave::` — cellular mmWave classes
- `ns3::millicar::` — vehicular V2X classes

## Example Simulation Structure

All examples follow this pattern:

```cpp
// 1. Set global defaults via Config::SetDefault BEFORE creating objects
Config::SetDefault("ns3::MmWaveSidelinkMac::UseAmc", BooleanValue(true));
Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(28e9));
Config::SetDefault("ns3::MmWaveVehicularHelper::Numerology", UintegerValue(3));
Config::SetDefault("ns3::MmWaveVehicularPropagationLossModel::ChannelCondition", StringValue("l"));

// 2. Create and configure nodes + mobility
NodeContainer nodes;
nodes.Create(2);
// install MobilityModel...

// 3. Install devices via helper
Ptr<MmWaveVehicularHelper> helper = CreateObject<MmWaveVehicularHelper>();
NetDeviceContainer devs = helper->InstallMmWaveVehicularNetDevices(nodes);

// 4. Install IP stack
InternetStackHelper internet;
internet.Install(nodes);

// 5. Assign addresses and run
Simulator::Run();
Simulator::Destroy();
```

## Key Configuration Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `MmWavePhyMacCommon::CenterFreq` | 28e9 Hz | Carrier frequency |
| `MmWaveVehicularHelper::Bandwidth` | 100e6 Hz | Channel bandwidth |
| `MmWaveVehicularHelper::Numerology` | 3 | 5G NR numerology (0–4) |
| `MmWaveVehicularPropagationLossModel::ChannelCondition` | `"l"` | `"a"` (approaching), `"l"` (LOS), `"n"` (NLOS), `"v"` (V2V) |
| `MmWaveVehicularPropagationLossModel::Shadowing` | true | Enable shadow fading |
| `MmWaveVehicularAntennaArrayModel::AntennaElements` | 4 | Number of antenna elements |
| `MmWaveSidelinkMac::UseAmc` | true | Adaptive modulation and coding |

## Code Conventions

- **File header:** Every `.h`/`.cc` begins with `/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */` followed by a GPLv2 copyright block.
- **Style:** ns-3 GNU coding style (see https://www.nsnam.org/developers/contributing-code/coding-style/). Indentation uses spaces, not tabs.
- **File naming:** `mmwave-*.h/.cc` for mmwave module; `mmwave-vehicular-*.h/.cc` or `mmwave-sidelink-*.h/.cc` for millicar.
- **Callbacks:** Defined as `typedef Callback<RetType, ArgTypes...> MmWave*Callback;` in header files.
- **TypeId registration:** Every `Object`-derived class implements `static TypeId GetTypeId(void)` and registers attributes there (this is how `Config::SetDefault` works).
- **Tests:** Located in `src/mmwave/test/` and `src/millicar/test/`, registered as ns-3 test suites.

## Result Visualization

Python plotting scripts are in `scripts/`:
- `plot_sinr_rate.py` — SINR and throughput plots
- `plot_rate_phy.py` — PHY-layer rate analysis
- `plot_latency.py` — Latency analysis

Test output data is stored under `Test/Test_data_22*/`.

## SUMO Integration

- `example_sumo_ns3.sh` — launches a combined SUMO + ns-3 simulation
- `sumo_ns3_paderborn_scenario.sh` — Paderborn city traffic scenario
- SUMO provides vehicle mobility traces; ns-3 millicar handles the wireless channel simulation
