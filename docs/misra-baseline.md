# MISRA baseline

cppcheck findings on the code as it stood before the ports-and-adapters refactor (commit `86ff54b`, all source under `src/`). Kept for comparison once the refactor is done. Nothing was fixed to produce this.

- Tool: Cppcheck 2.19.0 with its MISRA addon, run through `make misra`
- Rule categories: MISRA C:2023 headlines file (see `tools/misra/`)
- Regenerate a summary like this with `make misra 2>&1 | python tools/misra/summarize.py`

Every finding came from the MISRA addon; cppcheck's own checks reported nothing.

Total findings: 165

| Folder | Mandatory | Required | Advisory | Total |
|---|---:|---:|---:|---:|
| `src/app` | 3 | 78 | 19 | 100 |
| `src/common` | 2 | 22 | 21 | 45 |
| `src/main.c` | 0 | 0 | 1 | 1 |
| `src/mcu` | 0 | 1 | 18 | 19 |
| **All** | 5 | 101 | 59 | 165 |

| Rule | Category | Count |
|---|---|---:|
| 2.3 | Advisory | 1 |
| 2.4 | Advisory | 1 |
| 2.5 | Advisory | 6 |
| 8.2 | Required | 1 |
| 8.7 | Advisory | 2 |
| 10.1 | Required | 26 |
| 10.3 | Required | 4 |
| 10.4 | Required | 24 |
| 11.4 | Advisory | 14 |
| 11.5 | Advisory | 3 |
| 11.9 | Required | 5 |
| 12.1 | Advisory | 6 |
| 12.2 | Required | 1 |
| 13.3 | Advisory | 5 |
| 14.2 | Required | 1 |
| 14.4 | Required | 7 |
| 15.5 | Advisory | 15 |
| 15.6 | Required | 19 |
| 17.3 | Mandatory | 5 |
| 17.8 | Advisory | 4 |
| 18.4 | Advisory | 2 |
| 21.1 | Required | 10 |
| 21.2 | Required | 3 |
