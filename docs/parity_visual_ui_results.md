# L4/L5 aggregate evidence

Local verification: 2026-10-29. Counts/categories only; private artifacts stay in
`work/parity/`. These controls establish tooling behavior and independent engine
repeatability. They do not establish original-game parity.

| Coverage | Controls / attempts | Pass | Fail | Skip | Evidence category |
| --- | ---: | ---: | ---: | ---: | --- |
| L4 comparator and raster/alignment validation | 6 | 6 | 0 | 0 | synthetic controls |
| L5 state/command comparison | 1 | 1 | 0 | 0 | synthetic controls |
| L5 actual engine input trace, repeated run | 1 | 1 | 0 | 0 | independent implementation |
| Missing private resources and isolation failure handling | 2 | 2 | 0 | 0 | synthetic controls |
| Private clone static frame/trace probe | 1 | 1 | 0 | 0 | capture availability only |
| Private reference frame/trace probe | 1 | 0 | 1 | 0 | experiment_failed: namespace setup |
| Aligned reference/clone visual/UI scenarios | 0 | 0 | 0 | 0 | blocked_alignment_contract |

The ten synthetic counts are Python test methods, with multiple negative cases
inside some methods. Negative cases pass when the comparator rejects or detects
the injected defect. Reference failure occurred before the runtime launched.
The clone probe is unpaired. There are no measured per-region game mismatch
rates, reference UI sequences or command differences to publish.

Prerequisites: task 42 shared runner; fixed-tick reference render/camera/UI
observer; independently extracted structural masks; audited cross-engine command
and object normalization. See [parity.md](parity.md) for contracts and limitations.
