# Figures

Written by `make figures`; do not edit it by hand.
Run that again after a change to the firmware or its tests, and commit what it writes.

## Images

| | Flash | Of the flash available | Static RAM |
|---|---:|---:|---:|
| debug | 14602 bytes | 47.5% | 990 bytes |
| release | 13156 bytes | 42.8% | 342 bytes |
| midi | 13448 bytes | 43.8% | 394 bytes |

The flash available is the 30720 bytes below the old Nano bootloader.
Static RAM is initialised data, zeroed data and the one byte kept through a reset;
`avr-size` prints less, because it counts initialised data under flash only.

## Flash by source file, in bytes

| | Debug | Release | Midi |
|---|---:|---:|---:|
| **All of `adapters/target/`** | **2687** | **1240** | **1532** |
| **All of `app/`** | **2895** | **2895** | **2895** |
| **All of `core/`** | **8786** | **8786** | **8786** |
| `adapters/target/analog_reader.c` | 88 | 88 | 88 |
| `adapters/target/breadcrumb.c` | 12 | 12 | 12 |
| `adapters/target/clock_input.c` | 36 | 36 | 36 |
| `adapters/target/clock_output.c` | 12 | 12 | 12 |
| `adapters/target/cv_output.c` | 110 | 110 | 110 |
| `adapters/target/eeprom.c` | 308 | 308 | 308 |
| `adapters/target/gate_output.c` | 12 | 12 | 12 |
| `adapters/target/init.c` | 68 | 68 | 68 |
| `adapters/target/level_output.c` | 36 | 36 | 36 |
| `adapters/target/midi_input.c` | 0 | 0 | 146 |
| `adapters/target/midi_output.c` | 0 | 0 | 162 |
| `adapters/target/null_logger.c` | 0 | 26 | 26 |
| `adapters/target/null_midi_input.c` | 6 | 6 | 0 |
| `adapters/target/null_midi_output.c` | 10 | 10 | 0 |
| `adapters/target/register_init.c` | 48 | 48 | 48 |
| `adapters/target/reset_cause.c` | 12 | 12 | 12 |
| `adapters/target/reset_input.c` | 28 | 28 | 28 |
| `adapters/target/run_input.c` | 12 | 12 | 12 |
| `adapters/target/serial_logger.c` | 1473 | 0 | 0 |
| `adapters/target/shift_reg_reader.c` | 224 | 224 | 224 |
| `adapters/target/spi.c` | 70 | 70 | 70 |
| `adapters/target/status_output.c` | 12 | 12 | 12 |
| `adapters/target/timebase.c` | 90 | 90 | 90 |
| `adapters/target/watchdog.c` | 20 | 20 | 20 |
| `app/app.c` | 1862 | 1862 | 1862 |
| `app/app_profile.c` | 1017 | 1017 | 1017 |
| `app/main.c` | 16 | 16 | 16 |
| `core/address.c` | 294 | 294 | 294 |
| `core/bank.c` | 348 | 348 | 348 |
| `core/blink.c` | 172 | 172 | 172 |
| `core/cal.c` | 496 | 496 | 496 |
| `core/cal_record.c` | 152 | 152 | 152 |
| `core/clock.c` | 484 | 484 | 484 |
| `core/crc8.c` | 44 | 44 | 44 |
| `core/debounce.c` | 70 | 70 | 70 |
| `core/engine_cartesian.c` | 210 | 210 | 210 |
| `core/engine_chance.c` | 292 | 292 | 292 |
| `core/engine_euclid.c` | 240 | 240 | 240 |
| `core/engine_klee.c` | 406 | 406 | 406 |
| `core/engine_plain.c` | 146 | 146 | 146 |
| `core/engine_stage.c` | 378 | 378 | 378 |
| `core/env.c` | 454 | 454 | 454 |
| `core/errlog.c` | 364 | 364 | 364 |
| `core/gate.c` | 68 | 68 | 68 |
| `core/lfo.c` | 363 | 363 | 363 |
| `core/midi_in.c` | 114 | 114 | 114 |
| `core/midi_out.c` | 538 | 538 | 538 |
| `core/mod.c` | 382 | 382 | 382 |
| `core/note_map.c` | 157 | 157 | 157 |
| `core/panel.c` | 100 | 100 | 100 |
| `core/pickup.c` | 156 | 156 | 156 |
| `core/pitch_cal.c` | 152 | 152 | 152 |
| `core/prng.c` | 76 | 76 | 76 |
| `core/profile.c` | 234 | 234 | 234 |
| `core/profile_record.c` | 152 | 152 | 152 |
| `core/seq.c` | 1466 | 1466 | 1466 |
| `core/transport.c` | 278 | 278 | 278 |
| startup object | 128 | 128 | 128 |
| libgcc | 106 | 106 | 106 |
| padding | 0 | 1 | 1 |
| **Total** | **14602** | **13156** | **13448** |

## Static RAM by source file, in bytes

| | Debug | Release | Midi |
|---|---:|---:|---:|
| **All of `adapters/target/`** | **674** | **25** | **77** |
| **All of `app/`** | **248** | **248** | **248** |
| **All of `core/`** | **68** | **68** | **68** |
| `adapters/target/breadcrumb.c` | 1 | 1 | 1 |
| `adapters/target/eeprom.c` | 20 | 20 | 20 |
| `adapters/target/midi_input.c` | 0 | 0 | 18 |
| `adapters/target/midi_output.c` | 0 | 0 | 34 |
| `adapters/target/serial_logger.c` | 649 | 0 | 0 |
| `adapters/target/shift_reg_reader.c` | 2 | 2 | 2 |
| `adapters/target/timebase.c` | 2 | 2 | 2 |
| `app/app.c` | 224 | 224 | 224 |
| `app/app_profile.c` | 24 | 24 | 24 |
| `core/lfo.c` | 17 | 17 | 17 |
| `core/note_map.c` | 51 | 51 | 51 |
| padding | 0 | 1 | 1 |
| **Total** | **990** | **342** | **394** |

## Host tests

| Area | Programs | Tests |
|---|---:|---:|
| Core | 37 | 499 |
| App loop | 9 | 129 |
| Target adapters | 24 | 172 |
| `main.c` | 1 | 3 |
| **Total** | **71** | **803** |

| Program | Tests |
|---|---:|
| `test_address` | 36 |
| `test_analog_reader` | 11 |
| `test_app` | 34 |
| `test_app_cal` | 15 |
| `test_app_controls` | 14 |
| `test_app_crash` | 13 |
| `test_app_env` | 6 |
| `test_app_midi` | 11 |
| `test_app_midi_in` | 6 |
| `test_app_profile` | 16 |
| `test_app_status` | 14 |
| `test_bank` | 15 |
| `test_blink` | 12 |
| `test_breadcrumb` | 2 |
| `test_cal` | 16 |
| `test_cal_record` | 9 |
| `test_clock` | 30 |
| `test_clock_input` | 7 |
| `test_clock_output` | 3 |
| `test_crc8` | 5 |
| `test_cv_output` | 9 |
| `test_debounce` | 8 |
| `test_eeprom` | 15 |
| `test_engine_cartesian` | 7 |
| `test_engine_chance` | 9 |
| `test_engine_euclid` | 7 |
| `test_engine_klee` | 15 |
| `test_engine_plain` | 8 |
| `test_engine_stage` | 13 |
| `test_env` | 17 |
| `test_errlog` | 16 |
| `test_gate` | 10 |
| `test_gate_output` | 3 |
| `test_init` | 4 |
| `test_level_output` | 5 |
| `test_lfo` | 12 |
| `test_main` | 3 |
| `test_midi_in` | 11 |
| `test_midi_input` | 10 |
| `test_midi_out` | 21 |
| `test_midi_out_notes` | 22 |
| `test_midi_output` | 15 |
| `test_mod` | 14 |
| `test_note_map` | 12 |
| `test_null_logger` | 3 |
| `test_null_midi_input` | 2 |
| `test_null_midi_output` | 4 |
| `test_panel` | 8 |
| `test_pickup` | 9 |
| `test_pitch_cal` | 9 |
| `test_prng` | 4 |
| `test_profile` | 13 |
| `test_profile_record` | 10 |
| `test_register_init` | 7 |
| `test_reset_cause` | 4 |
| `test_reset_input` | 7 |
| `test_run_input` | 3 |
| `test_seq` | 27 |
| `test_seq_engines` | 11 |
| `test_seq_gate` | 15 |
| `test_seq_midi` | 5 |
| `test_seq_mod` | 12 |
| `test_seq_pulses` | 8 |
| `test_seq_transport` | 19 |
| `test_serial_logger` | 23 |
| `test_shift_reg_reader` | 10 |
| `test_spi` | 9 |
| `test_status_output` | 3 |
| `test_timebase` | 9 |
| `test_transport` | 24 |
| `test_watchdog` | 4 |

## Coverage

Firmware total: lines 2267/2267 (100.0%), branches 991/991 (100.0%), in `core/`, `app/` and `adapters/target/`.

## Emulated board scenarios

| File | Scenarios |
|---|---:|
| `calibration.test.js` | 30 |
| `debug.test.js` | 10 |
| `engines.test.js` | 41 |
| `midi.test.js` | 10 |
| `midi_clock.test.js` | 7 |
| `midi_in.test.js` | 9 |
| `modulation.test.js` | 30 |
| `profiles.test.js` | 21 |
| `release.test.js` | 15 |
| `status_light.test.js` | 18 |
| `transport.test.js` | 48 |
| `watchdog.test.js` | 21 |
| **Total** | **260** |
